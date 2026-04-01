#pragma once

#include <node_api.h>
#include <string>
#include <tuple>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

#include "../defines.h"
#include "../events.h"
#include "../body/body_shapes.h"

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

/* float */
template<> struct JsConvert<float> {
  static float from(napi_env env, napi_value v);
  static napi_value to(napi_env env, float v);
};

/* string */
template<> struct JsConvert<std::string> {
  static std::string from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const std::string& s);
};

/* BodyShapeType */
template<> struct JsConvert<BodyShapeType> {
  static BodyShapeType from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const BodyShapeType &v);
};

/* BodyMotionType */
template<> struct JsConvert<BodyMotionType> {
  static BodyMotionType from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const BodyMotionType &v);
};

/* WorldState */
template<> struct JsConvert<WorldState> {
  static WorldState from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const WorldState &v);
};

/* WorldSettings */
template<> struct JsConvert<WorldSettings> {
  static WorldSettings from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const WorldSettings &v);
};

/* EventBodyActivation */
template<> struct JsConvert<EventBodyActivation> {
  static napi_value to(napi_env env, const EventBodyActivation &v);
};

/* vector */
template<class T> struct JsConvert<std::vector<T> > {
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

/*  PhysicsMaterial */
template<> struct JsConvert<PhysicsMaterial> {
  static PhysicsMaterial from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const PhysicsMaterial &v);
};

/*  BoxShape */
template<> struct JsConvert<BoxShape> {
  static BoxShape from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const BoxShape &v);
};

/*  SphereShape */
template<> struct JsConvert<SphereShape> {
  static SphereShape from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const SphereShape &v);
};

/*  CapsuleShape */
template<> struct JsConvert<CapsuleShape> {
  static CapsuleShape from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const CapsuleShape &v);
};

/*  TriangleShape */
template<> struct JsConvert<TriangleShape> {
  static TriangleShape from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const TriangleShape &v);
};

/*  BodyCreationSettings */
template<> struct JsConvert<BodyCreationSettings> {
  static BodyCreationSettings from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const BodyCreationSettings &v);
};




// Forward declaration — full definition of JsConvert<T*>::to is in napi_base.h
// after NApiBase<T> is fully defined (avoids circular include).
template<class T> class NApiBase;

template<class T> struct JsConvert<T*> {
    static napi_value to(napi_env env, T* ptr);
};


/* vector<uint8_t> — maps to/from Node.js Buffer */
template<> struct JsConvert<std::vector<uint8_t>> {
  static std::vector<uint8_t> from(napi_env env, napi_value v);
  static napi_value to(napi_env env, const std::vector<uint8_t>& v);
};

/* array */
/*
template<class T> struct JsConvert<std::array<T> > {
  static std::array<T> from(napi_env env, napi_value v) {
      bool isArray;
      napi_is_array(env, v, &isArray);
      if (!isArray) {
          napi_throw_error(env, nullptr, "Expected array");
          return {};
      }
      uint32_t len;
      napi_get_array_length(env, v, &len);
      std::array<T> result;
      result.reserve(len);
      for (uint32_t i = 0; i < len; ++i) {
          napi_value item;
          napi_get_element(env, v, i, &item);
          result.push_back(JsConvert<T>::from(env, item));
      }
      return result;
  }

  static napi_value to(napi_env env, const std::array<T>& v)
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
*/

/*
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

  napi_value Get() const {
    napi_value fn;
    napi_get_reference_value(_env, _ref, &fn);
    return fn;
  }

  template<class... Args>
  void call(Args&&... args) const {
    napi_value fn = Get();
    napi_value global;
    napi_get_global(_env, &global);
    constexpr size_t N = sizeof...(Args);
    napi_value argv[N ? N : 1];
    size_t i = 0;
    ((argv[i++] = JsConvert<std::decay_t<Args>>::to(_env, args)), ...);
    napi_call_threadsafe_function(_env, global, fn, N, argv, nullptr);
  }

private:
  napi_env _env = nullptr;
  napi_ref _ref = nullptr;
};
*/



// =====================
// JsCallback
// =====================

class JsCallback {
public:
  JsCallback() = default;

  JsCallback(napi_env env, napi_value fn) {
    napi_value resource_name;
    napi_create_string_utf8(env, "JsCallback", NAPI_AUTO_LENGTH, &resource_name);

    napi_create_threadsafe_function(
      env,
      fn,
      nullptr,
      resource_name,
      0,                // unlimited queue
      1,                // thread count
      nullptr,
      nullptr,
      nullptr,
      &JsCallback::CallJs,
      &_tsfn
    );
  }

  ~JsCallback() {
    if (_tsfn) {
      napi_release_threadsafe_function(_tsfn, napi_tsfn_abort);
    }
  }

  // no copy
  JsCallback(const JsCallback&) = delete;
  JsCallback& operator=(const JsCallback&) = delete;

  // move
  JsCallback(JsCallback&& other) noexcept {
    _tsfn = other._tsfn;
    other._tsfn = nullptr;
  }

  JsCallback& operator=(JsCallback&& other) noexcept {
    if (this != &other) {
      if (_tsfn)
        napi_release_threadsafe_function(_tsfn, napi_tsfn_abort);

      _tsfn = other._tsfn;
      other._tsfn = nullptr;
    }
    return *this;
  }

  // =====================
  // call
  // =====================

  template<typename... Args>
  void call(Args&&... args) const {
    using Tuple = std::tuple<std::decay_t<Args>...>;

    auto* payload = new PayloadImpl<Tuple>(
      std::make_tuple(std::forward<Args>(args)...)
    );

    napi_call_threadsafe_function(
      _tsfn,
      payload,
      napi_tsfn_nonblocking
    );
  }

private:
  napi_threadsafe_function _tsfn = nullptr;

  // =====================
  // Type-erased payload
  // =====================

  struct PayloadBase {
    virtual ~PayloadBase() = default;
    virtual void invoke(napi_env env, napi_value js_cb) = 0;
  };

  template<typename Tuple>
  struct PayloadImpl : PayloadBase {
    Tuple args;

    explicit PayloadImpl(Tuple&& t) : args(std::move(t)) {}

    void invoke(napi_env env, napi_value js_cb) override {
      callWithTuple(env, js_cb, args,
        std::make_index_sequence<std::tuple_size_v<Tuple>>{}
      );
    }
  };

  // =====================
  // tuple unpack
  // =====================

  template<typename Tuple, size_t... I>
  static void callWithTuple(
    napi_env env,
    napi_value js_cb,
    Tuple& t,
    std::index_sequence<I...>
  ) {
    constexpr size_t N = sizeof...(I);
    napi_value argv[N ? N : 1];

    size_t i = 0;
    ((argv[i++] = JsConvert<std::tuple_element_t<I, Tuple>>::to(env, std::get<I>(t))), ...);

    napi_value global;
    napi_get_global(env, &global);

    napi_call_function(env, global, js_cb, N, argv, nullptr);
  }

  // =====================
  // TSFN callback
  // =====================

  static void CallJs(
    napi_env env,
    napi_value js_cb,
    void* /*context*/,
    void* data
  ) {
    auto* payload = static_cast<PayloadBase*>(data);
    payload->invoke(env, js_cb);
    delete payload;
  }
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
