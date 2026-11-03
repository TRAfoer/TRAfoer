#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include<time.h>
#include<math.h>
#include<stdint.h>

#define TIGR_IMPLEMENTATION
#define OLIVEC_IMPLEMENTATION
#define SIGANIMA_IMPLEMENTATION
#include "tigr.h"
#include "olive.c"
#include "SigAnima.c"
#include "openPPMp6.c"

#define WIDTH 800
#define HIGHT 600
#define PI 3.1415926

#define PTX 15
#define PTY 15
#define PTZ 15
#define PTMUL 160
#define SOILD_BLOCK_HALF_UP "▀"//

uint64_t timeDt=0;
uint32_t *data;
#define RandomCol 0
void my_exit(void)
{
    system("clear");
    printf("Exit with Clear \n");

}
#define CLAMP(val,dw,up) val<dw?dw:(val>up?up:val)
#define INDEX(i, j) i *HIGHT + j

Olivec_Canvas canvas;
char     Keys[]={'D','F','J','K'};
uint32_t Cols[]={OLIVEC_RGBA(220,20,60,255),OLIVEC_RGBA(255,69,0,255),
                 OLIVEC_RGBA(240,230,140,255),OLIVEC_RGBA(250,250,210,255)};
SIGAnimation TrackAnimas[4];

int sigWidth=100;
float downMul=0.8;
float speedMul=600.0;

double CurTime=0;

struct Note
{
    float Beats;
    int index;
};

void InitTempNotes(struct Note *nts,int count){
    for(int i =0;i<count;++i){
        struct Note N;
        N.Beats=fmod(((float)rand())/8000+2,24);
        N.index=rand()%4;
        nts[i]= N;
    }
}
void DrawNotes(struct Note *nts,int count,float CurB){
    const int NtThickness=20;
    for(int i =0;i<count;++i){
        if(nts[i].Beats<CurB)continue;
        int posY=(HIGHT*downMul)-((nts[i].Beats-CurB)*speedMul)-(NtThickness/2);
        if(posY<0)continue;
        int posX=((nts[i].index)*sigWidth)+(WIDTH/2-2*sigWidth);
        uint32_t col=Cols[nts[i].index];
        olivec_rect(canvas,posX,posY,sigWidth,NtThickness,col);
}}
void DrawLiness(){
    float vals[4];
    for(int i=0;i<4;++i)vals[i]=(1.0-Evaluate(TrackAnimas[i],CurTime))*10;

    olivec_line(canvas,(WIDTH/2-2*sigWidth)-vals[0]        ,0,(WIDTH/2-2*sigWidth)-vals[0]        ,HIGHT-1,0xffffffff);
    olivec_line(canvas,(WIDTH/2-sigWidth  )+vals[0]-vals[1],0,(WIDTH/2-sigWidth  )+vals[0]-vals[1],HIGHT-1,0xffffffff);
    olivec_line(canvas,(WIDTH/2           )+vals[1]-vals[2],0,(WIDTH/2           )+vals[1]-vals[2],HIGHT-1,0xffffffff);//0
    olivec_line(canvas,(WIDTH/2+sigWidth  )+vals[2]-vals[3],0,(WIDTH/2+sigWidth  )+vals[2]-vals[3],HIGHT-1,0xffffffff);
    olivec_line(canvas,(WIDTH/2+2*sigWidth)+vals[3]        ,0,(WIDTH/2+2*sigWidth)+vals[3]        ,HIGHT-1,0xffffffff);
    
    olivec_line(canvas,0,HIGHT*downMul,WIDTH-1,HIGHT*downMul,0xffffffff);
}
void InputDetect(Tigr *screen){
    for(int i=0;i<4;++i){
        if(tigrKeyDown(screen,Keys[i])){
            TrackAnimas[i].startTime=CurTime;
            printf("%c was down \n",Keys[i]);
        }
    }
}
float BPM=120;
signed main()
{
    float TpB=60.0/BPM;

    data=malloc(256*256*sizeof(uint32_t));
    atexit (my_exit);
    srand((uint32_t)time(NULL));
    Tigr *screen = tigrWindow(WIDTH, HIGHT, "TEST-ONE", TIGR_FIXED);
    canvas=olivec_canvas((uint32_t*)(screen->pix), WIDTH, HIGHT, WIDTH);
    for(int i=0;i<4;++i){
        TrackAnimas[i].duration=-1;
        TrackAnimas[i].power=0.5;
        TrackAnimas[i].duration=0.2;
    }
    //图片读取部分
    int pw,ph;
    int err=openPPMp6("test.ppm",data,&pw,&ph);
    if(err)printf("ERR!");
    Olivec_Canvas image;
    image=olivec_canvas(data, pw, ph, pw);

    const int Notecount=128;
    struct Note index[Notecount];
    InitTempNotes(index,Notecount);

    system("clear");
    if(sizeof(Keys)/sizeof(char)<4){
        return 1;
    }
    
    while (!tigrClosed(screen)&&!tigrKeyHeld(screen,'Q'))
    {
        CurTime+=tigrTime();
        float Bts=CurTime/TpB;
        olivec_fill(canvas,0xFF181818);
        InputDetect(screen);
        {
        float triangle_angle = 0.5f*PI*CurTime;

        float ps[4][2];
        float uvs[4][2] = {
            {0, 0},
            {1, 0},
            {1, 1},
            {0, 1},
        };
        float len = WIDTH/16;
        for (size_t i = 0; i < 4; ++i) {
            ps[i][0] = WIDTH/16+10  + cosf(PI/2*i + triangle_angle)*len;
            ps[i][1] = HIGHT/16+10  + sinf(PI/2*i + triangle_angle)*len;
        }
        for (size_t i = 0; i < 2; ++i) {
            int i1 = (i*2 + 0)%4;
            int i2 = (i*2 + 1)%4;
            int i3 = (i*2 + 2)%4;
            olivec_triangle3uv_bilinear(
                canvas,
                ps[i1][0], ps[i1][1],
                ps[i2][0], ps[i2][1],
                ps[i3][0], ps[i3][1],
                uvs[i1][0], uvs[i1][1],
                uvs[i2][0], uvs[i2][1],
                uvs[i3][0], uvs[i3][1],
                1, 1, 1,
                image
            );
       }
        
        }
        
        DrawNotes(index,Notecount,Bts);
        DrawLiness();

        char str1[24];
        char str2[24];
        sprintf(str1,"%7.5lf %7.5lf",CurTime,Bts);
        sprintf(str2,"fps equ %7.5lf",1.0/tigrTime());
        olivec_text(canvas,str1,10,HIGHT-100,olivec_default_font,2,0xffFFBF00);
        olivec_text(canvas,str2,10,HIGHT-100+20,olivec_default_font,2,0xffFFBF00);


        tigrUpdate(screen);
    }
    tigrFree(screen);
    return 0;
}