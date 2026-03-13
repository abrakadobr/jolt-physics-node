
class Ragdoll {


  uint32_t CreateRagdollSettings(uint32_t skeleton_id, float capsule_half_height, float capsule_radius, float spacing) {
    auto it = mSkeletons.find(skeleton_id);
    if (it == mSkeletons.end()) return 0;

    Ref<RagdollSettings> settings = new RagdollSettings();
    settings->mSkeleton = it->second;

    const int count = it->second->GetJointCount();
    settings->mParts.resize(count);

    for (int i = 0; i < count; ++i) {
      CapsuleShapeSettings shape_settings(capsule_half_height, capsule_radius);
      const ShapeSettings::ShapeResult shape_result = shape_settings.Create();
      if (shape_result.HasError()) return 0;

      auto &part = settings->mParts[i];
      part.SetShape(shape_result.Get());
      part.mPosition = RVec3(0.0, -spacing * static_cast<double>(i), 0.0);
      part.mRotation = Quat::sIdentity();
      part.mMotionType = EMotionType::Dynamic;
      part.mObjectLayer = Layers::MOVING;

      const int parent = it->second->GetJoint(i).mParentJointIndex;
      if (parent >= 0) {
        auto *fixed = new FixedConstraintSettings();
        fixed->mSpace = EConstraintSpace::WorldSpace;
        fixed->mAutoDetectPoint = true;
        part.mToParent = fixed;
      }
    }

    settings->CalculateConstraintPriorities();
    settings->CalculateBodyIndexToConstraintIndex();
    settings->CalculateConstraintIndexToBodyIdxPair();
    settings->DisableParentChildCollisions();

    const uint32_t id = mNextRagdollSettingsId++;
    mRagdollSettings[id] = settings;
    return id;
  }


  int GetRagdollBodyCount(uint32_t ragdoll_id) const {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return -1;
    return static_cast<int>(it->second->GetBodyCount());
  }

  bool GetRagdollBoneBodyId(uint32_t ragdoll_id, int index, uint32_t &out_body_id) const {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    if (index < 0 || index >= static_cast<int>(it->second->GetBodyCount())) return false;
    out_body_id = it->second->GetBodyID(index).GetIndexAndSequenceNumber();
    return true;
  }

