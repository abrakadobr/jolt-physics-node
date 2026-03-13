#pragma once

#include <node_api.h>
#include <string>
#include <tuple>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

#include "../defines.h"
// #include "jscallback.h"

namespace JOLT {

template<class T> struct JsConvert;

/* void */
template<> struct JsConvert<void> {
    static napi_value to(napi_env env);
};

/* bool */
template<> struct JsConvert<bool> {
  static bool from(napi_env env, napi_value v);
  static napi_value to(napi_env env, bool v);
};


/* int32 */
template<> struct JsConvert<int32_t> {
  static int32_t from(napi_env env, napi_value v);
  static napi_value to(napi_env env, int32_t v);
};


/* uint32_t */
template<> struct JsConvert<uint32_t> {
  static uint32_t from(napi_env env, napi_value v);
  static napi_value to(napi_env env, uint32_t v);
};


/* double */
template<> struct JsConvert<double> {
  static double from(napi_env env, napi_value v);
  static napi_value to(napi_env env, double v);
};

/* string */
template<> struct JsConvert<std::string> {
  static std::string from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const std::string& s);
};

/* WorldSettings */
template<>
struct JsConvert<WorldSettings> {
  static WorldSettings from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const WorldSettings &v);
};

/* vector */
template<class T>
struct JsConvert<std::vector<T> > {
  static std::vector<T> from(napi_env env, napi_value v) {
      bool isArray;
      napi_is_array(env, v, &isArray);
      if (!isArray) {
          napi_throw_error(env, nullptr, "Expected array");
          return {};
      }
      uint32_t len;
      napi_get_array_length(env, v, &len);
      std::vector<T> result;
      result.reserve(len);
      for (uint32_t i = 0; i < len; ++i) {
          napi_value item;
          napi_get_element(env, v, i, &item);
          result.push_back(JsConvert<T>::from(env, item));
      }
      return result;
  }

  static napi_value to(napi_env env, const std::vector<T>& v)
  {
      napi_value arr;
      napi_create_array_with_length(env, v.size(), &arr);
      for (size_t i = 0; i < v.size(); ++i) {
          napi_value val = JsConvert<T>::to(env, v[i]);
          napi_set_element(env, arr, i, val);
      }
      return arr;
  }
};

template<class T>
T* JsUnwrap(napi_env env, napi_value obj) {
    T* ptr = nullptr;
    napi_unwrap(env, obj, reinterpret_cast<void**>(&ptr));
    return ptr;
};


template<typename> struct MethodTraits;

template<class C, class R, class... Args>
struct MethodTraits<R (C::*)(Args...)>
{
    using Class = C;
    using Return = R;
    using ArgsTuple = std::tuple<Args...>;
};

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

template<class T>
class NApiBase {
public:
    static napi_ref constructor;

    static void Finalize(napi_env env, void* data, void*) {
        delete static_cast<T*>(data);
    }

    static napi_value New(napi_env env, napi_callback_info info) {
        napi_value thisArg;
        size_t argc = 8;
        napi_value argv[8];
        napi_get_cb_info(env, info, &argc, argv, &thisArg, nullptr);
        T* obj = T::Create(env, argc, argv);
        if (!obj)
            return nullptr;
        napi_wrap( env, thisArg, obj, Finalize, nullptr, nullptr);
        return thisArg;
    }

    static void StoreConstructor(napi_env env, napi_value cons) {
        napi_create_reference(env, cons, 1, &constructor);
    }

    static napi_value GetConstructor(napi_env env) {
        napi_value cons;
        napi_get_reference_value(env, constructor, &cons);
        return cons;
    }
};

template<class T>
napi_ref NApiBase<T>::constructor;

//#define METHOD(CLASS,NAME) MethodAdapter<CLASS,&CLASS::NAME>
#define METHOD(CLASS,NAME) \
{ #NAME, 0, (napi_callback)MethodAdapter<CLASS,&CLASS::NAME>, 0,0,0, napi_default, 0 }


class JsCallback {
public:
  JsCallback() = default;
  JsCallback(napi_env e, napi_value fn) : _env(e) {
    napi_create_reference(_env, fn, 1, &_ref);
  }

  ~JsCallback() {
    if (_ref) napi_delete_reference(_env, _ref);
  }

  // запрет копирования
  JsCallback(const JsCallback&) = delete;
  JsCallback& operator=(const JsCallback&) = delete;

  // разрешаем move
  JsCallback(JsCallback&& other) noexcept {
    _env = other._env;
    _ref = other._ref;
    other._ref = nullptr;
  }

  JsCallback& operator=(JsCallback&& other) noexcept {
    if (this != &other) {
      if (_ref) napi_delete_reference(_env, _ref);
      _env = other._env;
      _ref = other._ref;
      other._ref = nullptr;
    }
    return *this;
  }

  napi_value Get() {
    napi_value fn;
    napi_get_reference_value(_env, _ref, &fn);
    return fn;
  }

  template<class... Args>
  void call(Args&&... args) {
    napi_value fn = Get();
    napi_value global;
    napi_get_global(_env, &global);
    constexpr size_t N = sizeof...(Args);
    napi_value argv[N ? N : 1];
    size_t i = 0;
    ((argv[i++] = JsConvert<std::decay_t<Args>>::to(_env, args)), ...);
    napi_call_function(_env, global, fn, N, argv, nullptr);
  }

  /*
  template<class T>
  void call(T data) {
    napi_value fn = Get();
    napi_value global;
    napi_get_global(_env, &global);
    napi_value ndata = JsConvert<str::decay_t<T>>::to(_env, data);
    // size_t i = 0;
    // ((argv[i++] = JsConvert<std::decay_t<Args>>::to(_env, args)), ...);
    napi_call_function(_env, global, fn, 1, ndata, nullptr);
  }
  */
private:
  napi_env _env = nullptr;
  napi_ref _ref = nullptr;
};

typedef std::map<std::string, std::vector<JsCallback>> JsCallbacksMap;


template<>
struct JsConvert<JsCallback> {
    static JsCallback from(napi_env env, napi_value v) {
        napi_valuetype t;
        napi_typeof(env, v, &t);
        if (t != napi_function) {
            napi_throw_error(env, nullptr, "Expected function");
        }
        return JsCallback(env, v);
    }
};



}
