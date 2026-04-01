#include "box.h"
#include "../napi/napi_registry.h"

namespace JOLT {

  Box::Box(napi_env env) {
    _env = env;
    setType(BodyShapeType::Box);
  }

  void Box::setShape(const BoxShape &shape) {
    _boxShape = shape;
  }
  BoxShape Box::shape() {
    const JPH::BoxShape * joltShape = static_cast<const JPH::BoxShape* >(_body->GetShape());
    BoxShape ret;
    getShape(ret);
    ret.halfExtent = joltShape->GetHalfExtent();
    return ret;
  }
  // BoxShape Box::shape() {
    // return _boxShape;
  // }
}

static JOLT::AutoRegister _auto_reg_world(JOLT::Box::Init);
