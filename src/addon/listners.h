#pragma once

#include "includes.h"
#include "jolt.h"

namespace JOLT {

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
