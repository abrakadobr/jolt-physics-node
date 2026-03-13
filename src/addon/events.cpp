#include "events.h"
#include "world.h"

namespace JOLT {

	// See: ContactListener
  JPH::ValidateResult EngineContactListener::OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult)
	{
		// Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
		return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	void EngineContactListener::OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings)
	{
		// cout << "A contact was added" << endl;
	}

	void EngineContactListener::OnContactPersisted(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings)
	{
		// cout << "A contact was persisted" << endl;
	}

	void EngineContactListener::OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair)
	{
		// cout << "A contact was removed" << endl;
	}

  void EngineContactListener::setWorld(World * world) {
    _world = world;
  }

  void EngineBodyActivationListener::setWorld(World * world) {
    _world = world;
  }

	void EngineBodyActivationListener::OnBodyActivated(const JPH::BodyID &inBodyID, uint64_t inBodyUserData)
	{
		// cout << "A body got activated" << endl;
    if (!_world) return;
    EventBodyActivation e;
    e.body = inBodyID;
    e.active = true;
    _world->emit<EventBodyActivation>("body-activation", e);
	}

	void EngineBodyActivationListener::OnBodyDeactivated(const JPH::BodyID &inBodyID, uint64_t inBodyUserData)
	{
    if (!_world) return;
    EventBodyActivation e;
    e.body = inBodyID;
    e.active = false;
    _world->emit<EventBodyActivation>("body-activation", e);
		// cout << "A body went to sleep" << endl;
	}



}
