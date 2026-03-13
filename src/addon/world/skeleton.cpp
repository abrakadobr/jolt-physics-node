
class Skeleton {


  bool AddSkeletonJoint(uint32_t skeleton_id, const std::string &name, int32_t parent_idx) {
    auto it = mSkeletons.find(skeleton_id);
    if (it == mSkeletons.end()) return false;
    if (parent_idx < 0) {
      it->second->AddJoint(name);
    } else {
      it->second->AddJoint(name, parent_idx);
    }
    return true;
  }

  bool FinalizeSkeleton(uint32_t skeleton_id) {
    auto it = mSkeletons.find(skeleton_id);
    if (it == mSkeletons.end()) return false;
    it->second->CalculateParentJointIndices();
    return it->second->AreJointsCorrectlyOrdered();
  }

  int GetSkeletonJointCount(uint32_t skeleton_id) const {
    auto it = mSkeletons.find(skeleton_id);
    if (it == mSkeletons.end()) return -1;
    return it->second->GetJointCount();
  }

  bool GetSkeletonJointInfo(uint32_t skeleton_id, int32_t joint_index, std::string &out_name, int32_t &out_parent_index) const {
    auto it = mSkeletons.find(skeleton_id);
    if (it == mSkeletons.end()) return false;
    if (joint_index < 0 || joint_index >= it->second->GetJointCount()) return false;
    const Skeleton::Joint &joint = it->second->GetJoint(joint_index);
    out_name = joint.mName.c_str();
    out_parent_index = joint.mParentJointIndex;
    return true;
  }

  int GetSkeletonJointIndex(uint32_t skeleton_id, const std::string &name) const {
    auto it = mSkeletons.find(skeleton_id);
    if (it == mSkeletons.end()) return -1;
    return it->second->GetJointIndex(name);
  }




}
