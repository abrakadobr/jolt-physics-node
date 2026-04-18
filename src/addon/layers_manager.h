#pragma once

#include "includes.h"
// #include "layers.h"
#include "structs.h"
#include "jolt.h"

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



class LayersManager {
  public:
    static uint32_t const InvalidLayerID = 0xffffffff;

    LayersManager();

    ObjectLayerPairFilterImpl           iObjectLayerPairFilter;
    BPLayerInterfaceImpl                iBPLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl   iObjectVsBroadPhaseLayerFilter;

    void                                init();
    void                                addBLayer(const std::string &name);
    void                                addOLayer(const std::string &name, const std::string &bname, bool collides = false);

    size_t                              oLayersNumber() const;
    size_t                              bLayersNumber() const;
    void                                addOOLayerCollision(std::string src, std::string dst);
    void                                removeOOLayerCollision(std::string src, std::string dst);
    void                                addOBLayerCollision(std::string src, std::string dst);
    void                                removeOBLayerCollision(std::string src, std::string dst);

    BLayer                              bLayer(const std::string &name);
    OLayer                              oLayer(const std::string &name);
    BLayer                              bLayer(uint32_t id);
    OLayer                              oLayer(uint32_t id);

    JPH::ObjectLayer                    toObjectLayer(const std::string &name);
    JPH::BroadPhaseLayer                toBroadPhaseLayer(const std::string &name);

    // commands
    // void                                getLayers(CommandGetLayers cmd);
  protected:
    std::map<uint32_t, OLayer>          _oLayers;
    std::map<uint32_t, BLayer>           _bLayers;
    std::map<std::string, uint32_t>     _oNames;
    std::map<std::string, uint32_t>      _bNames;
    uint32_t                            _oLayerId = 0;
    uint32_t                             _bLayerId = 0;
};


}
