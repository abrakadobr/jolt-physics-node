  class ActivationListenerImpl final : public BodyActivationListener {
   public:
    explicit ActivationListenerImpl(PhysicsWorld *owner) : mOwner(owner) {}

    void OnBodyActivated(const BodyID &inBodyID, uint64 inBodyUserData) override { mOwner->QueueBodyActivation(true, inBodyID, inBodyUserData); }

    void OnBodyDeactivated(const BodyID &inBodyID, uint64 inBodyUserData) override { mOwner->QueueBodyActivation(false, inBodyID, inBodyUserData); }

   private:
    PhysicsWorld *mOwner;
  };

  class ContactListenerImpl final : public ContactListener {
   public:
    explicit ContactListenerImpl(PhysicsWorld *owner) : mOwner(owner) {}

    void OnContactAdded(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override {
      ApplyMaterialSettings(inBody1, inBody2, inManifold, ioSettings);
      mOwner->QueueContactEvent(PendingEventType::ContactAdded, inBody1.GetID(), inBody2.GetID(), inManifold);
    }

    void OnContactPersisted(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override {
      ApplyMaterialSettings(inBody1, inBody2, inManifold, ioSettings);
      mOwner->QueueContactEvent(PendingEventType::ContactPersisted, inBody1.GetID(), inBody2.GetID(), inManifold);
    }

    void OnContactRemoved(const SubShapeIDPair &inSubShapePair) override {
      mOwner->QueueContactRemoved(inSubShapePair.GetBody1ID(), inSubShapePair.GetBody2ID());
    }

   private:
    static void ApplyMaterialSettings(const Body &b1, const Body &b2,
                                       const ContactManifold &m, ContactSettings &s) {
      const PhysicsMaterial *mat1 = b1.GetShape()->GetMaterial(m.mSubShapeID1);
      const PhysicsMaterial *mat2 = b2.GetShape()->GetMaterial(m.mSubShapeID2);
      bool has1 = mat1 && mat1 != PhysicsMaterial::sDefault;
      bool has2 = mat2 && mat2 != PhysicsMaterial::sDefault;
      if (!has1 && !has2) return;
      float f1 = has1 ? static_cast<const IndexedMaterial *>(mat1)->mFriction : b1.GetFriction();
      float f2 = has2 ? static_cast<const IndexedMaterial *>(mat2)->mFriction : b2.GetFriction();
      float r1 = has1 ? static_cast<const IndexedMaterial *>(mat1)->mRestitution : b1.GetRestitution();
      float r2 = has2 ? static_cast<const IndexedMaterial *>(mat2)->mRestitution : b2.GetRestitution();
      s.mCombinedFriction = sqrt(f1 * f2);
      s.mCombinedRestitution = max(r1, r2);
    }
    PhysicsWorld *mOwner;
  };


