#include "sphere.h"
#include "../napi/napi_registry.h"

namespace JOLT {

  Sphere::Sphere(napi_env env) {
    _env = env;
    setType(BodyShapeType::Sphere);
  }


  void Sphere::setShape(const SphereShape &shape) {
    _shape = shape;
  }
  SphereShape Sphere::shape() {
    const JPH::SphereShape * joltShape = static_cast<const JPH::SphereShape* >(_body->GetShape());
    SphereShape ret;
    getShape(ret);
    ret.radius = joltShape->GetRadius();
    return ret;
  }
  // SphereShape Sphere::shape() {
    // return _shape;
  // }


}



static JOLT::AutoRegister _auto_reg_world(JOLT::Sphere::Init);
