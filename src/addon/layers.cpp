#include "layers.h"
#include "layers_manager.h"

namespace JOLT {

	bool ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const {
    return _manager->objectLayerFilter(inObject1, inObject2);
  }

  void ObjectLayerPairFilterImpl::setManager(LayersManager * manager) {
    _manager = manager;
  }

	uint BPLayerInterfaceImpl::GetNumBroadPhaseLayers() const {
    return _manager->layersNumber();
  }
	JPH::BroadPhaseLayer BPLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const {
    return _manager->object2broad(inLayer);
  }


  void BPLayerInterfaceImpl::setManager(LayersManager * manager) {
    _manager = manager;
  }

	bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const {
    return _manager->broadLayerFilter(inLayer1, inLayer2);
  }

  void ObjectVsBroadPhaseLayerFilterImpl::setManager(LayersManager * manager) {
    _manager = manager;
  }

}
