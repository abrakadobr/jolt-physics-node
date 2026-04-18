#include "listners.h"
#include "world.h"
#include "events.h"

namespace JOLT {

	// See: ContactListener
  JPH::ValidateResult EngineContactListener::OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult)
	{
		// Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
		return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	void EngineContactListener::OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings)
	{
    if (!_world) return;
    JPH::BodyID id1 = inBody1.GetID();
    JPH::BodyID id2 = inBody2.GetID();
    /*
    _world->emit("contact-added", EventBodyContact{ id1, id2 });
    BodyManager* bm = _world->bodiesManager();
    Body* b1 = bm->getBody(id1);
    Body* b2 = bm->getBody(id2);
    if (b1) b1->emit("contact-added", EventBodyContactSelf{ id2 });
    if (b2) b2->emit("contact-added", EventBodyContactSelf{ id1 });
    */
	}

	void EngineContactListener::OnContactPersisted(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings)
	{
    if (!_world) return;
    JPH::BodyID id1 = inBody1.GetID();
    JPH::BodyID id2 = inBody2.GetID();
    /*
    _world->emit("contact-persisted", EventBodyContact{ id1, id2 });
    BodyManager* bm = _world->bodiesManager();
    Body* b1 = bm->getBody(id1);
    Body* b2 = bm->getBody(id2);
    if (b1) b1->emit("contact-persisted", EventBodyContactSelf{ id2 });
    if (b2) b2->emit("contact-persisted", EventBodyContactSelf{ id1 });
    */
	}

	void EngineContactListener::OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair)
	{
    if (!_world) return;
    JPH::BodyID id1 = inSubShapePair.GetBody1ID();
    JPH::BodyID id2 = inSubShapePair.GetBody2ID();
    /*
    _world->emit("contact-removed", EventBodyContact{ id1, id2 });
    BodyManager* bm = _world->bodiesManager();
    Body* b1 = bm->getBody(id1);
    Body* b2 = bm->getBody(id2);
    if (b1) b1->emit("contact-removed", EventBodyContactSelf{ id2 });
    if (b2) b2->emit("contact-removed", EventBodyContactSelf{ id1 });
    */
	}

  void EngineContactListener::setWorld(World * world) {
    _world = world;
  }

  void EngineBodyActivationListener::setWorld(World * world) {
    _world = world;
  }

	void EngineBodyActivationListener::OnBodyActivated(const JPH::BodyID &inBodyID, uint64_t inBodyUserData)
	{
    if (!_world) return;
    _world->onBodyActivate(inBodyID, inBodyUserData);
    /*
    EventBodyActivation e{ inBodyID, true };
    _world->emit("body-activation", e);
    Body* b = _world->bodiesManager()->getBody(inBodyID);
    if (b) b->emit("body-activation", e);
    */
	}

	void EngineBodyActivationListener::OnBodyDeactivated(const JPH::BodyID &inBodyID, uint64_t inBodyUserData)
	{
    if (!_world) return;
    _world->onBodyDeactivate(inBodyID, inBodyUserData);
    /*
    EventBodyActivation e{ inBodyID, false };
    _world->emit("body-activation", e);
    Body* b = _world->bodiesManager()->getBody(inBodyID);
    if (b) b->emit("body-activation", e);
    */
	}


}
