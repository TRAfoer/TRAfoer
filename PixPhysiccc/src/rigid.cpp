#include "rigid.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <new>

namespace {
constexpr JPH::ObjectLayer static_layer = 0;
constexpr JPH::ObjectLayer moving_layer = 1;
constexpr unsigned max_bodies = 8192;

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
    {
        return JPH::BroadPhaseLayer(static_cast<JPH::BroadPhaseLayer::Type>(layer));
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
    {
        return layer.GetValue() == 0 ? "Static" : "Moving";
    }
#endif
};

class ObjectPairs final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override
    {
        return first == moving_layer || second == moving_layer;
    }
};

class BroadPhasePairs final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer object, JPH::BroadPhaseLayer broad) const override
    {
        return object == moving_layer || broad.GetValue() == moving_layer;
    }
};

std::mutex lifecycle_mutex;
unsigned world_count = 0;

void trace(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
    std::fputc('\n', stderr);
}

bool finite(RgVec3 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

JPH::Vec3 vector(RgVec3 v) { return JPH::Vec3(v.x, v.y, v.z); }
RgVec3 vector(JPH::Vec3Arg v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
} // namespace

struct RgWorld {
    BroadPhaseLayers layers;
    ObjectPairs object_pairs;
    BroadPhasePairs broad_pairs;
    JPH::TempAllocatorImpl temporary{32 * 1024 * 1024};
    JPH::JobSystemSingleThreaded jobs{JPH::cMaxPhysicsJobs};
    JPH::PhysicsSystem physics;
    std::array<RgBody, max_bodies> live_bodies;
    JPH::BodyID floor;

    RgWorld()
    {
        live_bodies.fill(RG_INVALID_BODY);
        physics.Init(max_bodies, 0, 65536, 32768, layers, broad_pairs, object_pairs);
        physics.SetGravity(JPH::Vec3(0, -9.81f, 0));
    }

    ~RgWorld()
    {
        auto &bodies = physics.GetBodyInterface();
        for (RgBody handle : live_bodies) {
            if (handle != RG_INVALID_BODY) {
                JPH::BodyID id(handle);
                bodies.RemoveBody(id);
                bodies.DestroyBody(id);
            }
        }
        if (!floor.IsInvalid()) {
            bodies.RemoveBody(floor);
            bodies.DestroyBody(floor);
        }
    }
};

namespace {
bool valid(RgWorld *world, RgBody body)
{
    if (world == nullptr || body == RG_INVALID_BODY) return false;
    const unsigned index = body & JPH::BodyID::cMaxBodyIndex;
    return index < max_bodies && world->live_bodies[index] == body;
}

bool dynamic(RgWorld *world, RgBody body)
{
    return valid(world, body) && world->physics.GetBodyInterface().GetMotionType(JPH::BodyID(body))
                               == JPH::EMotionType::Dynamic;
}
} // namespace

RgWorld *rg_create(void)
{
    // All worlds share Jolt's registry. The last world releases it only after its
    // bodies and shapes are gone; later worlds can register it again safely.
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    if (world_count == 0) {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = trace;
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
    RgWorld *world = new (std::nothrow) RgWorld();
    if (world != nullptr) {
        ++world_count;
        JPH::RefConst<JPH::Shape> shape = new JPH::BoxShape(JPH::Vec3(256, 1, 256));
        JPH::BodyCreationSettings floor(shape, JPH::RVec3(0, -1, 0),
                                       JPH::Quat::sIdentity(), JPH::EMotionType::Static,
                                       static_layer);
        floor.mFriction = 0.7f;
        world->floor = world->physics.GetBodyInterface().CreateAndAddBody(
            floor, JPH::EActivation::DontActivate);
        if (!world->floor.IsInvalid()) return world;
        delete world;
        --world_count;
    }
    if (world_count == 0) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
    return nullptr;
}

void rg_destroy(RgWorld *world)
{
    if (world == nullptr) return;
    std::lock_guard<std::mutex> lock(lifecycle_mutex);
    delete world;
    if (--world_count == 0) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

RgBody rg_add_voxels(RgWorld *world, const RgVec3 *centers, int count,
                     float half_size, RgVec3 position, RgQuat rotation,
                     float mass, int fixed)
{
    if (world == nullptr || centers == nullptr || count <= 0 || count > 65536 ||
        !std::isfinite(half_size) || half_size <= 0 || !finite(position) ||
        !std::isfinite(mass) || (!fixed && mass <= 0)) return RG_INVALID_BODY;
    const float length_squared = rotation.x * rotation.x + rotation.y * rotation.y +
                                 rotation.z * rotation.z + rotation.w * rotation.w;
    if (!std::isfinite(length_squared) || length_squared < 1.0e-12f) return RG_INVALID_BODY;
    for (int i = 0; i < count; ++i) if (!finite(centers[i])) return RG_INVALID_BODY;

    JPH::RefConst<JPH::Shape> cube = new JPH::BoxShape(
        JPH::Vec3::sReplicate(half_size), std::min(0.02f, half_size * 0.1f));
    JPH::StaticCompoundShapeSettings compound;
    compound.mSubShapes.reserve(static_cast<JPH::uint>(count));
    for (int i = 0; i < count; ++i)
        compound.AddShape(vector(centers[i]), JPH::Quat::sIdentity(), cube);
    JPH::ShapeSettings::ShapeResult shape = compound.Create();
    if (shape.HasError()) return RG_INVALID_BODY;

    JPH::Quat orientation(rotation.x, rotation.y, rotation.z, rotation.w);
    orientation = orientation.Normalized();
    JPH::BodyCreationSettings settings(shape.Get(), vector(position), orientation,
        fixed ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
        fixed ? static_layer : moving_layer);
    settings.mFriction = 0.6f;
    settings.mRestitution = 0.05f;
    settings.mAllowSleeping = true;
    if (!fixed) {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass;
        settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
    }
    JPH::BodyID id = world->physics.GetBodyInterface().CreateAndAddBody(
        settings, fixed ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
    if (id.IsInvalid()) return RG_INVALID_BODY;
    const RgBody handle = id.GetIndexAndSequenceNumber();
    world->live_bodies[id.GetIndex()] = handle;
    return handle;
}

void rg_remove(RgWorld *world, RgBody body)
{
    if (!valid(world, body)) return;
    JPH::BodyID id(body);
    auto &bodies = world->physics.GetBodyInterface();
    bodies.RemoveBody(id);
    bodies.DestroyBody(id);
    world->live_bodies[id.GetIndex()] = RG_INVALID_BODY;
}

int rg_get(RgWorld *world, RgBody body, RgState *state)
{
    if (state == nullptr || !valid(world, body)) return 0;
    auto &bodies = world->physics.GetBodyInterface();
    const JPH::BodyID id(body);
    JPH::RVec3 position;
    JPH::Quat rotation;
    JPH::Vec3 linear, angular;
    bodies.GetPositionAndRotation(id, position, rotation);
    bodies.GetLinearAndAngularVelocity(id, linear, angular);
    state->position = vector(position);
    state->rotation = {rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW()};
    state->linear_velocity = vector(linear);
    state->angular_velocity = vector(angular);
    state->center_of_mass = vector(bodies.GetCenterOfMassPosition(id));
    return 1;
}

void rg_set_velocity(RgWorld *world, RgBody body, RgVec3 linear, RgVec3 angular)
{
    if (!dynamic(world, body) || !finite(linear) || !finite(angular)) return;
    world->physics.GetBodyInterface().SetLinearAndAngularVelocity(
        JPH::BodyID(body), vector(linear), vector(angular));
}

void rg_impulse_at(RgWorld *world, RgBody body, RgVec3 impulse, RgVec3 point)
{
    if (!dynamic(world, body) || !finite(impulse) || !finite(point)) return;
    world->physics.GetBodyInterface().AddImpulse(JPH::BodyID(body), vector(impulse), vector(point));
}

void rg_step(RgWorld *world, float dt)
{
    if (world == nullptr || !std::isfinite(dt) || dt <= 0) return;
    world->physics.Update(dt, 1, &world->temporary, &world->jobs);
}
