#pragma once

#include <vector>
#include "layers.h"
#include "../structs.h"

namespace JOLT {

class LayersManager {
  public:
    static uint32_t const InvalidLayerID = 0xffffffff;

    LayersManager();

    void                                init();
    uint32_t                            addLayer(const std::string &code);

    ObjectLayerPairFilterImpl           iObjectLayerPairFilter;
    BPLayerInterfaceImpl                iBPLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl   iObjectVsBroadPhaseLayerFilter;

    uint                                layersNumber() const;
    bool                                addLayerCollision(std::string src, std::string dst, bool both = true);
    bool                                removeLayerCollision(std::string src, std::string dst, bool both = true);

    std::vector<uint32_t>               layersIDs();
    std::string                         layerName(uint32_t lid);
    uint32_t                            layerID(const std::string &name);
    Layer                               layerByID(uint32_t lid);
    Layer                               layerByName(const std::string &name);

    bool                                objectLayerFilter(JPH::ObjectLayer o1, JPH::ObjectLayer o2);
    JPH::BroadPhaseLayer                object2broad(JPH::ObjectLayer l);
    bool                                broadLayerFilter(JPH::ObjectLayer ol, JPH::BroadPhaseLayer bl);
  protected:
    std::map<std::string, Layer>        _layers;
    uint32_t                            _lastLayerId = -1;
};


}
