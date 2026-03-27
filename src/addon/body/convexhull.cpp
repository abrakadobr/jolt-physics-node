#include "convexhull.h"
#include "../napi/napi_registry.h"

namespace JOLT {

  ConvexHull::ConvexHull(napi_env env) {
    _env = env;
    setType(BodyShapeType::ConvexHull);
  }

}

static JOLT::AutoRegister _auto_reg_world(JOLT::ConvexHull::Init);
