#include "addon/napi/napi_registry.h"

namespace JOLT {

napi_value InitModule(napi_env env, napi_value exports)
{
    for (auto fn : ClassRegistry::List()) {
        fn(env, exports);
    }

    return exports;
}

}

NAPI_MODULE(NODE_GYP_MODULE_NAME, JOLT::InitModule)
