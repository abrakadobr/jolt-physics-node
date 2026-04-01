#include "triangle.h"
#include "../napi/napi_registry.h"

namespace JOLT {

  Triangle::Triangle(napi_env env) {
    _env = env;
    setType(BodyShapeType::Triangle);
  }


  void Triangle::setShape(const TriangleShape &shape) {
    _shape = shape;
  }
  TriangleShape Triangle::shape() const {
    return _shape;
  }
  TriangleShape Triangle::shape() {
    return _shape;
  }



}

static JOLT::AutoRegister _auto_reg_world(JOLT::Triangle::Init);
