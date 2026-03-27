#include "capsule.h"
#include "../napi/napi_registry.h"

namespace JOLT {

  Capsule::Capsule(napi_env env) {
    _env = env;
    setType(BodyShapeType::Capsule);
  }

}

static JOLT::AutoRegister _auto_reg_world(JOLT::Capsule::Init);
