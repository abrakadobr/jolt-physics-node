#pragma once

#include <node_api.h>
#include "napi_method.h"

namespace JOLT {


template<class T>
class NApiBase
{
public:

    static napi_ref constructor;
    static T* _pending; // set by JsConvert<T*>::to to wrap existing pointer

    bool _jsOwned = false;

public:

    explicit NApiBase()
    {
        _jsOwned = true;
    }

    static napi_value GetConstructor(napi_env env)
    {
        napi_value cons;
        napi_get_reference_value(env, constructor, &cons);
        return cons;
    }

    static void Finalize(napi_env env, void* data, void* hint)
    {
        T* obj = static_cast<T*>(data);

        if (obj && obj->_jsOwned)
            delete obj;
    }

    static napi_value New(napi_env env, napi_callback_info info)
    {
        napi_value thisArg;
        size_t argc = 0;

        napi_get_cb_info(env, info, &argc, nullptr, &thisArg, nullptr);

        T* obj = (_pending != nullptr) ? _pending : new T(env);
        _pending = nullptr;

        napi_wrap(env, thisArg, obj, Finalize, nullptr, nullptr);

        return thisArg;
    }

    static napi_value Init(napi_env env, napi_value exports)
    {
        auto props = T::Methods();

        napi_value cons;

        napi_define_class(
            env,
            T::ClassName,
            NAPI_AUTO_LENGTH,
            New,
            nullptr,
            props.size(),
            props.data(),
            &cons
        );

        napi_create_reference(env, cons, 1, &constructor);

        napi_set_named_property(env, exports, T::ClassName, cons);

        return exports;
    }


};

template<class T> napi_ref NApiBase<T>::constructor = nullptr;
template<class T> T* NApiBase<T>::_pending = nullptr;

// Inline definition of JsConvert<T*>::to — declared in js_convert.h,
// defined here because it needs the full NApiBase<T> definition.
template<class T>
napi_value JsConvert<T*>::to(napi_env env, T* ptr) {
    static_assert(std::is_base_of<NApiBase<T>, T>::value, "T must inherit from NApiBase");
    ptr->_jsOwned = false;
    NApiBase<T>::_pending = ptr;  // New() will use this instead of new T(env)
    napi_value cons = NApiBase<T>::GetConstructor(env);
    napi_value obj;
    napi_new_instance(env, cons, 0, nullptr, &obj);
    NApiBase<T>::_pending = nullptr;  // safety clear if New() wasn't called
    return obj;
}

}
