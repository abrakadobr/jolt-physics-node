#include "layers_manager.h"
#include <algorithm>

namespace JOLT {

	bool ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const {
    OLayer ol1 = _manager->oLayer(static_cast<uint32_t>(inObject1));
    OLayer ol2 = _manager->oLayer(static_cast<uint32_t>(inObject2));
    return ol1.collides || ol2.collides;
    // auto it = std::find(ol1.oCollisions.begin(), ol1.oCollisions.end(), inObject2);
    // bool ret = it != ol1.oCollisions.end();
    // std::cout << "ObjPair [" << inObject1 << " vs " << inObject2 << "] = " << ret << std::endl;
    // return ret;
  }

  void ObjectLayerPairFilterImpl::setManager(LayersManager * manager) {
    _manager = manager;
  }

	uint BPLayerInterfaceImpl::GetNumBroadPhaseLayers() const {
    return _manager->bLayersNumber();
  }
	JPH::BroadPhaseLayer BPLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const {
    OLayer ol = _manager->oLayer(static_cast<uint32_t>(inLayer));
    BLayer bl = _manager->bLayer(ol.bLayerId);
    return bl.broadPhaseLayer;
  }


  void BPLayerInterfaceImpl::setManager(LayersManager * manager) {
    _manager = manager;
  }

	bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const {
    OLayer ol1 = _manager->oLayer(static_cast<uint32_t>(inLayer1));
    return ol1.collides;
    // auto it = std::find(ol1.bCollisions.begin(), ol1.bCollisions.end(), static_cast<uint32_t>(inLayer2.GetValue()));
    // bool ret = it != ol1.bCollisions.end();
    // std::cout << "ObjVSBroad [" << inLayer1 << " vs " << (uint32_t)inLayer2.GetValue() << "] = " << ret << std::endl;
    // return ret;
  }

  void ObjectVsBroadPhaseLayerFilterImpl::setManager(LayersManager * manager) {
    _manager = manager;
  }




  LayersManager::LayersManager() {
    _oLayerId = 0;
    _bLayerId = 0;
    iObjectLayerPairFilter.setManager(this);
    iBPLayerInterface.setManager(this);
    iObjectVsBroadPhaseLayerFilter.setManager(this);
    init();
  }

  void LayersManager::addBLayer(const std::string &name) {
    BLayer l;
    l.name = name;
    l.id = _bLayerId;
    l.broadPhaseLayer = JPH::BroadPhaseLayer(l.id);

    _bLayers[l.id] = l;
    _bNames[l.name] = l.id;
    _bLayerId++;
    // std::cout << "add B layer: " << l.id << "/" << l.name <<  " ?? " << _bLayerId << std::endl;
  }
  void LayersManager::addOLayer(const std::string &name, const std::string &bname, bool collides) {
    OLayer ol;
    ol.name = name;
    ol.collides = collides;
    ol.id = _oLayerId;
    ol.oCollisions.clear();
    ol.bCollisions.clear();
    BLayer bl = bLayer(bname);
    ol.bLayerId = bl.id;
    _oLayers[ol.id] = ol;
    _oNames[ol.name] = ol.id;
    _oLayerId++;
    // std::cout << "add O layer: " << ol.id << "/" << ol.name << " bid: " << bl.id << std::endl;
  }

  size_t LayersManager::oLayersNumber() const {
    return _oLayers.size();
  }
  size_t LayersManager::bLayersNumber() const {
    return _bLayers.size();
  }
  void LayersManager::addOOLayerCollision(std::string src, std::string dst) {
    OLayer ol1 = oLayer(src);
    OLayer ol2 = oLayer(dst);
    _oLayers[ol1.id].oCollisions.push_back(ol2.id);
    _oLayers[ol2.id].oCollisions.push_back(ol1.id);
    // std::cout << "collision O" << ol1.id << "/" << ol2.id << std::endl;
  }
  void LayersManager::removeOOLayerCollision(std::string src, std::string dst) {
    OLayer ol1 = oLayer(src);
    OLayer ol2 = oLayer(dst);
    auto p2in1 = std::find(ol1.oCollisions.begin(), ol1.oCollisions.end(), ol2.id);
    auto p1in2 = std::find(ol2.oCollisions.begin(), ol2.oCollisions.end(), ol1.id);
    if (p2in1 != ol1.oCollisions.end()) ol1.oCollisions.erase(p2in1);
    if (p1in2 != ol2.oCollisions.end()) ol2.oCollisions.erase(p1in2);
  }
  void LayersManager::addOBLayerCollision(std::string src, std::string dst) {
    OLayer ol = oLayer(src);
    BLayer bl = bLayer(dst);
    auto bino = std::find(ol.bCollisions.begin(), ol.bCollisions.end(), bl.id);
    if (bino != ol.bCollisions.end()) return;
    ol.bCollisions.push_back(bl.id);
  }
  void LayersManager::removeOBLayerCollision(std::string src, std::string dst) {
    OLayer ol = oLayer(src);
    BLayer bl = bLayer(dst);
    auto bino = std::find(ol.bCollisions.begin(), ol.bCollisions.end(), bl.id);
    if (bino == ol.bCollisions.end()) return;
    _oLayers[ol.id].bCollisions.erase(bino);
  }

  BLayer LayersManager::bLayer(const std::string &name) {
    uint32_t id = _bNames[name];
    return _bLayers[id];
  }
  OLayer LayersManager::oLayer(const std::string &name) {
    uint32_t id = _oNames[name];
    return _oLayers[id];
  }
  BLayer LayersManager::bLayer(uint32_t id) {
    return _bLayers[id];
  }
  OLayer LayersManager::oLayer(uint32_t id) {
    return _oLayers[id];
  }


  JPH::ObjectLayer LayersManager::toObjectLayer(const std::string &name) {
    OLayer l = oLayer(name);
    return l.objectLayer;
  }
  JPH::BroadPhaseLayer LayersManager::toBroadPhaseLayer(const std::string &name) {
    BLayer l = bLayer(name);
    return l.broadPhaseLayer;
  }

  /*
  void LayersManager::getLayers(CommandGetLayers cmd) {
    std::vector<std::string> olist;
    std::vector<std::string> blist;
    if (cmd.objectLayers) {
      for (auto const& [k, v]: _oLayers) {
        olist.push_back(v.name);
      }
    }
    if (cmd.broadPhaseLayers) {
      for (auto const& [k, v]: _bLayers) {
        blist.push_back(v.name);
      }
    }
    // LayersListEvent e = EGen::LayersList(cmd.commandId, olist, blist);
//    this->_world->emit(e);
  }
  */

  void LayersManager::init() {
    addBLayer("static");
    addOLayer("static", "static");
    addBLayer("dynamic");
    addOLayer("dynamic", "dynamic", true);
    addOOLayerCollision("dynamic", "static");
    addOOLayerCollision("dynamic", "dynamic");
    addOOLayerCollision("static", "dynamic");
    addOBLayerCollision("dynamic", "static");
    addOBLayerCollision("dynamic", "dynamic");
    addOBLayerCollision("static", "dynamic");
  }

  /*
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

  */

}
