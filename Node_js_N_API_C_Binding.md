```md
# Node.js N-API C++ Binding Framework

Минималистичный C++ framework для создания Node.js native addons через **N-API**.

Основные цели архитектуры:

- минимальный boilerplate
- compile-time генерация адаптеров методов
- универсальная система конвертации типов
- поддержка variadic аргументов
- автоматическая регистрация классов
- безопасная работа с JS callbacks

Framework позволяет почти напрямую проксировать C++ классы в JavaScript.

---

# Архитектура

Framework состоит из следующих компонентов:

```

JsConvert<T>        — конвертация типов JS ⇄ C++
MethodTraits        — извлечение сигнатуры метода
MethodAdapter       — универсальный адаптер методов
CallImpl            — вызов C++ метода
JsUnwrap            — извлечение C++ объекта из JS wrapper
ClassRegistry       — автоматическая регистрация классов
JsCallback          — безопасный RAII wrapper для JS callbacks

```

---

# Поток вызова

## JS → C++

```

JS call
↓
napi_callback (MethodAdapter)
↓
napi_get_cb_info
↓
JsConvert<T>::from
↓
C++ method

```

## C++ → JS

```

C++ code
↓
JsConvert<T>::to
↓
prepare argv
↓
napi_call_function

```

---

# Конвертация типов

Все типы проходят через шаблон:

```

JsConvert<T>

```

Он определяет:

```

from()  — JS → C++
to()    — C++ → JS

````

---

## Пример: int32

```cpp
template<>
struct JsConvert<int32_t> {

  static int32_t from(napi_env env, napi_value v)
  {
      int32_t x;
      napi_get_value_int32(env, v, &x);
      return x;
  }

  static napi_value to(napi_env env, int32_t v)
  {
      napi_value r;
      napi_create_int32(env, v, &r);
      return r;
  }

};
````

---

# Поддерживаемые типы

```
void
bool
int32_t
uint32_t
double
std::string
std::vector<T>
JsCallback
```

---

# Конвертация std::vector

JS Array ⇄ std::vector.

```cpp
template<class T>
struct JsConvert<std::vector<T>>
{
    static std::vector<T> from(napi_env env, napi_value v)
    {
        bool isArray;
        napi_is_array(env, v, &isArray);

        if (!isArray) {
            napi_throw_error(env,nullptr,"Expected array");
            return {};
        }

        uint32_t len;
        napi_get_array_length(env,v,&len);

        std::vector<T> result;
        result.reserve(len);

        for(uint32_t i=0;i<len;i++)
        {
            napi_value item;
            napi_get_element(env,v,i,&item);

            result.push_back(JsConvert<T>::from(env,item));
        }

        return result;
    }

    static napi_value to(napi_env env,const std::vector<T>& v)
    {
        napi_value arr;
        napi_create_array_with_length(env,v.size(),&arr);

        for(size_t i=0;i<v.size();i++)
        {
            napi_value val=JsConvert<T>::to(env,v[i]);
            napi_set_element(env,arr,i,val);
        }

        return arr;
    }
};
```

---

# Извлечение объекта

Каждый JS объект содержит pointer на C++ объект.

Используется `napi_wrap`.

Извлечение:

```cpp
template<class T>
T* JsUnwrap(napi_env env, napi_value obj)
{
    T* ptr=nullptr;
    napi_unwrap(env,obj,reinterpret_cast<void**>(&ptr));
    return ptr;
}
```

---

# MethodTraits

Извлекает сигнатуру метода.

```cpp
template<class C,class R,class... Args>
struct MethodTraits<R (C::*)(Args...)>
{
    using Class=C;
    using Return=R;
    using ArgsTuple=std::tuple<Args...>;
};
```

---

# MethodAdapter

Универсальный адаптер для всех методов.

Он:

1. получает аргументы из JS
2. конвертирует их
3. вызывает C++ метод
4. возвращает результат

```cpp
template<class C,auto Method>
napi_value MethodAdapter(napi_env env,napi_callback_info info)
{
    using Traits=MethodTraits<decltype(Method)>;

    using R=typename Traits::Return;
    using Tuple=typename Traits::ArgsTuple;

    constexpr size_t N=std::tuple_size_v<Tuple>;

    napi_value thisArg;
    napi_value argv[N ? N : 1];

    size_t argc=N;

    napi_get_cb_info(env,info,&argc,argv,&thisArg,nullptr);

    if(argc!=N)
    {
        napi_throw_error(env,nullptr,"Invalid argument count");
        return nullptr;
    }

    return CallImpl<C,Method,R,Tuple>(
        env,
        thisArg,
        argv,
        std::make_index_sequence<N>()
    );
}
```

---

# CallImpl

Compile-time разворачивание аргументов.

```cpp
template<class C,auto Method,class R,class Tuple,size_t... I>
napi_value CallImpl(
    napi_env env,
    napi_value thisArg,
    napi_value* argv,
    std::index_sequence<I...>)
{
    C* obj=JsUnwrap<C>(env,thisArg);

    if(!obj)
    {
        napi_throw_error(env,nullptr,"Invalid this object");
        return nullptr;
    }

    if constexpr(std::is_void_v<R>)
    {
        (obj->*Method)(
            JsConvert<std::tuple_element_t<I,Tuple>>::from(env,argv[I])...
        );

        return JsConvert<void>::to(env);
    }
    else
    {
        R result=(obj->*Method)(
            JsConvert<std::tuple_element_t<I,Tuple>>::from(env,argv[I])...
        );

        return JsConvert<R>::to(env,result);
    }
}
```

---

# Регистрация методов

Макрос:

```cpp
#define METHOD(CLASS,NAME) MethodAdapter<CLASS,&CLASS::NAME>
```

Использование:

```cpp
{ "add",0,METHOD(MyClass,add),0,0,0,napi_default,0 }
```

---

# Регистрация классов

Автоматическая регистрация через registry.

```cpp
class ClassRegistry
{
public:

