
struct QueryFilters {
  uint32_t layer_mask = 0xFFFFFFFFu;  // all layers by default
  std::vector<uint32_t> exclude_ids;
};

// Parse optional query filter object { layerMask?: uint32, excludeBodyIds?: uint32[] }
bool ParseQueryFilters(napi_env env, napi_value opts, QueryFilters &out) {
  napi_valuetype type = napi_undefined;
  if (napi_typeof(env, opts, &type) != napi_ok || type != napi_object) return true;

  napi_value lm_v;
  if (napi_get_named_property(env, opts, "layerMask", &lm_v) == napi_ok) {
    uint32_t mask = 0;
    if (GetUInt32Arg(env, lm_v, &mask)) out.layer_mask = mask;
  }

  napi_value exc_v;
  if (napi_get_named_property(env, opts, "excludeBodyIds", &exc_v) == napi_ok) {
    bool is_array = false;
    if (napi_is_array(env, exc_v, &is_array) == napi_ok && is_array) {
      uint32_t len = 0;
      napi_get_array_length(env, exc_v, &len);
      out.exclude_ids.reserve(len);
      for (uint32_t i = 0; i < len; ++i) {
        napi_value elem;
        if (napi_get_element(env, exc_v, i, &elem) == napi_ok) {
          uint32_t id = 0;
          if (GetUInt32Arg(env, elem, &id)) out.exclude_ids.push_back(id);
        }
      }
    }
  }
  return true;
}


