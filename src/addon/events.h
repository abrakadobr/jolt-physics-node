#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

namespace JOLT {


  // using JPH;
  /*
  enum class PendingEventType {
    BodyActivated,
    BodyDeactivated,
    ContactAdded,
    ContactPersisted,
    ContactRemoved
  };

  struct PendingEvent {
    PendingEventType type;
    uint32_t body_a = 0;
    uint32_t body_b = 0;
    uint64_t user_data = 0;
    RVec3 point = RVec3::sZero();
    Vec3 normal = Vec3::sZero();
    float penetration_depth = 0.0f;
  };

  struct DebugGeoResult {
    std::vector<float>    linePos, triPos;
    std::vector<uint32_t> lineCol, triCol;
  };
  */

  struct EventBodyActivation {
    JPH::BodyID   body;
    bool          active;
  };

  class World;

// An example contact listener
class EngineContactListener : public JPH::ContactListener
{
public:
	// See: ContactListener
	virtual JPH::ValidateResult	OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult) override;
	virtual void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override;
	virtual void OnContactPersisted(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override;
	virtual void OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override;

  void setWorld(World * world);
protected:
  World   * _world = nullptr;
};

// An example activation listener
class EngineBodyActivationListener : public JPH::BodyActivationListener
{
public:
	virtual void OnBodyActivated(const JPH::BodyID &inBodyID, uint64_t inBodyUserData) override;
	virtual void OnBodyDeactivated(const JPH::BodyID &inBodyID, uint64_t inBodyUserData) override;

  void setWorld(World * world);
protected:
  World   * _world = nullptr;
};


}
