#pragma once

#include <node_api.h>
#include <vector>

namespace JOLT {

class ClassRegistry {
public:
    using InitFn = napi_value (*)(napi_env env, napi_value exports);
    static std::vector<InitFn>& List() {
        static std::vector<InitFn> list;
        return list;
    }
    static void Add(InitFn fn) {
        List().push_back(fn);
    }
};

class AutoRegister {
public:
    AutoRegister(ClassRegistry::InitFn fn) {
        ClassRegistry::Add(fn);
    }
};

// #define REGISTER_CLASS(CLASS) static JOLT::AutoRegister _auto_reg_##CLASS(CLASS::Init);

}
#define REGISTER_CLASS(CLASS) \
    static JOLT::AutoRegister _auto_reg_##__COUNTER__(CLASS::Init);

