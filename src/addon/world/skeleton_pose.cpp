
class SkeletonPose {


  uint32_t CreateSkeletonPose(uint32_t ragdoll_id) {
    auto it = mRagdolls.find(ragdoll_id);
    if (it == mRagdolls.end()) return 0;
    auto pose = std::make_unique<SkeletonPose>();
    pose->SetSkeleton(it->second->GetRagdollSettings()->GetSkeleton());
    const uint32_t id = mNextSkeletonPoseId++;
    mSkeletonPoses[id] = std::move(pose);
    return id;
  }

  bool DestroySkeletonPose(uint32_t pose_id) {
    return mSkeletonPoses.erase(pose_id) > 0;
  }

  bool SetPoseJoint(uint32_t pose_id, int ji, float tx, float ty, float tz,
                    float rx, float ry, float rz, float rw) {
    auto it = mSkeletonPoses.find(pose_id);
    if (it == mSkeletonPoses.end()) return false;
    if (ji < 0 || ji >= (int)it->second->GetJointCount()) return false;
    auto &j = it->second->GetJoint(ji);
    j.mTranslation = Vec3(tx, ty, tz);
    j.mRotation = Quat(rx, ry, rz, rw);
    return true;
  }

  bool GetPoseJoint(uint32_t pose_id, int ji, Vec3 &out_t, Quat &out_r) const {
    auto it = mSkeletonPoses.find(pose_id);
    if (it == mSkeletonPoses.end()) return false;
    if (ji < 0 || ji >= (int)it->second->GetJointCount()) return false;
    const auto &j = it->second->GetJoint(ji);
    out_t = j.mTranslation;
    out_r = j.mRotation;
    return true;
  }

  bool SetPoseRootOffset(uint32_t pose_id, double x, double y, double z) {
    auto it = mSkeletonPoses.find(pose_id);
    if (it == mSkeletonPoses.end()) return false;
    it->second->SetRootOffset(RVec3(x, y, z));
    return true;
  }

  bool GetPoseRootOffset(uint32_t pose_id, RVec3 &out) const {
    auto it = mSkeletonPoses.find(pose_id);
    if (it == mSkeletonPoses.end()) return false;
    out = it->second->GetRootOffset();
    return true;
  }

  bool CalculatePoseJointMatrices(uint32_t pose_id) {
    auto it = mSkeletonPoses.find(pose_id);
    if (it == mSkeletonPoses.end()) return false;
    it->second->CalculateJointMatrices();
    return true;
  }

  int GetPoseJointCount(uint32_t pose_id) const {
    auto it = mSkeletonPoses.find(pose_id);
    if (it == mSkeletonPoses.end()) return -1;
    return (int)it->second->GetJointCount();
  }



}
