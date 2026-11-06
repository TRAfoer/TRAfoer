#include "simulation.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CELL_SIZE 0.16f
#define CELL_MASS 0.04f
#define FIXED_STEP (1.0f / 120.0f)
#define MAX_CRACK_SEEDS 24

typedef struct {
    RgBody body;
    RgVec3 reference;
    int count, anchored;
} Piece;

struct VsWorld {
    VxGraph graph;
    RgWorld *rigid;
    Piece pieces[VX_MAX_VOXELS];
    int owner[VX_MAX_VOXELS];
    int piece_count, load;
    unsigned blast_number;
    float accumulator;
};

static const RgVec3 zero = {0, 0, 0};
static const RgQuat identity = {0, 0, 0, 1};
static RgVec3 add(RgVec3 a, RgVec3 b) { return (RgVec3){a.x+b.x,a.y+b.y,a.z+b.z}; }
static RgVec3 sub(RgVec3 a, RgVec3 b) { return (RgVec3){a.x-b.x,a.y-b.y,a.z-b.z}; }
static RgVec3 mul(RgVec3 a, float s) { return (RgVec3){a.x*s,a.y*s,a.z*s}; }
static RgVec3 cross(RgVec3 a, RgVec3 b)
{
    return (RgVec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
static float length_squared(RgVec3 a) { return a.x*a.x+a.y*a.y+a.z*a.z; }
static RgVec3 rotate(RgQuat q, RgVec3 v)
{
    RgVec3 axis={q.x,q.y,q.z};
    RgVec3 t=mul(cross(axis,v),2);
    return add(v,add(mul(t,q.w),cross(axis,t)));
}
static RgVec3 lattice(const VxCell *cell)
{
    return (RgVec3){cell->x*CELL_SIZE,cell->y*CELL_SIZE,cell->z*CELL_SIZE};
}
static int valid_cell(const VsWorld *w, int c)
{
    return w && c>=0 && c<w->graph.cell_count && w->graph.cells[c].occupied &&
           w->owner[c]>=0 && w->owner[c]<w->piece_count;
}

/* A component can only split: its old owner is captured before graph labels
   change. Unchanged components retain their body, contact state and velocity.
   New centers inherit v_new = v_old + omega x (COM_new - COM_old). */
static int rebuild_bodies(VsWorld *w, RgVec3 initial_translation)
{
    const int old_count=w->piece_count;
    const int count=w->graph.component_count;
    Piece *next=calloc((size_t)count,sizeof(*next));
    RgState *states=calloc((size_t)(old_count ? old_count : 1),sizeof(*states));
    int *reused=calloc((size_t)(old_count ? old_count : 1),sizeof(*reused));
    if (!next || !states || !reused) { free(next); free(states); free(reused); return 0; }
    for (int p=0;p<old_count;++p) {
        if (!rg_get(w->rigid,w->pieces[p].body,&states[p])) {
            free(next); free(states); free(reused); return 0;
        }
    }
    for (int p=0;p<count;++p) {
        RgVec3 sum=zero;
        int first=-1;
        for (int c=0;c<w->graph.cell_count;++c) {
            if (!w->graph.cells[c].occupied || w->graph.cells[c].component!=p) continue;
            if (first<0) first=c;
            sum=add(sum,lattice(&w->graph.cells[c]));
            ++next[p].count;
        }
        if (first<0) continue;
        next[p].reference=mul(sum,1.0f/next[p].count);
        next[p].anchored=w->graph.component_anchored[p];
        int parent=old_count ? w->owner[first] : -1;
        if (parent>=0 && w->pieces[parent].count==next[p].count &&
            w->pieces[parent].anchored==next[p].anchored) {
            next[p]=w->pieces[parent];
            reused[parent]=1;
            continue;
        }
        RgVec3 local[VX_MAX_VOXELS];
        int n=0;
        for (int c=0;c<w->graph.cell_count;++c)
            if (w->graph.cells[c].occupied && w->graph.cells[c].component==p)
                local[n++]=sub(lattice(&w->graph.cells[c]),next[p].reference);
        RgQuat rotation=parent>=0 ? states[parent].rotation : identity;
        RgVec3 position=parent>=0
            ? add(states[parent].position,rotate(rotation,sub(next[p].reference,w->pieces[parent].reference)))
            : add(initial_translation,next[p].reference);
        next[p].body=rg_add_voxels(w->rigid,local,n,CELL_SIZE*0.5f,position,rotation,
                                    n*CELL_MASS,next[p].anchored);
        if (next[p].body==RG_INVALID_BODY) {
            for (int q=0;q<p;++q) {
                int was_reused=0;
                for (int r=0;r<old_count;++r)
                    if (reused[r] && next[q].body==w->pieces[r].body) was_reused=1;
                if (!was_reused) rg_remove(w->rigid,next[q].body);
            }
            free(next); free(states); free(reused); return 0;
        }
        if (parent>=0 && !next[p].anchored) {
            RgState child;
            rg_get(w->rigid,next[p].body,&child);
            RgVec3 velocity=add(states[parent].linear_velocity,
                cross(states[parent].angular_velocity,
                      sub(child.center_of_mass,states[parent].center_of_mass)));
            rg_set_velocity(w->rigid,next[p].body,velocity,states[parent].angular_velocity);
        }
    }
    for (int p=0;p<old_count;++p) if (!reused[p]) rg_remove(w->rigid,w->pieces[p].body);
    memcpy(w->pieces,next,(size_t)count*sizeof(*next));
    w->piece_count=count;
    for (int c=0;c<w->graph.cell_count;++c)
        w->owner[c]=w->graph.cells[c].occupied ? w->graph.cells[c].component : -1;
    free(next); free(states); free(reused);
    return 1;
}

VsWorld *vs_create(int scene)
{
    if (scene!=VS_CRATE && scene!=VS_BEAM) return NULL;
    VsWorld *w=calloc(1,sizeof(*w));
    if (!w) return NULL;
    w->rigid=rg_create();
    if (!w->rigid) { free(w); return NULL; }
    if (scene==VS_CRATE) vx_make_crate(&w->graph,16);
    else vx_make_beam(&w->graph,16,3,3);
    RgVec3 translation={-(w->graph.nx-1)*CELL_SIZE*0.5f,
        scene==VS_CRATE ? CELL_SIZE*0.5f : 2.0f,
        -(w->graph.nz-1)*CELL_SIZE*0.5f};
    if (!rebuild_bodies(w,translation)) { vs_destroy(w); return NULL; }
    return w;
}

void vs_destroy(VsWorld *w)
{
    if (!w) return;
    rg_destroy(w->rigid);
    free(w);
}

const VxGraph *vs_graph(const VsWorld *w) { return w ? &w->graph : NULL; }
float vs_cell_size(void) { return CELL_SIZE; }
int vs_piece_count(const VsWorld *w) { return w ? w->piece_count : 0; }
int vs_owner(const VsWorld *w,int c) { return valid_cell(w,c) ? w->owner[c] : -1; }
int vs_piece(const VsWorld *w,int p,VsPiece *out)
{
    if (!w || !out || p<0 || p>=w->piece_count) return 0;
    if (!rg_get(w->rigid,w->pieces[p].body,&out->motion)) return 0;
    out->reference_center=w->pieces[p].reference;
    out->voxel_count=w->pieces[p].count;
    out->anchored=w->pieces[p].anchored;
    return 1;
}

RgVec3 vs_cell_position(const VsWorld *w,int c)
{
    VsPiece piece;
    if (!valid_cell(w,c) || !vs_piece(w,w->owner[c],&piece)) return zero;
    return add(piece.motion.position,rotate(piece.motion.rotation,
               sub(lattice(&w->graph.cells[c]),piece.reference_center)));
}

void vs_step(VsWorld *w,float dt)
{
    if (!w || !isfinite(dt) || dt<=0) return;
    /* Avoid an unbounded catch-up after dragging or suspending the window. */
    if (dt>0.25f) dt=0.25f;
    w->accumulator+=dt;
    while (w->accumulator>=FIXED_STEP) {
        rg_step(w->rigid,FIXED_STEP);
        w->accumulator-=FIXED_STEP;
    }
}

static uint32_t mix(uint32_t n)
{
    n^=n>>16; n*=UINT32_C(0x7feb352d); n^=n>>15;
    n*=UINT32_C(0x846ca68b); return n^(n>>16);
}

/* Runtime game approximation: an impact seeds an irregular spatial Voronoi
   crack field on this body's current lattice. Only bonds crossing its regions
   fail; the cubic material stays intact. Seeds are selected at impact time by
   farthest-point sampling with jitter, not stored tile boundaries. Jolt then
   owns every fragment's translation, rotation and collisions. */
static int fracture(VsWorld *w,int parent,int hit)
{
    int members[VX_MAX_VOXELS],region[VX_MAX_VOXELS],seeds[MAX_CRACK_SEEDS];
    float nearest[VX_MAX_VOXELS];
    int n=0,broken=0;
    for (int c=0;c<w->graph.cell_count;++c) if (valid_cell(w,c) && w->owner[c]==parent)
        members[n++]=c;
    if (n<2) return 0;
    int seed_count=n>=250 ? MAX_CRACK_SEEDS : (n+19)/20;
    if (seed_count<2) seed_count=2;
    if (seed_count>n) seed_count=n;
    uint32_t key=mix((uint32_t)(hit+2)*UINT32_C(2654435761) + ++w->blast_number);
    seeds[0]=hit>=0 ? hit : members[key%(uint32_t)n];
    for (int i=0;i<n;++i) nearest[i]=1.0e30f;
    for (int s=0;s<seed_count;++s) {
        float best=-1;
        int farthest=members[0];
        RgVec3 point=lattice(&w->graph.cells[seeds[s]]);
        for (int i=0;i<n;++i) {
            float d=length_squared(sub(lattice(&w->graph.cells[members[i]]),point));
            if (d<nearest[i]) nearest[i]=d;
            float jitter=0.78f+(mix((uint32_t)members[i]^key)%1000)*0.00044f;
            if (nearest[i]*jitter>best) { best=nearest[i]*jitter; farthest=members[i]; }
        }
        if (s+1<seed_count) seeds[s+1]=farthest;
    }
    for (int i=0;i<n;++i) {
        int c=members[i];
        float closest=1.0e30f;
        region[c]=0;
        for (int s=0;s<seed_count;++s) {
            float d=length_squared(sub(lattice(&w->graph.cells[c]),lattice(&w->graph.cells[seeds[s]])));
            if (d<closest) { closest=d; region[c]=s; }
        }
    }
    for (int b=0;b<w->graph.bond_count;++b) {
        VxBond *bond=&w->graph.bonds[b];
        if (bond->damage>=bond->strength || w->owner[bond->a]!=parent || w->owner[bond->b]!=parent) continue;
        if (region[bond->a]!=region[bond->b]) { bond->damage=bond->strength; ++broken; }
    }
    return broken;
}

int vs_explode(VsWorld *w,int cell)
{
    if (!w || cell < -1 || (cell>=0 && !valid_cell(w,cell))) return 0;
    int parent=cell>=0 ? w->owner[cell] : 0;
    if (parent>=w->piece_count) return 0;
    VsPiece before;
    if (!vs_piece(w,parent,&before)) return 0;
    RgVec3 point=cell>=0 ? vs_cell_position(w,cell) : before.motion.center_of_mass;
    RgVec3 local_point=cell>=0 ? lattice(&w->graph.cells[cell]) : before.reference_center;
    int affected[VX_MAX_VOXELS];
    for (int c=0;c<w->graph.cell_count;++c) affected[c]=valid_cell(w,c) && w->owner[c]==parent;
    /* Radial bond weakening accumulates without deleting or pulverizing cells. */
    vx_blast(&w->graph,local_point.x/CELL_SIZE,local_point.y/CELL_SIZE,
             local_point.z/CELL_SIZE,28.0f,35);
    int broken=fracture(w,parent,cell);
    if (broken>0) {
        vx_rebuild_components(&w->graph);
        if (!rebuild_bodies(w,zero)) return 0;
    }
    /* Pressure is distributed at each surviving voxel center. The off-center
       forces also generate torque on irregular fragments through the backend. */
    for (int c=0;c<w->graph.cell_count;++c) if (affected[c]) {
        RgVec3 location=vs_cell_position(w,c);
        RgVec3 direction=sub(location,point);
        float distance=sqrtf(length_squared(direction));
        if (distance<0.01f) direction=(RgVec3){0,1,0};
        else direction=mul(direction,1.0f/distance);
        float speed=4.5f/(0.6f+0.35f*distance);
        RgVec3 impulse=mul(direction,CELL_MASS*speed);
        rg_impulse_at(w->rigid,w->pieces[w->owner[c]].body,impulse,location);
    }
    return broken;
}

int vs_set_load(VsWorld *w,int total_load)
{
    if (!w || w->graph.kind!=VX_KIND_BEAM) return 0;
    if (total_load<0) total_load=0;
    w->load=total_load;
    int broken=vx_settle_beam(&w->graph,total_load);
    if (broken>0 && !rebuild_bodies(w,zero)) return 0;
    return broken;
}
int vs_load(const VsWorld *w) { return w ? w->load : 0; }

int vs_impulse(VsWorld *w,int cell,RgVec3 impulse)
{
    if (!valid_cell(w,cell) || !isfinite(impulse.x) || !isfinite(impulse.y) || !isfinite(impulse.z) ||
        w->pieces[w->owner[cell]].anchored) return 0;
    rg_impulse_at(w->rigid,w->pieces[w->owner[cell]].body,impulse,vs_cell_position(w,cell));
    return 1;
}
