#include "capsule.h"
#include "../napi/napi_registry.h"

namespace JOLT {

  Capsule::Capsule(napi_env env) {
    _env = env;
    setType(BodyShapeType::Capsule);
  }

  void Capsule::setShape(const CapsuleShape &shape) {
    _capsuleShape = shape;
  }
  CapsuleShape Capsule::shape() const {
    return _capsuleShape;
  }
  CapsuleShape Capsule::shape() {
    return _capsuleShape;
  }

}

static JOLT::AutoRegister _auto_reg_world(JOLT::Capsule::Init);
