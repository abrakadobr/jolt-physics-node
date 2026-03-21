#pragma once

#include <map>
#include <string>
#include "../jolt.h"

namespace JOLT {

  class LayersManager;

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override;
  void setManager(LayersManager * manager);
protected:
  LayersManager     * _manager;
};

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
	virtual uint GetNumBroadPhaseLayers() const override;
	virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override;

  void setManager(LayersManager * manager);

protected:
  LayersManager     * _manager;
};


/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override;
  void setManager(LayersManager * manager);

protected:
  LayersManager     * _manager;
};

}
