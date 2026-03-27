#include "triangle.h"
#include "../napi/napi_registry.h"

namespace JOLT {

  Triangle::Triangle(napi_env env) {
    _env = env;
    setType(BodyShapeType::Triangle);
  }

}

static JOLT::AutoRegister _auto_reg_world(JOLT::Triangle::Init);