  bool GetRagdollBoneTransform(uint32_t ragdoll_id, int index, RVec3 &out_pos, Quat &out_rot) const {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    if (index < 0 || index >= static_cast<int>(it->second->GetBodyCount())) return false;

    const BodyID body_id = it->second->GetBodyID(index);
    const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), body_id);
    if (!lock.Succeeded()) return false;

    out_pos = lock.GetBody().GetPosition();
    out_rot = lock.GetBody().GetRotation();
    return true;
  }

  bool SetRagdollBoneTransform(uint32_t ragdoll_id, int index, RVec3Arg pos, QuatArg rot, bool activate) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    if (index < 0 || index >= static_cast<int>(it->second->GetBodyCount())) return false;

    BodyInterface &bi = mPhysicsSystem.GetBodyInterface();
    bi.SetPositionAndRotation(
        it->second->GetBodyID(index),
        pos,
        rot,
        activate ? EActivation::Activate : EActivation::DontActivate);
    return true;
  }

  bool SetRagdollJointShape(uint32_t settings_id, int joint_index,
                             int kind, float half_h, float radius,
                             float hx, float hy, float hz) {
    auto it = mRagdollSettings.find(settings_id);
    if (it == mRagdollSettings.end()) return false;
    auto &parts = it->second->mParts;
    if (joint_index < 0 || joint_index >= static_cast<int>(parts.size())) return false;

    ShapeSettings::ShapeResult result;
    switch (kind) {
      case 0: { CapsuleShapeSettings s(half_h, radius); result = s.Create(); break; }
      case 1: { BoxShapeSettings s(Vec3(hx, hy, hz)); result = s.Create(); break; }
      case 2: { SphereShapeSettings s(radius); result = s.Create(); break; }
      default: return false;
    }
    if (result.HasError()) return false;
    parts[joint_index].SetShape(result.Get());
    return true;
  }

  bool SetRagdollJointTransform(uint32_t settings_id, int joint_index,
                                 RVec3Arg pos, QuatArg rot) {
    auto it = mRagdollSettings.find(settings_id);
    if (it == mRagdollSettings.end()) return false;
    auto &parts = it->second->mParts;
    if (joint_index < 0 || joint_index >= static_cast<int>(parts.size())) return false;
    parts[joint_index].mPosition = pos;
    parts[joint_index].mRotation = rot;
    return true;
  }

  bool SetRagdollJointConstraint(uint32_t settings_id, int joint_index, RagdollJointConstraintConfig cfg) {
    auto it = mRagdollSettings.find(settings_id);
    if (it == mRagdollSettings.end()) return false;
    auto &parts = it->second->mParts;
    if (joint_index <= 0 || joint_index >= static_cast<int>(parts.size())) return false;

    const RVec3 pivot = parts[joint_index].mPosition;

    if (cfg.autoAxes && cfg.type != 0) {
      const int parent_idx = it->second->mSkeleton->GetJoint(joint_index).mParentJointIndex;
      const Vec3 child_pos(static_cast<float>(pivot.GetX()), static_cast<float>(pivot.GetY()), static_cast<float>(pivot.GetZ()));
      const RVec3 parent_rpos = parts[parent_idx].mPosition;
      const Vec3 parent_pos(static_cast<float>(parent_rpos.GetX()), static_cast<float>(parent_rpos.GetY()), static_cast<float>(parent_rpos.GetZ()));
      Vec3 bone_dir = parent_pos - child_pos;
      const float bone_len = bone_dir.Length();
      bone_dir = (bone_len < 1.0e-6f) ? Vec3(0.0f, 1.0f, 0.0f) : bone_dir / bone_len;
      Vec3 perp = bone_dir.Cross(Vec3(1.0f, 0.0f, 0.0f));
      if (perp.LengthSq() < 1.0e-6f) perp = bone_dir.Cross(Vec3(0.0f, 0.0f, 1.0f));
      perp = perp.Normalized();

      if (cfg.type == 1) {
        cfg.twistAxis1 = cfg.twistAxis2 = bone_dir;
        cfg.planeAxis1 = cfg.planeAxis2 = perp;
      } else if (cfg.type == 2) {
        cfg.hingeAxis1 = cfg.hingeAxis2 = perp;
        cfg.normalAxis1 = cfg.normalAxis2 = bone_dir;
      } else if (cfg.type == 3) {
        cfg.coneAxis1 = cfg.coneAxis2 = bone_dir;
      }
    }

    switch (cfg.type) {
      case 0: {
        auto *s = new FixedConstraintSettings();
        s->mSpace = EConstraintSpace::WorldSpace;
        s->mAutoDetectPoint = true;
        parts[joint_index].mToParent = s;
        break;
      }
      case 1: {
        auto *s = new SwingTwistConstraintSettings();
        s->mSpace = EConstraintSpace::WorldSpace;
        s->mPosition1 = pivot;
        s->mPosition2 = pivot;
        s->mTwistAxis1 = cfg.twistAxis1;
        s->mTwistAxis2 = cfg.twistAxis2;
        s->mPlaneAxis1 = cfg.planeAxis1;
        s->mPlaneAxis2 = cfg.planeAxis2;
        s->mNormalHalfConeAngle = cfg.normalHalfCone;
        s->mPlaneHalfConeAngle = cfg.planeHalfCone;
        s->mTwistMinAngle = cfg.twistMin;
        s->mTwistMaxAngle = cfg.twistMax;
        parts[joint_index].mToParent = s;
        break;
      }
      case 2: {
        auto *s = new HingeConstraintSettings();
        s->mSpace = EConstraintSpace::WorldSpace;
        s->mPoint1 = pivot;
        s->mPoint2 = pivot;
        s->mHingeAxis1 = cfg.hingeAxis1;
        s->mHingeAxis2 = cfg.hingeAxis2;
        s->mNormalAxis1 = cfg.normalAxis1;
        s->mNormalAxis2 = cfg.normalAxis2;
        s->mLimitsMin = cfg.hingeMin;
        s->mLimitsMax = cfg.hingeMax;
        parts[joint_index].mToParent = s;
        break;
      }
      case 3: {
        auto *s = new ConeConstraintSettings();
        s->mSpace = EConstraintSpace::WorldSpace;
        s->mPoint1 = pivot;
        s->mPoint2 = pivot;
        s->mTwistAxis1 = cfg.coneAxis1;
        s->mTwistAxis2 = cfg.coneAxis2;
        s->mHalfConeAngle = cfg.halfConeAngle;
        parts[joint_index].mToParent = s;
        break;
      }
      default: return false;
    }
    return true;
  }

  bool GetRagdollConstraintIds(uint32_t ragdoll_id, std::vector<uint32_t> &out) const {
    auto it = mRagdollConstraintIds.find(ragdoll_id);
    if (it == mRagdollConstraintIds.end()) return false;
    out = it->second;
    return true;
  }


  bool RagdollSetPose(uint32_t ragdoll_id, uint32_t pose_id, bool lock_bodies) {
    auto ri = mRagdolls.find(ragdoll_id);
    if (ri == mRagdolls.end()) return false;
    auto pi = mSkeletonPoses.find(pose_id);
    if (pi == mSkeletonPoses.end()) return false;
    ri->second->SetPose(*pi->second, lock_bodies);
    return true;
  }

  bool RagdollGetPose(uint32_t ragdoll_id, uint32_t pose_id, bool lock_bodies) {
    auto ri = mRagdolls.find(ragdoll_id);
    if (ri == mRagdolls.end()) return false;
    auto pi = mSkeletonPoses.find(pose_id);
    if (pi == mSkeletonPoses.end()) return false;
    ri->second->GetPose(*pi->second, lock_bodies);
    return true;
  }

  bool RagdollDriveToPoseKinematics(uint32_t ragdoll_id, uint32_t pose_id, float dt, bool lock_bodies) {
    auto ri = mRagdolls.find(ragdoll_id);
    if (ri == mRagdolls.end()) return false;
    auto pi = mSkeletonPoses.find(pose_id);
    if (pi == mSkeletonPoses.end()) return false;
    ri->second->DriveToPoseUsingKinematics(*pi->second, dt, lock_bodies);
    return true;
  }

  bool RagdollDriveToPoseMotors(uint32_t ragdoll_id, uint32_t pose_id) {
    auto ri = mRagdolls.find(ragdoll_id);
    if (ri == mRagdolls.end()) return false;
    auto pi = mSkeletonPoses.find(pose_id);
    if (pi == mSkeletonPoses.end()) return false;
    ri->second->DriveToPoseUsingMotors(*pi->second);
    return true;
  }

  bool RagdollActivate(uint32_t ragdoll_id, bool lock_bodies) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->Activate(lock_bodies);
    return true;
  }

  int RagdollIsActive(uint32_t ragdoll_id) const {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return -1;
    return it->second->IsActive() ? 1 : 0;
  }

  bool RagdollGetRootTransform(uint32_t ragdoll_id, RVec3 &out_pos, Quat &out_rot) const {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->GetRootTransform(out_pos, out_rot);
    return true;
  }

  bool RagdollGetWorldSpaceBounds(uint32_t ragdoll_id, Vec3 &out_min, Vec3 &out_max) const {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    AABox box = it->second->GetWorldSpaceBounds();
    out_min = box.mMin;
    out_max = box.mMax;
    return true;
  }

  bool RagdollSetGroupID(uint32_t ragdoll_id, uint32_t group_id, bool lock_bodies) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->SetGroupID(static_cast<CollisionGroup::GroupID>(group_id), lock_bodies);
    return true;
  }

  bool RagdollResetWarmStart(uint32_t ragdoll_id) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->ResetWarmStart();
    return true;
  }

  bool RagdollSetLinearVelocity(uint32_t ragdoll_id, float vx, float vy, float vz, bool lock_bodies) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->SetLinearVelocity(Vec3(vx, vy, vz), lock_bodies);
    return true;
  }

  bool RagdollAddLinearVelocity(uint32_t ragdoll_id, float vx, float vy, float vz, bool lock_bodies) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->AddLinearVelocity(Vec3(vx, vy, vz), lock_bodies);
    return true;
  }

  bool RagdollSetLinearAndAngularVelocity(uint32_t ragdoll_id,
      float lvx, float lvy, float lvz, float avx, float avy, float avz, bool lock_bodies) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->SetLinearAndAngularVelocity(Vec3(lvx, lvy, lvz), Vec3(avx, avy, avz), lock_bodies);
    return true;
  }

  bool RagdollAddImpulse(uint32_t ragdoll_id, float ix, float iy, float iz, bool lock_bodies) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->AddImpulse(Vec3(ix, iy, iz), lock_bodies);
    return true;
  }

  bool RagdollAddToPhysicsSystem(uint32_t ragdoll_id, bool activate) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->AddToPhysicsSystem(activate ? EActivation::Activate : EActivation::DontActivate);
    return true;
  }

  bool RagdollRemoveFromPhysicsSystem(uint32_t ragdoll_id) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    it->second->RemoveFromPhysicsSystem();
    return true;
  }

  bool RagdollStabilize(uint32_t ragdoll_id) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return false;
    return const_cast<RagdollSettings *>(it->second->GetRagdollSettings())->Stabilize();
  }




}
