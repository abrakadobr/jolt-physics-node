#pragma once

#include "./js_convert.h"
#include "./jolt_convert.h"

namespace JOLT {

template<class T>
T* JsUnwrap(napi_env env, napi_value obj) {
    T* ptr = nullptr;
    napi_unwrap(env, obj, reinterpret_cast<void**>(&ptr));
    return ptr;
};

template<typename> struct MethodTraits;

// regular method
template<class C, class R, class... Args>
struct MethodTraits<R (C::*)(Args...)>
{
    using Class = C;
    using Return = R;
    using ArgsTuple = std::tuple<Args...>;
};

// const method
template<class C, class R, class... Args>
struct MethodTraits<R (C::*)(Args...) const>
{
    using Class = C;
    using Return = R;
    using ArgsTuple = std::tuple<Args...>;
};

template< class C, auto Method, class R, class Tuple, size_t... I >
napi_value CallImpl( napi_env env, napi_value thisArg, napi_value* argv, std::index_sequence<I...>) {
    C* obj = JsUnwrap<C>(env, thisArg);
    if (!obj) {
      napi_throw_error(env, nullptr, "Invalid this object");
      return nullptr;
    }

    if constexpr(std::is_void_v<R>)
    {
        (obj->*Method)( JsConvert<std::decay_t<std::tuple_element_t<I,Tuple>>>::from(env, argv[I])...);
        return JsConvert<void>::to(env);
    } else {
        R result = (obj->*Method)( JsConvert<std::decay_t<std::tuple_element_t<I,Tuple>>>::from(env, argv[I])...);
        return JsConvert<R>::to(env, result);
    }
}

template<class C, auto Method>
napi_value MethodAdapter(napi_env env, napi_callback_info info) {
    using Traits = MethodTraits<decltype(Method)>;

    using R = typename Traits::Return;
    using Tuple = typename Traits::ArgsTuple;

    constexpr size_t N = std::tuple_size_v<Tuple>;

    napi_value thisArg;
    napi_value argv[N ? N : 1];

    size_t argc = N;
    napi_get_cb_info(env, info, &argc, argv, &thisArg, nullptr);

    if (argc != N) {
      napi_throw_error(env, nullptr, "Invalid argument count");
      return nullptr;
    }

    return CallImpl<C,Method,R,Tuple>(
        env,
        thisArg,
        argv,
        std::make_index_sequence<N>()
    );
};

#define METHOD(CLASS,NAME) { #NAME, 0, (napi_callback)MethodAdapter<CLASS,&CLASS::NAME>, 0,0,0, napi_default, 0 }

}
