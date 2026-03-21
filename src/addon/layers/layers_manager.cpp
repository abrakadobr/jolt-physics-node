#include "layers_manager.h"
#include "../napi/napi_registry.h"
#include <algorithm>

namespace JOLT {

  LayersManager::LayersManager(napi_env env): _nenv(env) {
    iObjectLayerPairFilter.setManager(this);
    iBPLayerInterface.setManager(this);
    iObjectVsBroadPhaseLayerFilter.setManager(this);
  }

  void LayersManager::init() {
    addLayer("static");
    addLayer("dynamic");
    addLayerCollision("dynamic", "static");
    addLayerCollision("dynamic", "dynamic");
  }

  uint32_t LayersManager::addLayer(const std::string &code) {
    _lastLayerId++;
    uint32_t id = _lastLayerId;
    Layer l;
    l.id = id;
    l.objectLayer = id;
    l.broadPhaseLayer = JPH::BroadPhaseLayer(id);
    _layers.insert({code, l});
    return id;
  }

  uint LayersManager::layersNumber() const {
    return _layers.size();
  }

  std::vector<uint32_t> LayersManager::layersIDs() {
    std::vector<uint32_t> ret;
    for(auto pair: _layers) {
      ret.push_back(pair.second.id);
    }
    return ret;
  }

  std::string LayersManager::layerName(uint32_t lid) {
    for(auto pair: _layers) {
      if (pair.second.id == lid) return pair.first;
    }
    return "";
  }

  uint32_t LayersManager::layerID(const std::string &name) {
    if (!_layers.count(name)) return LayersManager::InvalidLayerID;
    return _layers[name].id;
  }

  Layer LayersManager::layerByID(uint32_t lid) {
    std::string name = layerName(lid);
    if (name != "") return layerByName(name);
    Layer l;
    l.id = LayersManager::InvalidLayerID;
    l.objectLayer = l.id;
    l.broadPhaseLayer = JPH::BroadPhaseLayer(l.id);
    return l;
  }

  Layer LayersManager::layerByName(const std::string &name) {
    if (!_layers.count(name)) {
      Layer l;
      l.id = LayersManager::InvalidLayerID;
      return l;
    }
    return _layers[name];
  }


  bool LayersManager::addLayerCollision(std::string src, std::string dst, bool both) {
    if (!_layers.count(src) || !_layers.count(dst)) return false;
    Layer dstLayer = _layers[dst];
    if (std::count(_layers[src].collides.begin(), _layers[src].collides.end(), dstLayer.id) > 0) {
      if (both) {
        addLayerCollision(dst, src, false);
      }
      return true;

    }
    _layers[src].collides.push_back(dstLayer.id);
    if (both) {
      addLayerCollision(dst, src, false);
    }
    return true;
  }

  bool LayersManager::removeLayerCollision(std::string src, std::string dst, bool both) {
    if (!_layers.count(src) || !_layers.count(dst)) return false;
    Layer dstLayer = _layers[dst];
    if (std::count(_layers[src].collides.begin(), _layers[src].collides.end(), dstLayer.id) > 0) {
      std::vector<uint32_t> vec(_layers[src].collides);
      _layers[src].collides.clear();
      for( uint32_t el: vec) {
        if (el != dstLayer.id)
          _layers[src].collides.push_back(el);
      }
    }
    if (both) {
      removeLayerCollision(dst, src, false);
    }
    return true;
  }


  bool LayersManager::objectLayerFilter(JPH::ObjectLayer o1, JPH::ObjectLayer o2) {
    std::string s1 = "";
    std::string s2 = "";
    for(auto layer: _layers) {
      if (layer.second.objectLayer == o1) s1 = layer.first;
      if (layer.second.objectLayer == o2) s2 = layer.first;
    }
    if (s1 == "" || s2 == "") return false;
    return (std::count(_layers[s1].collides.begin(), _layers[s1].collides.end(), _layers[s2].id) > 0);
  }

  JPH::BroadPhaseLayer LayersManager::object2broad(JPH::ObjectLayer l) {
    for(auto layer: _layers) {
      if (layer.second.objectLayer == l) return layer.second.broadPhaseLayer;
    }
    return JPH::BroadPhaseLayer(-1);
  }

  bool LayersManager::broadLayerFilter(JPH::ObjectLayer ol, JPH::BroadPhaseLayer bl) {
    std::string s1 = "";
    std::string s2 = "";
    for(auto layer: _layers) {
      if (layer.second.objectLayer == ol) s1 = layer.first;
      if (layer.second.broadPhaseLayer == bl) s2 = layer.first;
    }
    if (s1 == "" || s2 == "") return false;
    return (std::count(_layers[s1].collides.begin(), _layers[s1].collides.end(), _layers[s2].id) > 0);
  }



}
static JOLT::AutoRegister _auto_reg_world(JOLT::LayersManager::Init);
