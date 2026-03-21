#pragma once

#include <vector>
#include "layers.h"
#include "../napi/napi_base.h"

namespace JOLT {

  struct Layer {
    uint32_t                id;
    JPH::ObjectLayer        objectLayer = 0;
    JPH::BroadPhaseLayer    broadPhaseLayer;
    std::vector<uint32_t>   collides;
  };

class LayersManager: public NApiBase<LayersManager> {
  public:
    static uint32_t const InvalidLayerID = 0xffffffff;

    static constexpr const char* ClassName = "LayersManager";

    static std::vector<napi_property_descriptor> Methods() {
      return {
        METHOD(LayersManager,layersNumber),
        METHOD(LayersManager,layersIDs),
        METHOD(LayersManager,layerName),
        METHOD(LayersManager,addLayer)
      };
    };

    LayersManager(napi_env env);

    void init();

    uint32_t    addLayer(const std::string &code);
    ObjectLayerPairFilterImpl           iObjectLayerPairFilter;
    BPLayerInterfaceImpl                iBPLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl   iObjectVsBroadPhaseLayerFilter;

    uint  layersNumber() const;
    bool  addLayerCollision(std::string src, std::string dst, bool both = true);
    bool  removeLayerCollision(std::string src, std::string dst, bool both = true);

    std::vector<uint32_t> layersIDs();
    std::string layerName(uint32_t lid);
    uint32_t layerID(const std::string &name);
    Layer layerByID(uint32_t lid);
    Layer layerByName(const std::string &name);

    bool  objectLayerFilter(JPH::ObjectLayer o1, JPH::ObjectLayer o2);
    JPH::BroadPhaseLayer  object2broad(JPH::ObjectLayer l);
    bool  broadLayerFilter(JPH::ObjectLayer ol, JPH::BroadPhaseLayer bl);
  protected:
    napi_env                            _nenv = nullptr;
    std::map<std::string, Layer>        _layers;
    uint32_t                            _lastLayerId = -1;
};


}
