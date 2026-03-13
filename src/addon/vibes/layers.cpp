
namespace Layers {
constexpr ObjectLayer NON_MOVING = 0;
constexpr ObjectLayer MOVING = 1;
constexpr ObjectLayer NUM_LAYERS = 2;
}  // namespace Layers

namespace BroadPhaseLayers {
constexpr BroadPhaseLayer NON_MOVING(0);
constexpr BroadPhaseLayer MOVING(1);
constexpr uint NUM_LAYERS = 2;
}  // namespace BroadPhaseLayers

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
 public:
  BPLayerInterfaceImpl() {
    mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
    mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
  }

  uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

  BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override { return mObjectToBroadPhase[inLayer]; }

 private:
  BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter {
 public:
  bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
    switch (inLayer1) {
      case Layers::NON_MOVING:
        return inLayer2 == BroadPhaseLayers::MOVING;
      case Layers::MOVING:
        return true;
      default:
        return false;
    }
  }
};

class ObjectLayerPairFilterImpl final : public ObjectLayerPairFilter {
 public:
  bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override {
    switch (inObject1) {
      case Layers::NON_MOVING:
        return inObject2 == Layers::MOVING;
      case Layers::MOVING:
        return true;
      default:
        return false;
    }
  }
};

// Filters object layers by bitmask: bit N = layer N is allowed.
class MaskObjectLayerFilter final : public ObjectLayerFilter {
 public:
  explicit MaskObjectLayerFilter(uint32_t mask) : mMask(mask) {}
  bool ShouldCollide(ObjectLayer inLayer) const override {
    return inLayer < 32 && (mMask & (1u << inLayer)) != 0;
  }

 private:
  uint32_t mMask;
};