    using InitFn=napi_value(*)(napi_env,napi_value);

    static std::vector<InitFn>& List()
    {
        static std::vector<InitFn> list;
        return list;
    }

    static void Add(InitFn fn)
    {
        List().push_back(fn);
    }
};
```

---

# AutoRegister

```cpp
class AutoRegister
{
public:
    AutoRegister(ClassRegistry::InitFn fn)
    {
        ClassRegistry::Add(fn);
    }
};
```

---

# Макрос регистрации

```cpp
#define REGISTER_CLASS(CLASS) \
static JOLT::AutoRegister _auto_reg_##CLASS(CLASS::Init);
```

---

# Точка входа addon

```cpp
napi_value InitModule(napi_env env,napi_value exports)
{
    for(auto fn : ClassRegistry::List())
    {
        fn(env,exports);
    }

    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, InitModule)
```

---

# JsCallback

RAII wrapper для `napi_ref`.

Позволяет безопасно хранить JS функции и вызывать их из C++.

Особенности:

* automatic `napi_create_reference`
* automatic `napi_delete_reference`
* **copy запрещён**
* **move разрешён** (безопасно для vector/map)

```cpp
class JsCallback {
public:

  JsCallback() = default;

  JsCallback(napi_env e, napi_value fn) : _env(e) {
    napi_create_reference(_env, fn, 1, &_ref);
  }

  ~JsCallback() {
    if (_ref)
        napi_delete_reference(_env, _ref);
  }

  JsCallback(const JsCallback&) = delete;
  JsCallback& operator=(const JsCallback&) = delete;

  JsCallback(JsCallback&& other) noexcept {
    _env = other._env;
    _ref = other._ref;
    other._ref = nullptr;
  }

  JsCallback& operator=(JsCallback&& other) noexcept {

    if (this != &other) {

      if (_ref)
          napi_delete_reference(_env, _ref);

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

private:

  napi_env _env = nullptr;
  napi_ref _ref = nullptr;
};
```

---

# Converter для JsCallback

```cpp
template<>
struct JsConvert<JsCallback>
{
    static JsCallback from(napi_env env, napi_value v)
    {
        napi_valuetype t;
        napi_typeof(env, v, &t);

        if (t != napi_function)
        {
            napi_throw_error(env, nullptr, "Expected function");
        }

        return JsCallback(env, v);
    }
};
```

---

# Использование callbacks

Пример EventEmitter.

```cpp
typedef std::map<std::string,std::vector<JsCallback>> JsCallbacksMap;
```

Регистрация:

```cpp
callbacks["event"].push_back(std::move(cb));
```

Вызов:

```cpp
for(auto& cb : callbacks["event"])
{
    cb.call(value);
}
```

---

# Compile-time обработка аргументов

Используется fold expression:

```cpp
((argv[i++] = JsConvert<std::decay_t<Args>>::to(_env,args)), ...);
```

Компилятор разворачивает:

```
argv[0] = ...
argv[1] = ...
argv[2] = ...
```

Без runtime циклов.

---

# Ограничения

```
callbacks можно вызывать только из main Node thread
для worker threads нужен napi_threadsafe_function
```

---

# Итог

Framework обеспечивает:

```
JS → C++ вызовы
C++ → JS вызовы
автоматическую конвертацию типов
compile-time адаптацию методов
автоматическую регистрацию классов
безопасную работу с JS callbacks
минимальный boilerplate
```

Подходит для биндинга C++ библиотек и движков в Node.js.

```
```

