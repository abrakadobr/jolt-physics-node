#include "box.h"
#include "../napi/napi_registry.h"

namespace JOLT {

  Box::Box(napi_env env) {
    _env = env;
    setType(BodyShapeType::Box);
  }

}

static JOLT::AutoRegister _auto_reg_world(JOLT::Box::Init);
