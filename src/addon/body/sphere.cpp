#include "sphere.h"
#include "../napi/napi_registry.h"

namespace JOLT {

  Sphere::Sphere(napi_env env) {
    _env = env;
    setType(BodyShapeType::Sphere);
  }

}

static JOLT::AutoRegister _auto_reg_world(JOLT::Sphere::Init);
