#include "world.h"
#include "commands.h"
#include "events.h"
#include "js_convert.h"

JOLT::World world;

napi_value SendCommand(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  napi_value napi_cmd = argv[0];

  JOLT::JCommand cmd = JOLT::JsConvert::from(env, napi_cmd);
  world.enqueue(cmd);
  return nullptr;
}

napi_threadsafe_function g_tsfn;

void js_dispatch( napi_env env, napi_value js_cb, void* /*context*/, void* data) {
  std::queue<JOLT::JEvent> je = *(std::queue<JOLT::JEvent>*)data;
  if (je.empty()) return;
  napi_value arr;
  napi_create_array_with_length(env, je.size(), &arr);
  size_t i = 0;
  // shutdown flag. we should allow our thread to complete loop and send events
  // so we test events if there is shutdown, and run shutdown routine after events sent.
  bool shutdown = false;
  while(!je.empty()) {
    JOLT::JEvent jev = je.front();
    napi_value nevent = JOLT::JsConvert::to(env, jev, &shutdown);
    je.pop();
    napi_set_element(env, arr, i, nevent);
    i++;
  }
  napi_value recv;
  napi_get_undefined(env, &recv);
  napi_value * argv = &arr;
  napi_call_function(env, recv, js_cb, 1, argv, nullptr);
  delete (std::queue<JOLT::JEvent>*)data;
  if (shutdown) {
    std::thread * loop = world.loopThread();
    if (loop->joinable()) {
      loop->join();
    }
  }
}

napi_value OnEvent(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  napi_value js_cb = argv[0];
  napi_value resource_name;
  napi_create_string_utf8(env, "jolt_events", NAPI_AUTO_LENGTH, &resource_name);

  napi_create_threadsafe_function(
    env,
    js_cb,
    nullptr,
    resource_name,
    0,
    1,
    nullptr,
    nullptr,
    nullptr,
    js_dispatch,
    &g_tsfn
  );
  world.setTsfn(g_tsfn);
  return nullptr;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_value fn;
  napi_value fn2;

  napi_create_function(env, nullptr, 0, OnEvent, nullptr, &fn);
  napi_set_named_property(env, exports, "onEvent", fn);

  napi_create_function(env, nullptr, 0, SendCommand, nullptr, &fn2);
  napi_set_named_property(env, exports, "sendCommand", fn2);

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
