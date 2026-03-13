
class IndexedMaterial : public PhysicsMaterial {
 public:
  IndexedMaterial(uint32_t idx, float friction, float restitution)
      : mIndex(idx), mFriction(friction), mRestitution(restitution) {}
  virtual const char *GetDebugName() const override { return "IndexedMaterial"; }
  uint32_t mIndex;
  float mFriction;
  float mRestitution;
};

static uint32_t GetHitMaterialIndex(const Body &body, const SubShapeID &sub_shape_id) {
  const PhysicsMaterial *mat = body.GetShape()->GetMaterial(sub_shape_id);
  if (!mat || mat == PhysicsMaterial::sDefault) return 0;
  return static_cast<const IndexedMaterial *>(mat)->mIndex;
}


