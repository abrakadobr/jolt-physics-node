std::mutex gInitMutex;
std::atomic<uint32_t> gWorldCount{0};
bool gJoltInitialized = false;

void InitJoltIfNeeded() {
  std::lock_guard<std::mutex> guard(gInitMutex);
  if (gJoltInitialized) return;
  RegisterDefaultAllocator();
  Factory::sInstance = new Factory();
  RegisterTypes();
  gJoltInitialized = true;
}

void ShutdownJoltIfNeeded() {
  std::lock_guard<std::mutex> guard(gInitMutex);
  if (!gJoltInitialized || gWorldCount.load() != 0) return;
  UnregisterTypes();
  delete Factory::sInstance;
  Factory::sInstance = nullptr;
  gJoltInitialized = false;
}

bool GetWorldHandle(napi_env env, napi_value value, WorldHandle **out) {
  void *data = nullptr;
  if (napi_get_value_external(env, value, &data) != napi_ok || data == nullptr) return false;
  auto *handle = static_cast<WorldHandle *>(data);
  if (handle->world == nullptr) {
    ThrowError(env, "World is already destroyed");
    return false;
  }
  *out = handle;
  return true;
}

void FinalizeWorld(napi_env env, void *finalize_data, void *finalize_hint) {
  (void)env;
  (void)finalize_hint;

  auto *handle = static_cast<WorldHandle *>(finalize_data);
  if (handle == nullptr) return;

  if (handle->world != nullptr) {
    delete handle->world;
    handle->world = nullptr;
    gWorldCount.fetch_sub(1);
    ShutdownJoltIfNeeded();
  }

  delete handle;
}

napi_value CreateWorld(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok) return nullptr;

  double gravity = 9.81;
  if (argc == 1 && !GetDoubleArg(env, args[0], &gravity)) {
    ThrowTypeError(env, "createWorld(gravity?): gravity must be a number");
    return nullptr;
  }

  InitJoltIfNeeded();

  auto *handle = new WorldHandle();
  handle->world = new PhysicsWorld(env, static_cast<float>(gravity));
  gWorldCount.fetch_add(1);

  napi_value external;
  if (napi_create_external(env, handle, FinalizeWorld, nullptr, &external) != napi_ok) {
    delete handle->world;
    delete handle;
    gWorldCount.fetch_sub(1);
    ShutdownJoltIfNeeded();
    ThrowError(env, "Failed to create world handle");
    return nullptr;
  }

  return external;
}

napi_value DestroyWorld(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok || argc < 1) {
    ThrowTypeError(env, "destroyWorld(world) expects 1 argument");
    return nullptr;
  }

  void *data = nullptr;
  if (napi_get_value_external(env, args[0], &data) != napi_ok || data == nullptr) {
    ThrowTypeError(env, "destroyWorld(world): world must be value returned by createWorld");
    return nullptr;
  }

  auto *handle = static_cast<WorldHandle *>(data);
  if (handle->world != nullptr) {
    delete handle->world;
    handle->world = nullptr;
    gWorldCount.fetch_sub(1);
    ShutdownJoltIfNeeded();
  }

  napi_value undefined;
  napi_get_undefined(env, &undefined);
  return undefined;
}

#define WORLD_FN_BEGIN(expected)                         \
  size_t argc = expected;                                \
  std::vector<napi_value> args(expected);               \
  if (napi_get_cb_info(env, info, &argc, args.data(), nullptr, nullptr) != napi_ok || argc < expected) { \
    ThrowTypeError(env, "Invalid arguments");          \
    return nullptr;                                      \
  }                                                      \
  WorldHandle *handle = nullptr;                         \
  if (!GetWorldHandle(env, args[0], &handle)) {         \
    ThrowTypeError(env, "world must be value returned by createWorld"); \
    return nullptr;                                      \
  }

// Like WORLD_FN_BEGIN but allows up to maxargs args (>= required are mandatory).
#define WORLD_FN_OPT(required, maxargs)                  \
  size_t argc = maxargs;                                 \
  std::vector<napi_value> args(maxargs);                \
  if (napi_get_cb_info(env, info, &argc, args.data(), nullptr, nullptr) != napi_ok || argc < required) { \
    ThrowTypeError(env, "Invalid arguments");          \
    return nullptr;                                      \
  }                                                      \
  WorldHandle *handle = nullptr;                         \
  if (!GetWorldHandle(env, args[0], &handle)) {         \
    ThrowTypeError(env, "world must be value returned by createWorld"); \
    return nullptr;                                      \
  }

napi_value StepWorld(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double dt = 0.0;
  if (!GetDoubleArg(env, args[1], &dt) || dt <= 0.0) {
    ThrowTypeError(env, "step(world, dt): dt must be positive number");
    return nullptr;
  }
  handle->world->Step(static_cast<float>(dt));
  handle->world->DispatchCallbacks();
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  return undefined;
}

napi_value SetGravity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double gravity = 0.0;
  if (!GetDoubleArg(env, args[1], &gravity)) {
    ThrowTypeError(env, "setGravity(world, gravity): gravity must be number");
    return nullptr;
  }
  handle->world->SetGravity(static_cast<float>(gravity));
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  return undefined;
}

napi_value SetBodyActivationCallback(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  if (!handle->world->SetBodyActivationCallback(args[1])) {
    ThrowTypeError(env, "setBodyActivationCallback: callback must be function, null or undefined");
    return nullptr;
  }
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  return undefined;
}

napi_value SetContactCallback(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  if (!handle->world->SetContactCallback(args[1])) {
    ThrowTypeError(env, "setContactCallback: callback must be function, null or undefined");
    return nullptr;
  }
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  return undefined;
}

napi_value CreateSphere(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(8)
  double radius, x, y, z, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[1], &radius) || radius <= 0 || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) ||
      !GetDoubleArg(env, args[4], &z) || !GetBoolArg(env, args[5], &dynamic) || !GetDoubleArg(env, args[6], &restitution) ||
      !GetDoubleArg(env, args[7], &friction)) {
    ThrowTypeError(env, "createSphere: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateSphere(static_cast<float>(radius), x, y, z, dynamic, static_cast<float>(restitution), static_cast<float>(friction));
  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create sphere");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateBox(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  double hx, hy, hz, x, y, z, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[1], &hx) || hx <= 0 || !GetDoubleArg(env, args[2], &hy) || hy <= 0 || !GetDoubleArg(env, args[3], &hz) || hz <= 0 ||
      !GetDoubleArg(env, args[4], &x) || !GetDoubleArg(env, args[5], &y) || !GetDoubleArg(env, args[6], &z) || !GetBoolArg(env, args[7], &dynamic) ||
      !GetDoubleArg(env, args[8], &restitution) || !GetDoubleArg(env, args[9], &friction)) {
    ThrowTypeError(env, "createBox: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateBox(
      static_cast<float>(hx),
      static_cast<float>(hy),
      static_cast<float>(hz),
      x,
      y,
      z,
      dynamic,
      static_cast<float>(restitution),
      static_cast<float>(friction));

  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create box");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateCapsule(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(9)
  double half_h, radius, x, y, z, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[1], &half_h) || half_h < 0 || !GetDoubleArg(env, args[2], &radius) || radius <= 0 ||
      !GetDoubleArg(env, args[3], &x) || !GetDoubleArg(env, args[4], &y) || !GetDoubleArg(env, args[5], &z) ||
      !GetBoolArg(env, args[6], &dynamic) || !GetDoubleArg(env, args[7], &restitution) || !GetDoubleArg(env, args[8], &friction)) {
    ThrowTypeError(env, "createCapsule: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateCapsule(
      static_cast<float>(half_h),
      static_cast<float>(radius),
      x,
      y,
      z,
      dynamic,
      static_cast<float>(restitution),
      static_cast<float>(friction));

  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create capsule");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateCylinder(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(9)
  double half_h, radius, x, y, z, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[1], &half_h) || half_h < 0 || !GetDoubleArg(env, args[2], &radius) || radius <= 0 ||
      !GetDoubleArg(env, args[3], &x) || !GetDoubleArg(env, args[4], &y) || !GetDoubleArg(env, args[5], &z) ||
      !GetBoolArg(env, args[6], &dynamic) || !GetDoubleArg(env, args[7], &restitution) || !GetDoubleArg(env, args[8], &friction)) {
    ThrowTypeError(env, "createCylinder: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateCylinder(
      static_cast<float>(half_h),
      static_cast<float>(radius),
      x,
      y,
      z,
      dynamic,
      static_cast<float>(restitution),
      static_cast<float>(friction));

  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create cylinder");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateTaperedCapsule(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  double half_h, top_r, bottom_r, x, y, z, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[1], &half_h) || half_h < 0 || !GetDoubleArg(env, args[2], &top_r) || top_r <= 0 ||
      !GetDoubleArg(env, args[3], &bottom_r) || bottom_r <= 0 || !GetDoubleArg(env, args[4], &x) || !GetDoubleArg(env, args[5], &y) ||
      !GetDoubleArg(env, args[6], &z) || !GetBoolArg(env, args[7], &dynamic) || !GetDoubleArg(env, args[8], &restitution) ||
      !GetDoubleArg(env, args[9], &friction)) {
    ThrowTypeError(env, "createTaperedCapsule: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateTaperedCapsule(
      static_cast<float>(half_h),
      static_cast<float>(top_r),
      static_cast<float>(bottom_r),
      x,
      y,
      z,
      dynamic,
      static_cast<float>(restitution),
      static_cast<float>(friction));

  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create tapered capsule");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateTaperedCylinder(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  double half_h, top_r, bottom_r, x, y, z, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[1], &half_h) || half_h < 0 || !GetDoubleArg(env, args[2], &top_r) || top_r <= 0 ||
      !GetDoubleArg(env, args[3], &bottom_r) || bottom_r <= 0 || !GetDoubleArg(env, args[4], &x) || !GetDoubleArg(env, args[5], &y) ||
      !GetDoubleArg(env, args[6], &z) || !GetBoolArg(env, args[7], &dynamic) || !GetDoubleArg(env, args[8], &restitution) ||
      !GetDoubleArg(env, args[9], &friction)) {
    ThrowTypeError(env, "createTaperedCylinder: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateTaperedCylinder(
      static_cast<float>(half_h),
      static_cast<float>(top_r),
      static_cast<float>(bottom_r),
      x,
      y,
      z,
      dynamic,
      static_cast<float>(restitution),
      static_cast<float>(friction));

  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create tapered cylinder");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateConvexHull(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(8)

  bool is_array = false;
  if (napi_is_array(env, args[1], &is_array) != napi_ok || !is_array) {
    ThrowTypeError(env, "createConvexHull: points must be number array [x,y,z,...]");
    return nullptr;
  }

  uint32_t len = 0;
  napi_get_array_length(env, args[1], &len);
  if (len < 12 || (len % 3) != 0) {
    ThrowTypeError(env, "createConvexHull: need at least 4 points");
    return nullptr;
  }

  std::vector<Vec3> points;
  points.reserve(len / 3);
  for (uint32_t i = 0; i < len; i += 3) {
    napi_value vx, vy, vz;
    double x, y, z;
    napi_get_element(env, args[1], i, &vx);
    napi_get_element(env, args[1], i + 1, &vy);
    napi_get_element(env, args[1], i + 2, &vz);
    if (!GetDoubleArg(env, vx, &x) || !GetDoubleArg(env, vy, &y) || !GetDoubleArg(env, vz, &z)) {
      ThrowTypeError(env, "createConvexHull: points must contain numbers");
      return nullptr;
    }
    points.emplace_back(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
  }

  double px, py, pz, restitution, friction;
  bool dynamic;
  if (!GetDoubleArg(env, args[2], &px) || !GetDoubleArg(env, args[3], &py) || !GetDoubleArg(env, args[4], &pz) ||
      !GetBoolArg(env, args[5], &dynamic) || !GetDoubleArg(env, args[6], &restitution) || !GetDoubleArg(env, args[7], &friction)) {
    ThrowTypeError(env, "createConvexHull: invalid body args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateConvexHull(points, px, py, pz, dynamic, static_cast<float>(restitution), static_cast<float>(friction));
  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create convex hull");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateMesh(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(7, 8)

  bool vertices_is_array = false;
  bool indices_is_array = false;
  napi_is_array(env, args[1], &vertices_is_array);
  napi_is_array(env, args[2], &indices_is_array);
  if (!vertices_is_array || !indices_is_array) {
    ThrowTypeError(env, "createMesh: vertices/indices must be arrays");
    return nullptr;
  }

  uint32_t vlen = 0;
  uint32_t ilen = 0;
  napi_get_array_length(env, args[1], &vlen);
  napi_get_array_length(env, args[2], &ilen);
  if (vlen < 9 || (vlen % 3) != 0 || ilen < 3 || (ilen % 3) != 0) {
    ThrowTypeError(env, "createMesh: invalid array lengths");
    return nullptr;
  }

  std::vector<Float3> vertices;
  vertices.reserve(vlen / 3);
  for (uint32_t i = 0; i < vlen; i += 3) {
    napi_value vx, vy, vz;
    double x, y, z;
    napi_get_element(env, args[1], i, &vx);
    napi_get_element(env, args[1], i + 1, &vy);
    napi_get_element(env, args[1], i + 2, &vz);
    if (!GetDoubleArg(env, vx, &x) || !GetDoubleArg(env, vy, &y) || !GetDoubleArg(env, vz, &z)) {
      ThrowTypeError(env, "createMesh: vertices must contain numbers");
      return nullptr;
    }
    vertices.push_back(Float3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
  }

  // Parse optional 8th argument: opts { materialIndices, materials, restitution }
  std::vector<uint32_t> mat_index_per_tri;
  PhysicsMaterialList mat_list;
  double restitution = 0.0;

  if (argc > 7) {
    napi_value opts = args[7];
    napi_valuetype opts_type = napi_undefined;
    napi_typeof(env, opts, &opts_type);
    if (opts_type == napi_object) {
      // restitution override
      napi_value rv;
      if (napi_get_named_property(env, opts, "restitution", &rv) == napi_ok) GetDoubleArg(env, rv, &restitution);

      // materialIndices — one uint per triangle
      napi_value mi_v;
      if (napi_get_named_property(env, opts, "materialIndices", &mi_v) == napi_ok) {
        bool is_typed = false;
        bool is_arr = false;
        napi_is_typedarray(env, mi_v, &is_typed);
        napi_is_array(env, mi_v, &is_arr);
        uint32_t num_tris = ilen / 3;
        if (is_typed) {
          napi_typedarray_type ta_type;
          size_t byte_offset = 0, length = 0;
          void *data = nullptr;
          napi_value buf;
          napi_get_typedarray_info(env, mi_v, &ta_type, &length, &data, &buf, &byte_offset);
          mat_index_per_tri.resize(num_tris, 0);
          for (uint32_t k = 0; k < num_tris && k < static_cast<uint32_t>(length); ++k)
            mat_index_per_tri[k] = static_cast<uint32_t *>(data)[k];
        } else if (is_arr) {
          uint32_t mi_len = 0;
          napi_get_array_length(env, mi_v, &mi_len);
          mat_index_per_tri.resize(num_tris, 0);
          for (uint32_t k = 0; k < num_tris && k < mi_len; ++k) {
            napi_value elem;
            napi_get_element(env, mi_v, k, &elem);
            uint32_t idx = 0;
            GetUInt32Arg(env, elem, &idx);
            mat_index_per_tri[k] = idx;
          }
        }
      }

      // materials — array of { friction, restitution }
      napi_value mats_v;
      if (napi_get_named_property(env, opts, "materials", &mats_v) == napi_ok) {
        bool is_arr = false;
        napi_is_array(env, mats_v, &is_arr);
        if (is_arr) {
          uint32_t mats_len = 0;
          napi_get_array_length(env, mats_v, &mats_len);
          for (uint32_t k = 0; k < mats_len; ++k) {
            napi_value mobj;
            napi_get_element(env, mats_v, k, &mobj);
            double mf = 0.5, mr = 0.0;
            napi_value fv, rv2;
            if (napi_get_named_property(env, mobj, "friction", &fv) == napi_ok) GetDoubleArg(env, fv, &mf);
            if (napi_get_named_property(env, mobj, "restitution", &rv2) == napi_ok) GetDoubleArg(env, rv2, &mr);
            mat_list.push_back(new IndexedMaterial(k, static_cast<float>(mf), static_cast<float>(mr)));
          }
        }
      }

      // Auto-create material slots if indices given but no materials array
      if (!mat_index_per_tri.empty() && mat_list.empty()) {
        uint32_t max_idx = *std::max_element(mat_index_per_tri.begin(), mat_index_per_tri.end());
        for (uint32_t k = 0; k <= max_idx; ++k)
          mat_list.push_back(new IndexedMaterial(k, 0.5f, 0.0f));
      }
    }
  }

  std::vector<IndexedTriangle> tris;
  tris.reserve(ilen / 3);
  for (uint32_t i = 0; i < ilen; i += 3) {
    napi_value va, vb, vc;
    uint32_t a, b, c;
    napi_get_element(env, args[2], i, &va);
    napi_get_element(env, args[2], i + 1, &vb);
    napi_get_element(env, args[2], i + 2, &vc);
    if (!GetUInt32Arg(env, va, &a) || !GetUInt32Arg(env, vb, &b) || !GetUInt32Arg(env, vc, &c)) {
      ThrowTypeError(env, "createMesh: indices must contain uint numbers");
      return nullptr;
    }
    uint32_t tri_idx = i / 3;
    uint32_t mat_idx = (tri_idx < mat_index_per_tri.size()) ? mat_index_per_tri[tri_idx] : 0;
    tris.push_back(IndexedTriangle(a, b, c, mat_idx));
  }

  double x, y, z, friction;
  if (!GetDoubleArg(env, args[3], &x) || !GetDoubleArg(env, args[4], &y) || !GetDoubleArg(env, args[5], &z) ||
      !GetDoubleArg(env, args[6], &friction)) {
    ThrowTypeError(env, "createMesh: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateMesh(tris.empty() ? std::vector<Float3>() : vertices, tris, mat_list, x, y, z, static_cast<float>(friction), static_cast<float>(restitution));
  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create mesh");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value GetBodyPosition(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getBodyPosition: bodyId must be uint32");
    return nullptr;
  }
  RVec3 pos;
  if (!handle->world->GetBodyPosition(id, pos)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  return MakeVec3Object(env, pos);
}

napi_value GetBodyRotation(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getBodyRotation: bodyId must be uint32");
    return nullptr;
  }
  Quat rot;
  if (!handle->world->GetBodyRotation(id, rot)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  return MakeQuatObject(env, rot);
}

napi_value SetBodyPosition(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(6)
  uint32_t id;
  double x, y, z;
  bool activate;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z) ||
      !GetBoolArg(env, args[5], &activate)) {
    ThrowTypeError(env, "setBodyPosition: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetBodyPosition(id, x, y, z, activate), &out);
  return out;
}

napi_value SetBodyRotation(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(7)
  uint32_t id;
  double x, y, z, w;
  bool activate;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z) ||
      !GetDoubleArg(env, args[5], &w) || !GetBoolArg(env, args[6], &activate)) {
    ThrowTypeError(env, "setBodyRotation: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetBodyRotation(id, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w), activate), &out);
  return out;
}

napi_value GetLinearVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getLinearVelocity: bodyId must be uint32");
    return nullptr;
  }
  Vec3 v;
  if (!handle->world->GetLinearVelocity(id, v)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  return MakeVec3Object(env, RVec3(v.GetX(), v.GetY(), v.GetZ()));
}

napi_value SetLinearVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "setLinearVelocity: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetLinearVelocity(id, Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))), &out);
  return out;
}

napi_value GetAngularVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getAngularVelocity: bodyId must be uint32");
    return nullptr;
  }
  Vec3 v;
  if (!handle->world->GetAngularVelocity(id, v)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  return MakeVec3Object(env, RVec3(v.GetX(), v.GetY(), v.GetZ()));
}

napi_value SetAngularVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "setAngularVelocity: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetAngularVelocity(id, Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))), &out);
  return out;
}

napi_value ApplyImpulse(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "applyImpulse: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->ApplyImpulse(id, Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))), &out);
  return out;
}

napi_value AddForce(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "addForce: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->AddForce(id, Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))), &out);
  return out;
}

napi_value AddTorque(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "addTorque: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->AddTorque(id, Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))), &out);
  return out;
}

napi_value AddAngularImpulse(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "addAngularImpulse: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->AddAngularImpulse(id, Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))), &out);
  return out;
}

napi_value SetFrictionValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  double friction;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &friction)) {
    ThrowTypeError(env, "setFriction: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetFrictionValue(id, static_cast<float>(friction)), &out);
  return out;
}

napi_value GetFrictionValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getFriction: invalid bodyId");
    return nullptr;
  }
  float value = 0.0f;
  if (!handle->world->GetFrictionValue(id, value)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_double(env, value, &out);
  return out;
}

napi_value SetRestitutionValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  double restitution;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &restitution)) {
    ThrowTypeError(env, "setRestitution: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetRestitutionValue(id, static_cast<float>(restitution)), &out);
  return out;
}

napi_value GetRestitutionValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getRestitution: invalid bodyId");
    return nullptr;
  }
  float value = 0.0f;
  if (!handle->world->GetRestitutionValue(id, value)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_double(env, value, &out);
  return out;
}

napi_value SetGravityFactorValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  double factor;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &factor)) {
    ThrowTypeError(env, "setGravityFactor: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetGravityFactorValue(id, static_cast<float>(factor)), &out);
  return out;
}

napi_value GetGravityFactorValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getGravityFactor: invalid bodyId");
    return nullptr;
  }
  float value = 0.0f;
  if (!handle->world->GetGravityFactorValue(id, value)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_double(env, value, &out);
  return out;
}

napi_value SetMotionTypeValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id;
  int32_t motion_type;
  bool activate;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &motion_type) || !GetBoolArg(env, args[3], &activate)) {
    ThrowTypeError(env, "setMotionType: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetMotionTypeValue(id, motion_type, activate), &out);
  return out;
}

napi_value GetMotionTypeValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getMotionType: invalid bodyId");
    return nullptr;
  }
  int32_t value = 0;
  if (!handle->world->GetMotionTypeValue(id, value)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_int32(env, value, &out);
  return out;
}

napi_value SetMotionQualityValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  int32_t quality;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &quality)) {
    ThrowTypeError(env, "setMotionQuality: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetMotionQualityValue(id, quality), &out);
  return out;
}

napi_value GetMotionQualityValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getMotionQuality: invalid bodyId");
    return nullptr;
  }
  int32_t value = 0;
  if (!handle->world->GetMotionQualityValue(id, value)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_int32(env, value, &out);
  return out;
}

napi_value SetObjectLayerValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  uint32_t layer;
  if (!GetUInt32Arg(env, args[1], &id) || !GetUInt32Arg(env, args[2], &layer)) {
    ThrowTypeError(env, "setObjectLayer: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetObjectLayerValue(id, layer), &out);
  return out;
}

napi_value GetObjectLayerValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getObjectLayer: invalid bodyId");
    return nullptr;
  }
  uint32_t value = 0;
  if (!handle->world->GetObjectLayerValue(id, value)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, value, &out);
  return out;
}

napi_value SetDampingValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id;
  double linear_damping;
  double angular_damping;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &linear_damping) || !GetDoubleArg(env, args[3], &angular_damping)) {
    ThrowTypeError(env, "setDamping: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetDamping(id, static_cast<float>(linear_damping), static_cast<float>(angular_damping)), &out);
  return out;
}

napi_value GetDampingValue(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getDamping: invalid bodyId");
    return nullptr;
  }
  float linear = 0.0f;
  float angular = 0.0f;
  if (!handle->world->GetDamping(id, linear, angular)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_create_object(env, &out);
  napi_value lv;
  napi_value av;
  napi_create_double(env, linear, &lv);
  napi_create_double(env, angular, &av);
  napi_set_named_property(env, out, "linear", lv);
  napi_set_named_property(env, out, "angular", av);
  return out;
}

napi_value ActivateBody(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "activateBody: bodyId must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->ActivateBody(id), &out);
  return out;
}

napi_value DeactivateBody(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "deactivateBody: bodyId must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->DeactivateBody(id), &out);
  return out;
}

napi_value RemoveBody(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "removeBody: bodyId must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->RemoveBody(id), &out);
  return out;
}

napi_value HasBody(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "hasBody: bodyId must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->HasBody(id), &out);
  return out;
}

napi_value IsBodyActive(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "isBodyActive: bodyId must be uint32");
    return nullptr;
  }
  bool active = false;
  if (!handle->world->IsBodyActive(id, active)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, active, &out);
  return out;
}

napi_value SetBodySensor(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  bool is_sensor;
  if (!GetUInt32Arg(env, args[1], &id) || !GetBoolArg(env, args[2], &is_sensor)) {
    ThrowTypeError(env, "setBodySensor: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetBodySensor(id, is_sensor), &out);
  return out;
}

napi_value IsBodySensor(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "isBodySensor: bodyId must be uint32");
    return nullptr;
  }
  bool is_sensor = false;
  if (!handle->world->IsBodySensor(id, is_sensor)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, is_sensor, &out);
  return out;
}

napi_value GetCenterOfMassPosition(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getCenterOfMassPosition: bodyId must be uint32");
    return nullptr;
  }
  RVec3 pos;
  if (!handle->world->GetCenterOfMassPosition(id, pos)) {
    ThrowError(env, "Body not found");
    return nullptr;
  }
  return MakeVec3Object(env, pos);
}

napi_value RayCastClosest(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(8, 9)
  double ox, oy, oz, dx, dy, dz, max_dist;
  if (!GetDoubleArg(env, args[1], &ox) || !GetDoubleArg(env, args[2], &oy) || !GetDoubleArg(env, args[3], &oz) || !GetDoubleArg(env, args[4], &dx) ||
      !GetDoubleArg(env, args[5], &dy) || !GetDoubleArg(env, args[6], &dz) || !GetDoubleArg(env, args[7], &max_dist) || max_dist <= 0) {
    ThrowTypeError(env, "rayCastClosest: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 8) ParseQueryFilters(env, args[8], filters);

  uint32_t body = 0;
  double fraction = 0;
  Vec3 normal;
  uint32_t mat_idx = 0;
  if (!handle->world->RayCastClosest(RVec3(ox, oy, oz), Vec3(static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)), max_dist, filters, body, fraction, normal, mat_idx)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }

  napi_value out;
  napi_create_object(env, &out);

  napi_value body_v, fraction_v, normal_v, mat_v;
  napi_create_uint32(env, body, &body_v);
  napi_create_double(env, fraction, &fraction_v);
  normal_v = MakeVec3Object(env, RVec3(normal.GetX(), normal.GetY(), normal.GetZ()));
  napi_create_uint32(env, mat_idx, &mat_v);

  napi_set_named_property(env, out, "bodyId", body_v);
  napi_set_named_property(env, out, "fraction", fraction_v);
  napi_set_named_property(env, out, "normal", normal_v);
  napi_set_named_property(env, out, "materialIndex", mat_v);
  return out;
}

napi_value RayCastAll(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(8, 9)
  double ox, oy, oz, dx, dy, dz, max_dist;
  if (!GetDoubleArg(env, args[1], &ox) || !GetDoubleArg(env, args[2], &oy) || !GetDoubleArg(env, args[3], &oz) || !GetDoubleArg(env, args[4], &dx) ||
      !GetDoubleArg(env, args[5], &dy) || !GetDoubleArg(env, args[6], &dz) || !GetDoubleArg(env, args[7], &max_dist) || max_dist <= 0) {
    ThrowTypeError(env, "rayCastAll: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 8) ParseQueryFilters(env, args[8], filters);

  std::vector<PhysicsWorld::RayHitInfo> hits;
  if (!handle->world->RayCastAll(RVec3(ox, oy, oz), Vec3(static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)), max_dist, filters, hits)) {
    napi_value arr;
    napi_create_array_with_length(env, 0, &arr);
    return arr;
  }

  napi_value arr;
  napi_create_array_with_length(env, hits.size(), &arr);
  for (size_t i = 0; i < hits.size(); ++i) {
    napi_value item;
    napi_create_object(env, &item);
    napi_value body_v, fraction_v, mat_v;
    napi_value normal_v = MakeVec3Object(env, RVec3(hits[i].normal.GetX(), hits[i].normal.GetY(), hits[i].normal.GetZ()));
    napi_create_uint32(env, hits[i].body_id, &body_v);
    napi_create_double(env, hits[i].fraction, &fraction_v);
    napi_create_uint32(env, hits[i].material_index, &mat_v);
    napi_set_named_property(env, item, "bodyId", body_v);
    napi_set_named_property(env, item, "fraction", fraction_v);
    napi_set_named_property(env, item, "normal", normal_v);
    napi_set_named_property(env, item, "materialIndex", mat_v);
    napi_set_element(env, arr, static_cast<uint32_t>(i), item);
  }
  return arr;
}

napi_value CollideSphereAll(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(6, 7)
  double cx, cy, cz, radius, max_sep;
  if (!GetDoubleArg(env, args[1], &cx) || !GetDoubleArg(env, args[2], &cy) || !GetDoubleArg(env, args[3], &cz) ||
      !GetDoubleArg(env, args[4], &radius) || radius <= 0.0 || !GetDoubleArg(env, args[5], &max_sep) || max_sep < 0.0) {
    ThrowTypeError(env, "collideSphereAll: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 6) ParseQueryFilters(env, args[6], filters);

  std::vector<PhysicsWorld::ShapeQueryHit> hits;
  if (!handle->world->CollideSphereAll(RVec3(cx, cy, cz), static_cast<float>(radius), static_cast<float>(max_sep), filters, hits)) {
    napi_value arr;
    napi_create_array_with_length(env, 0, &arr);
    return arr;
  }

  napi_value arr;
  napi_create_array_with_length(env, hits.size(), &arr);
  for (size_t i = 0; i < hits.size(); ++i) {
    napi_value item;
    napi_create_object(env, &item);
    napi_value body_v, penetration_v, fraction_v, mat_v;
    napi_create_uint32(env, hits[i].body_id, &body_v);
    napi_create_double(env, hits[i].penetration_depth, &penetration_v);
    napi_create_double(env, hits[i].fraction, &fraction_v);
    napi_create_uint32(env, hits[i].material_index, &mat_v);
    napi_set_named_property(env, item, "bodyId", body_v);
    napi_set_named_property(env, item, "penetrationDepth", penetration_v);
    napi_set_named_property(env, item, "fraction", fraction_v);
    napi_set_named_property(env, item, "point", MakeVec3Object(env, RVec3(hits[i].point.GetX(), hits[i].point.GetY(), hits[i].point.GetZ())));
    napi_set_named_property(env, item, "normal", MakeVec3Object(env, RVec3(hits[i].normal.GetX(), hits[i].normal.GetY(), hits[i].normal.GetZ())));
    napi_set_named_property(env, item, "materialIndex", mat_v);
    napi_set_element(env, arr, static_cast<uint32_t>(i), item);
  }
  return arr;
}

napi_value CastSphereAll(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(9, 10)
  double ox, oy, oz, dx, dy, dz, max_dist, radius;
  if (!GetDoubleArg(env, args[1], &ox) || !GetDoubleArg(env, args[2], &oy) || !GetDoubleArg(env, args[3], &oz) || !GetDoubleArg(env, args[4], &dx) ||
      !GetDoubleArg(env, args[5], &dy) || !GetDoubleArg(env, args[6], &dz) || !GetDoubleArg(env, args[7], &max_dist) || max_dist <= 0.0 ||
      !GetDoubleArg(env, args[8], &radius) || radius <= 0.0) {
    ThrowTypeError(env, "castSphereAll: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 9) ParseQueryFilters(env, args[9], filters);

  std::vector<PhysicsWorld::ShapeQueryHit> hits;
  if (!handle->world->CastSphereAll(
          RVec3(ox, oy, oz),
          Vec3(static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)),
          static_cast<float>(max_dist),
          static_cast<float>(radius),
          filters,
          hits)) {
    napi_value arr;
    napi_create_array_with_length(env, 0, &arr);
    return arr;
  }

  napi_value arr;
  napi_create_array_with_length(env, hits.size(), &arr);
  for (size_t i = 0; i < hits.size(); ++i) {
    napi_value item;
    napi_create_object(env, &item);
    napi_value body_v, penetration_v, fraction_v, mat_v;
    napi_create_uint32(env, hits[i].body_id, &body_v);
    napi_create_double(env, hits[i].penetration_depth, &penetration_v);
    napi_create_double(env, hits[i].fraction, &fraction_v);
    napi_create_uint32(env, hits[i].material_index, &mat_v);
    napi_set_named_property(env, item, "bodyId", body_v);
    napi_set_named_property(env, item, "penetrationDepth", penetration_v);
    napi_set_named_property(env, item, "fraction", fraction_v);
    napi_set_named_property(env, item, "point", MakeVec3Object(env, RVec3(hits[i].point.GetX(), hits[i].point.GetY(), hits[i].point.GetZ())));
    napi_set_named_property(env, item, "normal", MakeVec3Object(env, RVec3(hits[i].normal.GetX(), hits[i].normal.GetY(), hits[i].normal.GetZ())));
    napi_set_named_property(env, item, "materialIndex", mat_v);
    napi_set_element(env, arr, static_cast<uint32_t>(i), item);
  }
  return arr;
}

napi_value CastBoxAll(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(11, 12)
  double ox, oy, oz, dx, dy, dz, max_dist, hx, hy, hz;
  if (!GetDoubleArg(env, args[1], &ox) || !GetDoubleArg(env, args[2], &oy) || !GetDoubleArg(env, args[3], &oz) ||
      !GetDoubleArg(env, args[4], &dx) || !GetDoubleArg(env, args[5], &dy) || !GetDoubleArg(env, args[6], &dz) ||
      !GetDoubleArg(env, args[7], &max_dist) || max_dist <= 0.0 ||
      !GetDoubleArg(env, args[8], &hx) || hx <= 0.0 ||
      !GetDoubleArg(env, args[9], &hy) || hy <= 0.0 ||
      !GetDoubleArg(env, args[10], &hz) || hz <= 0.0) {
    ThrowTypeError(env, "castBoxAll: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 11) ParseQueryFilters(env, args[11], filters);

  std::vector<PhysicsWorld::ShapeQueryHit> hits;
  if (!handle->world->CastBoxAll(
          RVec3(ox, oy, oz),
          Vec3(static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)),
          static_cast<float>(max_dist),
          static_cast<float>(hx), static_cast<float>(hy), static_cast<float>(hz),
          filters, hits)) {
    napi_value arr;
    napi_create_array_with_length(env, 0, &arr);
    return arr;
  }

  napi_value arr;
  napi_create_array_with_length(env, hits.size(), &arr);
  for (size_t i = 0; i < hits.size(); ++i) {
    napi_value item;
    napi_create_object(env, &item);
    napi_value body_v, penetration_v, fraction_v, mat_v;
    napi_create_uint32(env, hits[i].body_id, &body_v);
    napi_create_double(env, hits[i].penetration_depth, &penetration_v);
    napi_create_double(env, hits[i].fraction, &fraction_v);
    napi_create_uint32(env, hits[i].material_index, &mat_v);
    napi_set_named_property(env, item, "bodyId", body_v);
    napi_set_named_property(env, item, "penetrationDepth", penetration_v);
    napi_set_named_property(env, item, "fraction", fraction_v);
    napi_set_named_property(env, item, "point", MakeVec3Object(env, RVec3(hits[i].point.GetX(), hits[i].point.GetY(), hits[i].point.GetZ())));
    napi_set_named_property(env, item, "normal", MakeVec3Object(env, RVec3(hits[i].normal.GetX(), hits[i].normal.GetY(), hits[i].normal.GetZ())));
    napi_set_named_property(env, item, "materialIndex", mat_v);
    napi_set_element(env, arr, static_cast<uint32_t>(i), item);
  }
  return arr;
}

napi_value CastCapsuleAll(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(10, 11)
  double ox, oy, oz, dx, dy, dz, max_dist, half_height, radius;
  if (!GetDoubleArg(env, args[1], &ox) || !GetDoubleArg(env, args[2], &oy) || !GetDoubleArg(env, args[3], &oz) ||
      !GetDoubleArg(env, args[4], &dx) || !GetDoubleArg(env, args[5], &dy) || !GetDoubleArg(env, args[6], &dz) ||
      !GetDoubleArg(env, args[7], &max_dist) || max_dist <= 0.0 ||
      !GetDoubleArg(env, args[8], &half_height) || half_height <= 0.0 ||
      !GetDoubleArg(env, args[9], &radius) || radius <= 0.0) {
    ThrowTypeError(env, "castCapsuleAll: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 10) ParseQueryFilters(env, args[10], filters);

  std::vector<PhysicsWorld::ShapeQueryHit> hits;
  if (!handle->world->CastCapsuleAll(
          RVec3(ox, oy, oz),
          Vec3(static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)),
          static_cast<float>(max_dist),
          static_cast<float>(half_height),
          static_cast<float>(radius),
          filters, hits)) {
    napi_value arr;
    napi_create_array_with_length(env, 0, &arr);
    return arr;
  }

  napi_value arr;
  napi_create_array_with_length(env, hits.size(), &arr);
  for (size_t i = 0; i < hits.size(); ++i) {
    napi_value item;
    napi_create_object(env, &item);
    napi_value body_v, penetration_v, fraction_v, mat_v;
    napi_create_uint32(env, hits[i].body_id, &body_v);
    napi_create_double(env, hits[i].penetration_depth, &penetration_v);
    napi_create_double(env, hits[i].fraction, &fraction_v);
    napi_create_uint32(env, hits[i].material_index, &mat_v);
    napi_set_named_property(env, item, "bodyId", body_v);
    napi_set_named_property(env, item, "penetrationDepth", penetration_v);
    napi_set_named_property(env, item, "fraction", fraction_v);
    napi_set_named_property(env, item, "point", MakeVec3Object(env, RVec3(hits[i].point.GetX(), hits[i].point.GetY(), hits[i].point.GetZ())));
    napi_set_named_property(env, item, "normal", MakeVec3Object(env, RVec3(hits[i].normal.GetX(), hits[i].normal.GetY(), hits[i].normal.GetZ())));
    napi_set_named_property(env, item, "materialIndex", mat_v);
    napi_set_element(env, arr, static_cast<uint32_t>(i), item);
  }
  return arr;
}

napi_value QueryAABB(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(7, 8)
  double min_x, min_y, min_z, max_x, max_y, max_z;
  if (!GetDoubleArg(env, args[1], &min_x) || !GetDoubleArg(env, args[2], &min_y) || !GetDoubleArg(env, args[3], &min_z) ||
      !GetDoubleArg(env, args[4], &max_x) || !GetDoubleArg(env, args[5], &max_y) || !GetDoubleArg(env, args[6], &max_z) ||
      min_x > max_x || min_y > max_y || min_z > max_z) {
    ThrowTypeError(env, "queryAABB: invalid args");
    return nullptr;
  }
  QueryFilters filters;
  if (argc > 7) ParseQueryFilters(env, args[7], filters);

  std::vector<uint32_t> bodies;
  if (!handle->world->QueryAABB(RVec3(min_x, min_y, min_z), RVec3(max_x, max_y, max_z), filters, bodies)) {
    napi_value arr;
    napi_create_array_with_length(env, 0, &arr);
    return arr;
  }

  napi_value arr;
  napi_create_array_with_length(env, bodies.size(), &arr);
  for (size_t i = 0; i < bodies.size(); ++i) {
    napi_value id;
    napi_create_uint32(env, bodies[i], &id);
    napi_set_element(env, arr, static_cast<uint32_t>(i), id);
  }
  return arr;
}

napi_value AreBodiesInContact(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t a, b;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b)) {
    ThrowTypeError(env, "areBodiesInContact: body ids must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->AreBodiesInContact(a, b), &out);
  return out;
}

napi_value CreateFixedConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t a, b;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b)) {
    ThrowTypeError(env, "createFixedConstraint: invalid body ids");
    return nullptr;
  }
  uint32_t id = handle->world->CreateFixedConstraint(a, b);
  if (id == 0) {
    ThrowError(env, "Failed to create fixed constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateDistanceConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(11)
  uint32_t a, b;
  double ax, ay, az, bx, by, bz, min_d, max_d;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &ax) || !GetDoubleArg(env, args[4], &ay) ||
      !GetDoubleArg(env, args[5], &az) || !GetDoubleArg(env, args[6], &bx) || !GetDoubleArg(env, args[7], &by) || !GetDoubleArg(env, args[8], &bz) ||
      !GetDoubleArg(env, args[9], &min_d) || !GetDoubleArg(env, args[10], &max_d) || min_d > max_d) {
    ThrowTypeError(env, "createDistanceConstraint: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateDistanceConstraint(a, b, RVec3(ax, ay, az), RVec3(bx, by, bz), static_cast<float>(min_d), static_cast<float>(max_d));
  if (id == 0) {
    ThrowError(env, "Failed to create distance constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateHingeConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(12)
  uint32_t a, b;
  double px, py, pz, ax, ay, az, nx, ny, nz;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &ax) || !GetDoubleArg(env, args[7], &ay) || !GetDoubleArg(env, args[8], &az) ||
      !GetDoubleArg(env, args[9], &nx) || !GetDoubleArg(env, args[10], &ny) || !GetDoubleArg(env, args[11], &nz)) {
    ThrowTypeError(env, "createHingeConstraint: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateHingeConstraint(
      a,
      b,
      RVec3(px, py, pz),
      Vec3(static_cast<float>(ax), static_cast<float>(ay), static_cast<float>(az)),
      Vec3(static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz)));
  if (id == 0) {
    ThrowError(env, "Failed to create hinge constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateSliderConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(14)
  uint32_t a, b;
  double px, py, pz, ax, ay, az, nx, ny, nz, min_l, max_l;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &ax) || !GetDoubleArg(env, args[7], &ay) || !GetDoubleArg(env, args[8], &az) ||
      !GetDoubleArg(env, args[9], &nx) || !GetDoubleArg(env, args[10], &ny) || !GetDoubleArg(env, args[11], &nz) || !GetDoubleArg(env, args[12], &min_l) ||
      !GetDoubleArg(env, args[13], &max_l) || min_l > max_l) {
    ThrowTypeError(env, "createSliderConstraint: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreateSliderConstraint(
      a,
      b,
      RVec3(px, py, pz),
      Vec3(static_cast<float>(ax), static_cast<float>(ay), static_cast<float>(az)),
      Vec3(static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz)),
      static_cast<float>(min_l),
      static_cast<float>(max_l));
  if (id == 0) {
    ThrowError(env, "Failed to create slider constraint");
    return nullptr;
  }

  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreatePointConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(6)
  uint32_t a, b;
  double px, py, pz;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz)) {
    ThrowTypeError(env, "createPointConstraint: invalid args");
    return nullptr;
  }

  uint32_t id = handle->world->CreatePointConstraint(a, b, RVec3(px, py, pz));
  if (id == 0) {
    ThrowError(env, "Failed to create point constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateConeConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  uint32_t a, b;
  double px, py, pz, ax, ay, az, half_angle;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &ax) || !GetDoubleArg(env, args[7], &ay) || !GetDoubleArg(env, args[8], &az) ||
      !GetDoubleArg(env, args[9], &half_angle)) {
    ThrowTypeError(env, "createConeConstraint: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateConeConstraint(
      a, b, RVec3(px, py, pz), Vec3(static_cast<float>(ax), static_cast<float>(ay), static_cast<float>(az)), static_cast<float>(half_angle));
  if (id == 0) {
    ThrowError(env, "Failed to create cone constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateSwingTwistConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(16)
  uint32_t a, b;
  double px, py, pz, tx, ty, tz, plx, ply, plz, normal_half, plane_half, twist_min, twist_max;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &tx) || !GetDoubleArg(env, args[7], &ty) || !GetDoubleArg(env, args[8], &tz) ||
      !GetDoubleArg(env, args[9], &plx) || !GetDoubleArg(env, args[10], &ply) || !GetDoubleArg(env, args[11], &plz) ||
      !GetDoubleArg(env, args[12], &normal_half) || !GetDoubleArg(env, args[13], &plane_half) || !GetDoubleArg(env, args[14], &twist_min) ||
      !GetDoubleArg(env, args[15], &twist_max)) {
    ThrowTypeError(env, "createSwingTwistConstraint: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateSwingTwistConstraint(
      a,
      b,
      RVec3(px, py, pz),
      Vec3(static_cast<float>(tx), static_cast<float>(ty), static_cast<float>(tz)),
      Vec3(static_cast<float>(plx), static_cast<float>(ply), static_cast<float>(plz)),
      static_cast<float>(normal_half),
      static_cast<float>(plane_half),
      static_cast<float>(twist_min),
      static_cast<float>(twist_max));
  if (id == 0) {
    ThrowError(env, "Failed to create swing twist constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateSixDOFConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(12)
  uint32_t a, b;
  double px, py, pz, axx, axy, axz, ayx, ayy, ayz;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &axx) || !GetDoubleArg(env, args[7], &axy) || !GetDoubleArg(env, args[8], &axz) ||
      !GetDoubleArg(env, args[9], &ayx) || !GetDoubleArg(env, args[10], &ayy) || !GetDoubleArg(env, args[11], &ayz)) {
    ThrowTypeError(env, "createSixDOFConstraint: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateSixDOFConstraint(
      a,
      b,
      RVec3(px, py, pz),
      Vec3(static_cast<float>(axx), static_cast<float>(axy), static_cast<float>(axz)),
      Vec3(static_cast<float>(ayx), static_cast<float>(ayy), static_cast<float>(ayz)));
  if (id == 0) {
    ThrowError(env, "Failed to create six dof constraint");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value RemoveConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "removeConstraint: id must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->RemoveConstraintById(id), &out);
  return out;
}

napi_value SetHingeLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id;
  double min_angle;
  double max_angle;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &min_angle) || !GetDoubleArg(env, args[3], &max_angle)) {
    ThrowTypeError(env, "setHingeLimits: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetHingeLimits(id, static_cast<float>(min_angle), static_cast<float>(max_angle)), &out);
  return out;
}

napi_value SetSliderLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id;
  double min_limit;
  double max_limit;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &min_limit) || !GetDoubleArg(env, args[3], &max_limit)) {
    ThrowTypeError(env, "setSliderLimits: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetSliderLimits(id, static_cast<float>(min_limit), static_cast<float>(max_limit)), &out);
  return out;
}

napi_value SetHingeMotor(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(6)
  uint32_t id;
  int32_t state;
  double target_velocity;
  double target_angle;
  double max_torque;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &state) || !GetDoubleArg(env, args[3], &target_velocity) ||
      !GetDoubleArg(env, args[4], &target_angle) || !GetDoubleArg(env, args[5], &max_torque)) {
    ThrowTypeError(env, "setHingeMotor: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetHingeMotor(id, state, static_cast<float>(target_velocity), static_cast<float>(target_angle), static_cast<float>(max_torque)),
      &out);
  return out;
}

napi_value SetSliderMotor(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(6)
  uint32_t id;
  int32_t state;
  double target_velocity;
  double target_position;
  double max_force;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &state) || !GetDoubleArg(env, args[3], &target_velocity) ||
      !GetDoubleArg(env, args[4], &target_position) || !GetDoubleArg(env, args[5], &max_force)) {
    ThrowTypeError(env, "setSliderMotor: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetSliderMotor(id, state, static_cast<float>(target_velocity), static_cast<float>(target_position), static_cast<float>(max_force)),
      &out);
  return out;
}

napi_value SetConeHalfAngle(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  double half_angle;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &half_angle)) {
    ThrowTypeError(env, "setConeHalfAngle: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetConeHalfAngle(id, static_cast<float>(half_angle)), &out);
  return out;
}

napi_value SetSwingTwistLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(6)
  uint32_t id;
  double normal_half;
  double plane_half;
  double twist_min;
  double twist_max;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &normal_half) || !GetDoubleArg(env, args[3], &plane_half) ||
      !GetDoubleArg(env, args[4], &twist_min) || !GetDoubleArg(env, args[5], &twist_max)) {
    ThrowTypeError(env, "setSwingTwistLimits: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetSwingTwistLimits(
          id, static_cast<float>(normal_half), static_cast<float>(plane_half), static_cast<float>(twist_min), static_cast<float>(twist_max)),
      &out);
  return out;
}

napi_value SetSwingTwistMotor(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(12)
  uint32_t id;
  int32_t swing_state;
  int32_t twist_state;
  double vx, vy, vz, qx, qy, qz, qw, max_torque;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &swing_state) || !GetInt32Arg(env, args[3], &twist_state) ||
      !GetDoubleArg(env, args[4], &vx) || !GetDoubleArg(env, args[5], &vy) || !GetDoubleArg(env, args[6], &vz) || !GetDoubleArg(env, args[7], &qx) ||
      !GetDoubleArg(env, args[8], &qy) || !GetDoubleArg(env, args[9], &qz) || !GetDoubleArg(env, args[10], &qw) || !GetDoubleArg(env, args[11], &max_torque)) {
    ThrowTypeError(env, "setSwingTwistMotor: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetSwingTwistMotor(
          id,
          swing_state,
          twist_state,
          Vec3(static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz)),
          Quat(static_cast<float>(qx), static_cast<float>(qy), static_cast<float>(qz), static_cast<float>(qw)),
          static_cast<float>(max_torque)),
      &out);
  return out;
}

napi_value SetSixDOFLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(14)
  uint32_t id;
  double tminx, tminy, tminz, tmaxx, tmaxy, tmaxz, rminx, rminy, rminz, rmaxx, rmaxy, rmaxz;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &tminx) || !GetDoubleArg(env, args[3], &tminy) || !GetDoubleArg(env, args[4], &tminz) ||
      !GetDoubleArg(env, args[5], &tmaxx) || !GetDoubleArg(env, args[6], &tmaxy) || !GetDoubleArg(env, args[7], &tmaxz) || !GetDoubleArg(env, args[8], &rminx) ||
      !GetDoubleArg(env, args[9], &rminy) || !GetDoubleArg(env, args[10], &rminz) || !GetDoubleArg(env, args[11], &rmaxx) || !GetDoubleArg(env, args[12], &rmaxy) ||
      !GetDoubleArg(env, args[13], &rmaxz)) {
    ThrowTypeError(env, "setSixDOFLimits: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetSixDOFLimits(
          id,
          Vec3(static_cast<float>(tminx), static_cast<float>(tminy), static_cast<float>(tminz)),
          Vec3(static_cast<float>(tmaxx), static_cast<float>(tmaxy), static_cast<float>(tmaxz)),
          Vec3(static_cast<float>(rminx), static_cast<float>(rminy), static_cast<float>(rminz)),
          Vec3(static_cast<float>(rmaxx), static_cast<float>(rmaxy), static_cast<float>(rmaxz))),
      &out);
  return out;
}

napi_value SetSixDOFMotorState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id;
  int32_t axis;
  int32_t state;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &axis) || !GetInt32Arg(env, args[3], &state)) {
    ThrowTypeError(env, "setSixDOFMotorState: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetSixDOFMotorState(id, axis, state), &out);
  return out;
}

napi_value SetSixDOFTargetVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(8)
  uint32_t id;
  double lx, ly, lz, ax, ay, az;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &lx) || !GetDoubleArg(env, args[3], &ly) || !GetDoubleArg(env, args[4], &lz) ||
      !GetDoubleArg(env, args[5], &ax) || !GetDoubleArg(env, args[6], &ay) || !GetDoubleArg(env, args[7], &az)) {
    ThrowTypeError(env, "setSixDOFTargetVelocity: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetSixDOFTargetVelocity(
          id, Vec3(static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(lz)), Vec3(static_cast<float>(ax), static_cast<float>(ay), static_cast<float>(az))),
      &out);
  return out;
}

napi_value SetSixDOFTargetPose(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(9)
  uint32_t id;
  double px, py, pz, qx, qy, qz, qw;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &px) || !GetDoubleArg(env, args[3], &py) || !GetDoubleArg(env, args[4], &pz) ||
      !GetDoubleArg(env, args[5], &qx) || !GetDoubleArg(env, args[6], &qy) || !GetDoubleArg(env, args[7], &qz) || !GetDoubleArg(env, args[8], &qw)) {
    ThrowTypeError(env, "setSixDOFTargetPose: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(
      env,
      handle->world->SetSixDOFTargetPose(
          id,
          Vec3(static_cast<float>(px), static_cast<float>(py), static_cast<float>(pz)),
          Quat(static_cast<float>(qx), static_cast<float>(qy), static_cast<float>(qz), static_cast<float>(qw))),
      &out);
  return out;
}

// ─── Motor spring setters ──────────────────────────────────────────────────
napi_value SetHingeMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "setHingeMotorSpring: need (id, opts)"); return nullptr; }
  MotorSpringParams p; ParseMotorSpringParams(env, args[2], p);
  napi_value out; napi_get_boolean(env, handle->world->SetHingeMotorSpring(id, p), &out); return out;
}

napi_value SetSliderMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "setSliderMotorSpring: need (id, opts)"); return nullptr; }
  MotorSpringParams p; ParseMotorSpringParams(env, args[2], p);
  napi_value out; napi_get_boolean(env, handle->world->SetSliderMotorSpring(id, p), &out); return out;
}

napi_value SetSwingMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "setSwingMotorSpring: need (id, opts)"); return nullptr; }
  MotorSpringParams p; ParseMotorSpringParams(env, args[2], p);
  napi_value out; napi_get_boolean(env, handle->world->SetSwingMotorSpring(id, p), &out); return out;
}

napi_value SetTwistMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "setTwistMotorSpring: need (id, opts)"); return nullptr; }
  MotorSpringParams p; ParseMotorSpringParams(env, args[2], p);
  napi_value out; napi_get_boolean(env, handle->world->SetTwistMotorSpring(id, p), &out); return out;
}

napi_value SetSixDOFMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id; int32_t axis;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &axis)) {
    ThrowTypeError(env, "setSixDOFMotorSpring: need (id, axis, opts)"); return nullptr;
  }
  MotorSpringParams p; ParseMotorSpringParams(env, args[3], p);
  napi_value out; napi_get_boolean(env, handle->world->SetSixDOFMotorSpring(id, axis, p), &out); return out;
}

// ─── Motor spring getters ──────────────────────────────────────────────────
napi_value GetHingeMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getHingeMotorSpring: need (id)"); return nullptr; }
  MotorSettings ms; if (!handle->world->GetHingeMotorSpring(id, ms)) { napi_value n; napi_get_null(env, &n); return n; }
  return MakeMotorSpringObject(env, ms);
}

napi_value GetSliderMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getSliderMotorSpring: need (id)"); return nullptr; }
  MotorSettings ms; if (!handle->world->GetSliderMotorSpring(id, ms)) { napi_value n; napi_get_null(env, &n); return n; }
  return MakeMotorSpringObject(env, ms);
}

napi_value GetSwingMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getSwingMotorSpring: need (id)"); return nullptr; }
  MotorSettings ms; if (!handle->world->GetSwingMotorSpring(id, ms)) { napi_value n; napi_get_null(env, &n); return n; }
  return MakeMotorSpringObject(env, ms);
}

napi_value GetTwistMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getTwistMotorSpring: need (id)"); return nullptr; }
  MotorSettings ms; if (!handle->world->GetTwistMotorSpring(id, ms)) { napi_value n; napi_get_null(env, &n); return n; }
  return MakeMotorSpringObject(env, ms);
}

napi_value GetSixDOFMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id; int32_t axis;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &axis)) {
    ThrowTypeError(env, "getSixDOFMotorSpring: need (id, axis)"); return nullptr;
  }
  MotorSettings ms; if (!handle->world->GetSixDOFMotorSpring(id, axis, ms)) { napi_value n; napi_get_null(env, &n); return n; }
  return MakeMotorSpringObject(env, ms);
}

napi_value GetHingeAngle(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getHingeAngle: invalid args");
    return nullptr;
  }
  float angle = 0.0f;
  if (!handle->world->GetHingeAngle(id, angle)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_double(env, static_cast<double>(angle), &out);
  return out;
}

napi_value GetHingeMotorState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getHingeMotorState: invalid args");
    return nullptr;
  }
  int32_t state = 0;
  if (!handle->world->GetHingeMotorState(id, state)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_int32(env, state, &out);
  return out;
}

napi_value GetSliderPosition(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSliderPosition: invalid args");
    return nullptr;
  }
  float position = 0.0f;
  if (!handle->world->GetSliderPosition(id, position)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_double(env, static_cast<double>(position), &out);
  return out;
}

napi_value GetSliderMotorState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSliderMotorState: invalid args");
    return nullptr;
  }
  int32_t state = 0;
  if (!handle->world->GetSliderMotorState(id, state)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_int32(env, state, &out);
  return out;
}

napi_value GetSixDOFRotation(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSixDOFRotation: invalid args");
    return nullptr;
  }
  Quat rotation;
  if (!handle->world->GetSixDOFRotation(id, rotation)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  return MakeQuatObject(env, rotation);
}

napi_value GetSixDOFLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSixDOFLimits: invalid args");
    return nullptr;
  }
  Vec3 tmin, tmax, rmin, rmax;
  if (!handle->world->GetSixDOFLimits(id, tmin, tmax, rmin, rmax)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_object(env, &out);
  napi_set_named_property(env, out, "translationMin", MakeVec3Object(env, RVec3(tmin)));
  napi_set_named_property(env, out, "translationMax", MakeVec3Object(env, RVec3(tmax)));
  napi_set_named_property(env, out, "rotationMin", MakeVec3Object(env, RVec3(rmin)));
  napi_set_named_property(env, out, "rotationMax", MakeVec3Object(env, RVec3(rmax)));
  return out;
}

napi_value GetSixDOFMotorState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  int32_t axis;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &axis)) {
    ThrowTypeError(env, "getSixDOFMotorState: invalid args");
    return nullptr;
  }
  int32_t state = 0;
  if (!handle->world->GetSixDOFMotorState(id, axis, state)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_int32(env, state, &out);
  return out;
}

napi_value GetSwingTwistRotation(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSwingTwistRotation: invalid args");
    return nullptr;
  }
  Quat rotation;
  if (!handle->world->GetSwingTwistRotation(id, rotation)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  return MakeQuatObject(env, rotation);
}

napi_value GetSwingTwistMotorState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSwingTwistMotorState: invalid args");
    return nullptr;
  }
  int32_t swing_state = 0;
  int32_t twist_state = 0;
  if (!handle->world->GetSwingTwistMotorState(id, swing_state, twist_state)) {
    napi_value n;
    napi_get_null(env, &n);
    return n;
  }
  napi_value out;
  napi_create_object(env, &out);
  napi_value swing, twist;
  napi_create_int32(env, swing_state, &swing);
  napi_create_int32(env, twist_state, &twist);
  napi_set_named_property(env, out, "swingState", swing);
  napi_set_named_property(env, out, "twistState", twist);
  return out;
}

napi_value GetHingeLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getHingeLambdas: invalid args"); return nullptr; }
  Vec3 pos; float rx, ry, rlim, motor;
  if (!handle->world->GetHingeLambdas(id, pos, rx, ry, rlim, motor)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec3Object(env, pos));
  napi_set_named_property(env, out, "rotation", MakeVec2Object(env, rx, ry));
  SetF64Prop(env, out, "rotationLimits", rlim);
  SetF64Prop(env, out, "motor", motor);
  return out;
}

napi_value GetSliderLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getSliderLambdas: invalid args"); return nullptr; }
  float px, py, plim, motor; Vec3 rot;
  if (!handle->world->GetSliderLambdas(id, px, py, plim, rot, motor)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec2Object(env, px, py));
  SetF64Prop(env, out, "positionLimits", plim);
  napi_set_named_property(env, out, "rotation", MakeVec3Object(env, rot));
  SetF64Prop(env, out, "motor", motor);
  return out;
}

napi_value GetSwingTwistLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getSwingTwistLambdas: invalid args"); return nullptr; }
  Vec3 pos, motor; float twist, swy, swz;
  if (!handle->world->GetSwingTwistLambdas(id, pos, twist, swy, swz, motor)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec3Object(env, pos));
  SetF64Prop(env, out, "twist", twist);
  SetF64Prop(env, out, "swingY", swy);
  SetF64Prop(env, out, "swingZ", swz);
  napi_set_named_property(env, out, "motor", MakeVec3Object(env, motor));
  return out;
}

napi_value GetSixDOFLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getSixDOFLambdas: invalid args"); return nullptr; }
  Vec3 pos, rot, mtrans, mrot;
  if (!handle->world->GetSixDOFLambdas(id, pos, rot, mtrans, mrot)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec3Object(env, pos));
  napi_set_named_property(env, out, "rotation", MakeVec3Object(env, rot));
  napi_set_named_property(env, out, "motorTranslation", MakeVec3Object(env, mtrans));
  napi_set_named_property(env, out, "motorRotation", MakeVec3Object(env, mrot));
  return out;
}

napi_value GetConeLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getConeLambdas: invalid args"); return nullptr; }
  Vec3 pos; float rot;
  if (!handle->world->GetConeLambdas(id, pos, rot)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec3Object(env, pos));
  SetF64Prop(env, out, "rotation", rot);
  return out;
}

napi_value GetPointLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getPointLambdas: invalid args"); return nullptr; }
  Vec3 pos;
  if (!handle->world->GetPointLambdas(id, pos)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec3Object(env, pos));
  return out;
}

napi_value GetFixedLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getFixedLambdas: invalid args"); return nullptr; }
  Vec3 pos, rot;
  if (!handle->world->GetFixedLambdas(id, pos, rot)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec3Object(env, pos));
  napi_set_named_property(env, out, "rotation", MakeVec3Object(env, rot));
  return out;
}

napi_value GetDistanceLambda(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getDistanceLambda: invalid args"); return nullptr; }
  float out_val;
  if (!handle->world->GetDistanceLambda(id, out_val)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_double(env, static_cast<double>(out_val), &out);
  return out;
}

napi_value GetPulleyLambda(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getPulleyLambda: invalid args"); return nullptr; }
  float out_val;
  if (!handle->world->GetPulleyLambda(id, out_val)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_double(env, static_cast<double>(out_val), &out);
  return out;
}

napi_value GetGearLambda(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getGearLambda: invalid args"); return nullptr; }
  float out_val;
  if (!handle->world->GetGearLambda(id, out_val)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_double(env, static_cast<double>(out_val), &out);
  return out;
}

napi_value GetPathLambdas(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) { ThrowTypeError(env, "getPathLambdas: invalid args"); return nullptr; }
  float px, py, plim, motor, rhx, rhy; Vec3 rot;
  if (!handle->world->GetPathLambdas(id, px, py, plim, motor, rhx, rhy, rot)) { napi_value n; napi_get_null(env, &n); return n; }
  napi_value out; napi_create_object(env, &out);
  napi_set_named_property(env, out, "position", MakeVec2Object(env, px, py));
  SetF64Prop(env, out, "positionLimits", plim);
  SetF64Prop(env, out, "motor", motor);
  napi_set_named_property(env, out, "rotationHinge", MakeVec2Object(env, rhx, rhy));
  napi_set_named_property(env, out, "rotation", MakeVec3Object(env, rot));
  return out;
}

napi_value GetHingeLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getHingeLimits: invalid args");
    return nullptr;
  }
  float vmin = 0.0f, vmax = 0.0f;
  if (!handle->world->GetHingeLimits(id, vmin, vmax)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out, nmin, nmax;
  napi_create_object(env, &out);
  napi_create_double(env, static_cast<double>(vmin), &nmin);
  napi_create_double(env, static_cast<double>(vmax), &nmax);
  napi_set_named_property(env, out, "min", nmin);
  napi_set_named_property(env, out, "max", nmax);
  return out;
}

napi_value GetSliderLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSliderLimits: invalid args");
    return nullptr;
  }
  float vmin = 0.0f, vmax = 0.0f;
  if (!handle->world->GetSliderLimits(id, vmin, vmax)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out, nmin, nmax;
  napi_create_object(env, &out);
  napi_create_double(env, static_cast<double>(vmin), &nmin);
  napi_create_double(env, static_cast<double>(vmax), &nmax);
  napi_set_named_property(env, out, "min", nmin);
  napi_set_named_property(env, out, "max", nmax);
  return out;
}

napi_value GetSwingTwistLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getSwingTwistLimits: invalid args");
    return nullptr;
  }
  float n_half = 0.0f, p_half = 0.0f, t_min = 0.0f, t_max = 0.0f;
  if (!handle->world->GetSwingTwistLimits(id, n_half, p_half, t_min, t_max)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out, vnh, vph, vtmin, vtmax;
  napi_create_object(env, &out);
  napi_create_double(env, static_cast<double>(n_half), &vnh);
  napi_create_double(env, static_cast<double>(p_half), &vph);
  napi_create_double(env, static_cast<double>(t_min), &vtmin);
  napi_create_double(env, static_cast<double>(t_max), &vtmax);
  napi_set_named_property(env, out, "normalHalfCone", vnh);
  napi_set_named_property(env, out, "planeHalfCone", vph);
  napi_set_named_property(env, out, "twistMin", vtmin);
  napi_set_named_property(env, out, "twistMax", vtmax);
  return out;
}

napi_value CreateGearConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(10, 12)
  uint32_t a, b;
  double h1x, h1y, h1z, h2x, h2y, h2z, ratio;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) ||
      napi_get_value_double(env, args[3], &h1x) != napi_ok ||
      napi_get_value_double(env, args[4], &h1y) != napi_ok ||
      napi_get_value_double(env, args[5], &h1z) != napi_ok ||
      napi_get_value_double(env, args[6], &h2x) != napi_ok ||
      napi_get_value_double(env, args[7], &h2y) != napi_ok ||
      napi_get_value_double(env, args[8], &h2z) != napi_ok ||
      napi_get_value_double(env, args[9], &ratio) != napi_ok) {
    ThrowTypeError(env, "createGearConstraint: invalid args");
    return nullptr;
  }
  uint32_t c1 = 0, c2 = 0;
  if (argc > 10) GetUInt32Arg(env, args[10], &c1);
  if (argc > 11) GetUInt32Arg(env, args[11], &c2);
  const uint32_t id = handle->world->CreateGearConstraint(
    a, b,
    Vec3(static_cast<float>(h1x), static_cast<float>(h1y), static_cast<float>(h1z)),
    Vec3(static_cast<float>(h2x), static_cast<float>(h2y), static_cast<float>(h2z)),
    static_cast<float>(ratio), c1, c2);
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreatePulleyConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(16, 18)
  uint32_t a, b;
  double bp1x, bp1y, bp1z, fp1x, fp1y, fp1z, bp2x, bp2y, bp2z, fp2x, fp2y, fp2z, ratio;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b) ||
      napi_get_value_double(env, args[3], &bp1x) != napi_ok ||
      napi_get_value_double(env, args[4], &bp1y) != napi_ok ||
      napi_get_value_double(env, args[5], &bp1z) != napi_ok ||
      napi_get_value_double(env, args[6], &fp1x) != napi_ok ||
      napi_get_value_double(env, args[7], &fp1y) != napi_ok ||
      napi_get_value_double(env, args[8], &fp1z) != napi_ok ||
      napi_get_value_double(env, args[9], &bp2x) != napi_ok ||
      napi_get_value_double(env, args[10], &bp2y) != napi_ok ||
      napi_get_value_double(env, args[11], &bp2z) != napi_ok ||
      napi_get_value_double(env, args[12], &fp2x) != napi_ok ||
      napi_get_value_double(env, args[13], &fp2y) != napi_ok ||
      napi_get_value_double(env, args[14], &fp2z) != napi_ok ||
      napi_get_value_double(env, args[15], &ratio) != napi_ok) {
    ThrowTypeError(env, "createPulleyConstraint: invalid args");
    return nullptr;
  }
  double min_len = 0.0, max_len = -1.0;
  if (argc > 16) napi_get_value_double(env, args[16], &min_len);
  if (argc > 17) napi_get_value_double(env, args[17], &max_len);
  const uint32_t id = handle->world->CreatePulleyConstraint(
    a, b,
    RVec3(static_cast<float>(bp1x), static_cast<float>(bp1y), static_cast<float>(bp1z)),
    RVec3(static_cast<float>(fp1x), static_cast<float>(fp1y), static_cast<float>(fp1z)),
    RVec3(static_cast<float>(bp2x), static_cast<float>(bp2y), static_cast<float>(bp2z)),
    RVec3(static_cast<float>(fp2x), static_cast<float>(fp2y), static_cast<float>(fp2z)),
    static_cast<float>(ratio), static_cast<float>(min_len), static_cast<float>(max_len));
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value GetPulleyLength(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getPulleyLength: invalid args");
    return nullptr;
  }
  float length = 0.0f;
  if (!handle->world->GetPulleyLength(id, length)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out;
  napi_create_double(env, static_cast<double>(length), &out);
  return out;
}

napi_value SetPulleyLength(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id;
  double min_len, max_len;
  if (!GetUInt32Arg(env, args[1], &id) ||
      napi_get_value_double(env, args[2], &min_len) != napi_ok ||
      napi_get_value_double(env, args[3], &max_len) != napi_ok) {
    ThrowTypeError(env, "setPulleyLength: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetPulleyLengthLimits(id, static_cast<float>(min_len), static_cast<float>(max_len)), &out);
  return out;
}

napi_value GetPulleyLengthLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getPulleyLengthLimits: invalid args");
    return nullptr;
  }
  float vmin = 0.0f, vmax = 0.0f;
  if (!handle->world->GetPulleyLengthLimits(id, vmin, vmax)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out, nmin, nmax;
  napi_create_object(env, &out);
  napi_create_double(env, static_cast<double>(vmin), &nmin);
  napi_create_double(env, static_cast<double>(vmax), &nmax);
  napi_set_named_property(env, out, "min", nmin);
  napi_set_named_property(env, out, "max", nmax);
  return out;
}

napi_value CreatePathConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(15)
  uint32_t a, b;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b)) {
    ThrowTypeError(env, "createPathConstraint: invalid args");
    return nullptr;
  }
  bool is_arr = false;
  napi_is_array(env, args[3], &is_arr);
  if (!is_arr) {
    ThrowTypeError(env, "createPathConstraint: points must be an array");
    return nullptr;
  }
  uint32_t pts_len = 0;
  napi_get_array_length(env, args[3], &pts_len);
  if (pts_len % 9 != 0) {
    ThrowTypeError(env, "createPathConstraint: points length must be multiple of 9");
    return nullptr;
  }
  std::vector<float> flat_points(pts_len);
  for (uint32_t i = 0; i < pts_len; i++) {
    napi_value elem;
    napi_get_element(env, args[3], i, &elem);
    double val = 0.0;
    napi_get_value_double(env, elem, &val);
    flat_points[i] = static_cast<float>(val);
  }
  bool closed = false;
  napi_get_value_bool(env, args[4], &closed);
  double ppx, ppy, ppz, prx, pry, prz, prw, pf, mf;
  int32_t rot_type;
  if (napi_get_value_double(env, args[5], &ppx) != napi_ok ||
      napi_get_value_double(env, args[6], &ppy) != napi_ok ||
      napi_get_value_double(env, args[7], &ppz) != napi_ok ||
      napi_get_value_double(env, args[8], &prx) != napi_ok ||
      napi_get_value_double(env, args[9], &pry) != napi_ok ||
      napi_get_value_double(env, args[10], &prz) != napi_ok ||
      napi_get_value_double(env, args[11], &prw) != napi_ok ||
      napi_get_value_double(env, args[12], &pf) != napi_ok ||
      napi_get_value_double(env, args[13], &mf) != napi_ok ||
      !GetInt32Arg(env, args[14], &rot_type)) {
    ThrowTypeError(env, "createPathConstraint: invalid numeric args");
    return nullptr;
  }
  const uint32_t id = handle->world->CreatePathConstraint(
    a, b, flat_points, closed,
    Vec3(static_cast<float>(ppx), static_cast<float>(ppy), static_cast<float>(ppz)),
    Quat(static_cast<float>(prx), static_cast<float>(pry), static_cast<float>(prz), static_cast<float>(prw)),
    static_cast<float>(pf), static_cast<float>(mf), static_cast<int>(rot_type));
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value GetPathFraction(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getPathFraction: invalid args");
    return nullptr;
  }
  float frac = 0.0f;
  if (!handle->world->GetPathFraction(id, frac)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out;
  napi_create_double(env, static_cast<double>(frac), &out);
  return out;
}

napi_value GetPathMaxFraction(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getPathMaxFraction: invalid args");
    return nullptr;
  }
  float max_frac = 0.0f;
  if (!handle->world->GetPathMaxFraction(id, max_frac)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out;
  napi_create_double(env, static_cast<double>(max_frac), &out);
  return out;
}

napi_value SetPathMotor(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(3, 5)
  uint32_t id;
  int32_t state;
  if (!GetUInt32Arg(env, args[1], &id) || !GetInt32Arg(env, args[2], &state)) {
    ThrowTypeError(env, "setPathMotor: invalid args");
    return nullptr;
  }
  bool ok_res = handle->world->SetPathMotorState(id, state);
  if (ok_res && argc > 3) {
    double vel = 0.0;
    napi_get_value_double(env, args[3], &vel);
    handle->world->SetPathTargetVelocity(id, static_cast<float>(vel));
  }
  if (ok_res && argc > 4) {
    double frac = 0.0;
    napi_get_value_double(env, args[4], &frac);
    handle->world->SetPathTargetFraction(id, static_cast<float>(frac));
  }
  napi_value out;
  napi_get_boolean(env, ok_res, &out);
  return out;
}

napi_value GetPathMotorState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getPathMotorState: invalid args");
    return nullptr;
  }
  int32_t state = 0;
  float vel = 0.0f, frac = 0.0f;
  if (!handle->world->GetPathMotorState(id, state, vel, frac)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out, ns, nv, nf;
  napi_create_object(env, &out);
  napi_create_int32(env, state, &ns);
  napi_create_double(env, static_cast<double>(vel), &nv);
  napi_create_double(env, static_cast<double>(frac), &nf);
  napi_set_named_property(env, out, "state", ns);
  napi_set_named_property(env, out, "targetVelocity", nv);
  napi_set_named_property(env, out, "targetFraction", nf);
  return out;
}

napi_value SetPathMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "setPathMotorSpring: invalid args");
    return nullptr;
  }
  MotorSpringParams p;
  ParseMotorSpringParams(env, args[2], p);
  napi_value out;
  napi_get_boolean(env, handle->world->SetPathMotorSpring(id, p), &out);
  return out;
}

napi_value GetPathMotorSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getPathMotorSpring: invalid args");
    return nullptr;
  }
  MotorSettings ms;
  if (!handle->world->GetPathMotorSpring(id, ms)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  return MakeMotorSpringObject(env, ms);
}

// ─── Distance constraint limits & spring ─────────────────────────────────────

napi_value SetDistanceLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t id; double mn, mx;
  if (!GetUInt32Arg(env, args[1], &id) || !GetDoubleArg(env, args[2], &mn) || !GetDoubleArg(env, args[3], &mx)) {
    ThrowTypeError(env, "setDistanceLimits: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetDistanceLimits(id, static_cast<float>(mn), static_cast<float>(mx)), &out);
  return out;
}

napi_value GetDistanceLimits(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getDistanceLimits: invalid args");
    return nullptr;
  }
  float mn = 0.0f, mx = 0.0f;
  if (!handle->world->GetDistanceLimits(id, mn, mx)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value obj, mnv, mxv;
  napi_create_object(env, &obj);
  napi_create_double(env, static_cast<double>(mn), &mnv);
  napi_create_double(env, static_cast<double>(mx), &mxv);
  napi_set_named_property(env, obj, "min", mnv);
  napi_set_named_property(env, obj, "max", mxv);
  return obj;
}

napi_value SetDistanceLimitsSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "setDistanceLimitsSpring: invalid args");
    return nullptr;
  }
  SpringSettings ss;
  ParseSpringSettings(env, args[2], ss);
  napi_value out;
  napi_get_boolean(env, handle->world->SetDistanceLimitsSpring(id, ss), &out);
  return out;
}

napi_value GetDistanceLimitsSpring(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getDistanceLimitsSpring: invalid args");
    return nullptr;
  }
  SpringSettings ss;
  if (!handle->world->GetDistanceLimitsSpring(id, ss)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  return MakeSpringSettingsObject(env, ss);
}

// ─── RackAndPinion constraint ────────────────────────────────────────────────

napi_value CreateRackAndPinionConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t a, b;
  if (!GetUInt32Arg(env, args[1], &a) || !GetUInt32Arg(env, args[2], &b)) {
    ThrowTypeError(env, "createRackAndPinionConstraint: invalid body IDs");
    return nullptr;
  }
  napi_value opts = args[3];
  auto getVec = [&](const char *key, Vec3 def) -> Vec3 {
    Vec3 v = def;
    GetVec3ObjProp(env, opts, key, v);
    return v;
  };
  auto getF = [&](const char *key, float def) -> float {
    napi_value v;
    if (napi_get_named_property(env, opts, key, &v) == napi_ok) {
      double d = 0.0;
      if (GetDoubleArg(env, v, &d)) return static_cast<float>(d);
    }
    return def;
  };
  auto getU = [&](const char *key, uint32_t def) -> uint32_t {
    napi_value v;
    if (napi_get_named_property(env, opts, key, &v) == napi_ok) {
      uint32_t u = 0;
      if (GetUInt32Arg(env, v, &u)) return u;
    }
    return def;
  };
  Vec3 hinge_axis = getVec("hingeAxis", Vec3(0, 1, 0));
  Vec3 slider_axis = getVec("sliderAxis", Vec3(1, 0, 0));
  float ratio = getF("ratio", 1.0f);
  uint32_t pinion_id = getU("pinionConstraintId", 0);
  uint32_t rack_id   = getU("rackConstraintId",   0);
  const uint32_t id = handle->world->CreateRackAndPinionConstraint(a, b, hinge_axis, slider_axis, ratio, pinion_id, rack_id);
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value GetRackAndPinionLambda(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id;
  if (!GetUInt32Arg(env, args[1], &id)) {
    ThrowTypeError(env, "getRackAndPinionLambda: invalid args");
    return nullptr;
  }
  float val = 0.0f;
  if (!handle->world->GetRackAndPinionLambda(id, val)) {
    napi_value n; napi_get_null(env, &n); return n;
  }
  napi_value out;
  napi_create_double(env, static_cast<double>(val), &out);
  return out;
}

// ─── HeightField shape ────────────────────────────────────────────────────────

napi_value CreateHeightField(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  // args[1] = opts { samples: Float32Array|Array, sampleCount, offset, scale, position, friction, restitution }
  napi_value opts = args[1];

  // sampleCount
  uint32_t sample_count = 0;
  {
    napi_value v;
    if (napi_get_named_property(env, opts, "sampleCount", &v) != napi_ok || !GetUInt32Arg(env, v, &sample_count) || sample_count < 2) {
      ThrowTypeError(env, "createHeightField: sampleCount must be >= 2");
      return nullptr;
    }
  }

  // samples — accept Float32Array or regular Array
  std::vector<float> samples;
  {
    napi_value sv;
    if (napi_get_named_property(env, opts, "samples", &sv) != napi_ok) {
      ThrowTypeError(env, "createHeightField: missing samples");
      return nullptr;
    }
    bool is_typed = false;
    napi_is_typedarray(env, sv, &is_typed);
    if (is_typed) {
      napi_typedarray_type ta_type;
      size_t byte_offset = 0, length = 0;
      void *data = nullptr;
      napi_value buf;
      if (napi_get_typedarray_info(env, sv, &ta_type, &length, &data, &buf, &byte_offset) != napi_ok || ta_type != napi_float32_array) {
        ThrowTypeError(env, "createHeightField: samples must be Float32Array or Array");
        return nullptr;
      }
      samples.assign(static_cast<float *>(data), static_cast<float *>(data) + length);
    } else {
      bool is_array = false;
      napi_is_array(env, sv, &is_array);
      if (!is_array) {
        ThrowTypeError(env, "createHeightField: samples must be Float32Array or Array");
        return nullptr;
      }
      uint32_t len = 0;
      napi_get_array_length(env, sv, &len);
      samples.reserve(len);
      for (uint32_t i = 0; i < len; ++i) {
        napi_value elem;
        napi_get_element(env, sv, i, &elem);
        double d = 0.0;
        GetDoubleArg(env, elem, &d);
        samples.push_back(static_cast<float>(d));
      }
    }
  }

  Vec3 offset = Vec3::sZero();
  Vec3 scale(1.0f, 1.0f, 1.0f);
  GetVec3ObjProp(env, opts, "offset", offset);
  GetVec3ObjProp(env, opts, "scale", scale);

  double x = 0, y = 0, z = 0;
  {
    napi_value posv;
    if (napi_get_named_property(env, opts, "position", &posv) == napi_ok) {
      napi_value xv, yv, zv;
      if (napi_get_named_property(env, posv, "x", &xv) == napi_ok) GetDoubleArg(env, xv, &x);
      if (napi_get_named_property(env, posv, "y", &yv) == napi_ok) GetDoubleArg(env, yv, &y);
      if (napi_get_named_property(env, posv, "z", &zv) == napi_ok) GetDoubleArg(env, zv, &z);
    }
  }

  double friction = 0.5, restitution = 0.0;
  {
    napi_value v;
    if (napi_get_named_property(env, opts, "friction", &v) == napi_ok) GetDoubleArg(env, v, &friction);
    if (napi_get_named_property(env, opts, "restitution", &v) == napi_ok) GetDoubleArg(env, v, &restitution);
  }

  // materialIndices — Uint8Array or Array, one per sample
  std::vector<uint8_t> mat_indices;
  {
    napi_value mi_v;
    if (napi_get_named_property(env, opts, "materialIndices", &mi_v) == napi_ok) {
      bool is_typed = false;
      bool is_arr = false;
      napi_is_typedarray(env, mi_v, &is_typed);
      napi_is_array(env, mi_v, &is_arr);
      if (is_typed) {
        napi_typedarray_type ta_type;
        size_t byte_offset = 0, length = 0;
        void *data = nullptr;
        napi_value buf;
        napi_get_typedarray_info(env, mi_v, &ta_type, &length, &data, &buf, &byte_offset);
        if (ta_type == napi_uint8_array) {
          mat_indices.assign(static_cast<uint8_t *>(data), static_cast<uint8_t *>(data) + length);
        }
      } else if (is_arr) {
        uint32_t len = 0;
        napi_get_array_length(env, mi_v, &len);
        mat_indices.reserve(len);
        for (uint32_t k = 0; k < len; ++k) {
          napi_value elem;
          napi_get_element(env, mi_v, k, &elem);
          uint32_t idx = 0;
          GetUInt32Arg(env, elem, &idx);
          mat_indices.push_back(static_cast<uint8_t>(idx));
        }
      }
    }
  }

  // materials — array of { friction, restitution }
  PhysicsMaterialList mat_list;
  {
    napi_value mats_v;
    if (napi_get_named_property(env, opts, "materials", &mats_v) == napi_ok) {
      bool is_arr = false;
      napi_is_array(env, mats_v, &is_arr);
      if (is_arr) {
        uint32_t mats_len = 0;
        napi_get_array_length(env, mats_v, &mats_len);
        for (uint32_t k = 0; k < mats_len; ++k) {
          napi_value mobj;
          napi_get_element(env, mats_v, k, &mobj);
          double mf = 0.5, mr = 0.0;
          napi_value fv, rv2;
          if (napi_get_named_property(env, mobj, "friction", &fv) == napi_ok) GetDoubleArg(env, fv, &mf);
          if (napi_get_named_property(env, mobj, "restitution", &rv2) == napi_ok) GetDoubleArg(env, rv2, &mr);
          mat_list.push_back(new IndexedMaterial(k, static_cast<float>(mf), static_cast<float>(mr)));
        }
      }
    }
  }

  const uint32_t id = handle->world->CreateHeightField(
      samples, sample_count, offset, scale, mat_indices, mat_list, x, y, z,
      static_cast<float>(friction), static_cast<float>(restitution));
  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create height field");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

// ─── Compound shapes ─────────────────────────────────────────────────────────

static bool ParseSubShapeSpecArray(napi_env env, napi_value arr, std::vector<SubShapeSpec> &out) {
  bool is_array = false;
  napi_is_array(env, arr, &is_array);
  if (!is_array) return false;
  uint32_t len = 0;
  napi_get_array_length(env, arr, &len);
  out.reserve(len);
  for (uint32_t i = 0; i < len; ++i) {
    napi_value elem;
    if (napi_get_element(env, arr, i, &elem) != napi_ok) return false;
    SubShapeSpec spec;
    if (!ParseSubShapeSpec(env, elem, spec)) return false;
    out.push_back(std::move(spec));
  }
  return true;
}

static uint32_t ParseCompoundBodyOpts(napi_env env, napi_value opts,
                                       std::vector<SubShapeSpec> &subs,
                                       double &x, double &y, double &z,
                                       bool &dynamic, float &friction, float &restitution) {
  napi_value subv;
  if (napi_get_named_property(env, opts, "shapes", &subv) != napi_ok || !ParseSubShapeSpecArray(env, subv, subs)) return 0;
  napi_value posv;
  if (napi_get_named_property(env, opts, "position", &posv) == napi_ok) {
    napi_value xv, yv, zv;
    if (napi_get_named_property(env, posv, "x", &xv) == napi_ok) GetDoubleArg(env, xv, &x);
    if (napi_get_named_property(env, posv, "y", &yv) == napi_ok) GetDoubleArg(env, yv, &y);
    if (napi_get_named_property(env, posv, "z", &zv) == napi_ok) GetDoubleArg(env, zv, &z);
  }
  napi_value dv;
  if (napi_get_named_property(env, opts, "dynamic", &dv) == napi_ok) GetBoolArg(env, dv, &dynamic);
  {
    napi_value v;
    if (napi_get_named_property(env, opts, "friction", &v) == napi_ok) { double d; if (GetDoubleArg(env, v, &d)) friction = static_cast<float>(d); }
    if (napi_get_named_property(env, opts, "restitution", &v) == napi_ok) { double d; if (GetDoubleArg(env, v, &d)) restitution = static_cast<float>(d); }
  }
  return 1;
}

napi_value CreateStaticCompound(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  std::vector<SubShapeSpec> subs;
  double x = 0, y = 0, z = 0; bool dynamic = false; float friction = 0.5f, restitution = 0.0f;
  if (!ParseCompoundBodyOpts(env, args[1], subs, x, y, z, dynamic, friction, restitution)) {
    ThrowTypeError(env, "createStaticCompound: invalid args");
    return nullptr;
  }
  const uint32_t id = handle->world->CreateStaticCompound(subs, x, y, z, dynamic, friction, restitution);
  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create static compound");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateMutableCompound(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  std::vector<SubShapeSpec> subs;
  double x = 0, y = 0, z = 0; bool dynamic = false; float friction = 0.5f, restitution = 0.0f;
  if (!ParseCompoundBodyOpts(env, args[1], subs, x, y, z, dynamic, friction, restitution)) {
    ThrowTypeError(env, "createMutableCompound: invalid args");
    return nullptr;
  }
  const uint32_t id = handle->world->CreateMutableCompound(subs, x, y, z, dynamic, friction, restitution);
  if (id == BodyID::cInvalidBodyID) {
    ThrowError(env, "Failed to create mutable compound");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value AddMutableSubShape(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t body_id;
  if (!GetUInt32Arg(env, args[1], &body_id)) {
    ThrowTypeError(env, "addMutableSubShape: invalid body_id");
    return nullptr;
  }
  SubShapeSpec spec;
  if (!ParseSubShapeSpec(env, args[2], spec)) {
    ThrowTypeError(env, "addMutableSubShape: invalid sub-shape spec");
    return nullptr;
  }
  const int32_t idx = handle->world->AddMutableSubShape(body_id, spec);
  napi_value out;
  napi_create_int32(env, idx, &out);
  return out;
}

napi_value RemoveMutableSubShape(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t body_id, index;
  if (!GetUInt32Arg(env, args[1], &body_id) || !GetUInt32Arg(env, args[2], &index)) {
    ThrowTypeError(env, "removeMutableSubShape: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->RemoveMutableSubShape(body_id, index), &out);
  return out;
}

napi_value ModifyMutableSubShape(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t body_id, index;
  if (!GetUInt32Arg(env, args[1], &body_id) || !GetUInt32Arg(env, args[2], &index)) {
    ThrowTypeError(env, "modifyMutableSubShape: invalid args");
    return nullptr;
  }
  Vec3 pos = Vec3::sZero();
  Quat rot = Quat::sIdentity();
  {
    napi_value posv = args[3];
    napi_valuetype vt = napi_undefined;
    napi_typeof(env, posv, &vt);
    if (vt == napi_object) {
      napi_value xv, yv, zv; double px = 0, py = 0, pz = 0;
      if (napi_get_named_property(env, posv, "x", &xv) == napi_ok) GetDoubleArg(env, xv, &px);
      if (napi_get_named_property(env, posv, "y", &yv) == napi_ok) GetDoubleArg(env, yv, &py);
      if (napi_get_named_property(env, posv, "z", &zv) == napi_ok) GetDoubleArg(env, zv, &pz);
      pos = Vec3(static_cast<float>(px), static_cast<float>(py), static_cast<float>(pz));
    }
  }
  {
    napi_value rotv = args[4];
    napi_valuetype vt = napi_undefined;
    napi_typeof(env, rotv, &vt);
    if (vt == napi_object) {
      napi_value xv, yv, zv, wv; double rx = 0, ry = 0, rz = 0, rw = 1;
      if (napi_get_named_property(env, rotv, "x", &xv) == napi_ok) GetDoubleArg(env, xv, &rx);
      if (napi_get_named_property(env, rotv, "y", &yv) == napi_ok) GetDoubleArg(env, yv, &ry);
      if (napi_get_named_property(env, rotv, "z", &zv) == napi_ok) GetDoubleArg(env, zv, &rz);
      if (napi_get_named_property(env, rotv, "w", &wv) == napi_ok) GetDoubleArg(env, wv, &rw);
      rot = Quat(static_cast<float>(rx), static_cast<float>(ry), static_cast<float>(rz), static_cast<float>(rw));
    }
  }
  napi_value out;
  napi_get_boolean(env, handle->world->ModifyMutableSubShape(body_id, index, pos, rot), &out);
  return out;
}

napi_value AdjustMutableCenterOfMass(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t body_id;
  if (!GetUInt32Arg(env, args[1], &body_id)) {
    ThrowTypeError(env, "adjustMutableCenterOfMass: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->AdjustMutableCenterOfMass(body_id), &out);
  return out;
}

// ─────────────────────────────────────────────────────────────────────────────

napi_value CreateSkeleton(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(1)
  uint32_t id = handle->world->CreateSkeleton();
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value AddSkeletonJoint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t skeleton_id;
  std::string name;
  int32_t parent_idx;
  if (!GetUInt32Arg(env, args[1], &skeleton_id) || !GetStringArg(env, args[2], &name) || !GetInt32Arg(env, args[3], &parent_idx)) {
    ThrowTypeError(env, "addSkeletonJoint: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->AddSkeletonJoint(skeleton_id, name, parent_idx), &out);
  return out;
}

napi_value FinalizeSkeleton(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t skeleton_id;
  if (!GetUInt32Arg(env, args[1], &skeleton_id)) {
    ThrowTypeError(env, "finalizeSkeleton: skeletonId must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->FinalizeSkeleton(skeleton_id), &out);
  return out;
}

napi_value GetSkeletonJointCount(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t skeleton_id;
  if (!GetUInt32Arg(env, args[1], &skeleton_id)) {
    ThrowTypeError(env, "getSkeletonJointCount: skeletonId must be uint32");
    return nullptr;
  }
  int count = handle->world->GetSkeletonJointCount(skeleton_id);
  if (count < 0) {
    ThrowError(env, "Skeleton not found");
    return nullptr;
  }
  napi_value out;
  napi_create_int32(env, count, &out);
  return out;
}

napi_value GetSkeletonJointInfo(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t skeleton_id;
  int32_t joint_index;
  if (!GetUInt32Arg(env, args[1], &skeleton_id) || !GetInt32Arg(env, args[2], &joint_index)) {
    ThrowTypeError(env, "getSkeletonJointInfo: invalid args");
    return nullptr;
  }

  std::string name;
  int32_t parent_index = -1;
  if (!handle->world->GetSkeletonJointInfo(skeleton_id, joint_index, name, parent_index)) {
    ThrowError(env, "Skeleton or joint not found");
    return nullptr;
  }

  napi_value out;
  napi_create_object(env, &out);
  napi_value name_v;
  napi_value parent_v;
  napi_create_string_utf8(env, name.c_str(), name.size(), &name_v);
  napi_create_int32(env, parent_index, &parent_v);
  napi_set_named_property(env, out, "name", name_v);
  napi_set_named_property(env, out, "parentIndex", parent_v);
  return out;
}

napi_value GetSkeletonJointIndex(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t skeleton_id;
  std::string name;
  if (!GetUInt32Arg(env, args[1], &skeleton_id) || !GetStringArg(env, args[2], &name)) {
    ThrowTypeError(env, "getSkeletonJointIndex: invalid args");
    return nullptr;
  }

  int idx = handle->world->GetSkeletonJointIndex(skeleton_id, name);
  if (idx < 0) {
    ThrowError(env, "Skeleton or joint not found");
    return nullptr;
  }
  napi_value out;
  napi_create_int32(env, idx, &out);
  return out;
}

napi_value CreateRagdollSettings(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t skeleton_id;
  double half_h, radius, spacing;
  if (!GetUInt32Arg(env, args[1], &skeleton_id) || !GetDoubleArg(env, args[2], &half_h) || !GetDoubleArg(env, args[3], &radius) ||
      !GetDoubleArg(env, args[4], &spacing) || half_h < 0 || radius <= 0 || spacing < 0) {
    ThrowTypeError(env, "createRagdollSettings: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateRagdollSettings(skeleton_id, static_cast<float>(half_h), static_cast<float>(radius), static_cast<float>(spacing));
  if (id == 0) {
    ThrowError(env, "Failed to create ragdoll settings");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value CreateRagdoll(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t settings_id;
  uint32_t group_id;
  uint32_t user_lo;
  bool activate;
  if (!GetUInt32Arg(env, args[1], &settings_id) || !GetUInt32Arg(env, args[2], &group_id) || !GetUInt32Arg(env, args[3], &user_lo) ||
      !GetBoolArg(env, args[4], &activate)) {
    ThrowTypeError(env, "createRagdoll: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateRagdoll(settings_id, group_id, static_cast<uint64_t>(user_lo), activate);
  if (id == 0) {
    ThrowError(env, "Failed to create ragdoll");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, id, &out);
  return out;
}

napi_value DestroyRagdoll(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "destroyRagdoll: ragdollId must be uint32");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->DestroyRagdoll(ragdoll_id), &out);
  return out;
}

napi_value GetRagdollBodyCount(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "getRagdollBodyCount: ragdollId must be uint32");
    return nullptr;
  }
  int count = handle->world->GetRagdollBodyCount(ragdoll_id);
  if (count < 0) {
    ThrowError(env, "Ragdoll not found");
    return nullptr;
  }
  napi_value out;
  napi_create_int32(env, count, &out);
  return out;
}

napi_value GetRagdollBoneBodyId(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t ragdoll_id;
  int32_t index;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetInt32Arg(env, args[2], &index)) {
    ThrowTypeError(env, "getRagdollBoneBodyId: invalid args");
    return nullptr;
  }
  uint32_t body_id = 0;
  if (!handle->world->GetRagdollBoneBodyId(ragdoll_id, index, body_id)) {
    ThrowError(env, "Ragdoll or bone not found");
    return nullptr;
  }
  napi_value out;
  napi_create_uint32(env, body_id, &out);
  return out;
}

napi_value GetRagdollBoneTransform(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t ragdoll_id;
  int32_t index;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetInt32Arg(env, args[2], &index)) {
    ThrowTypeError(env, "getRagdollBoneTransform: invalid args");
    return nullptr;
  }

  RVec3 pos;
  Quat rot;
  if (!handle->world->GetRagdollBoneTransform(ragdoll_id, index, pos, rot)) {
    ThrowError(env, "Failed to get ragdoll bone transform");
    return nullptr;
  }

  napi_value out;
  napi_create_object(env, &out);
  napi_value p = MakeVec3Object(env, pos);
  napi_value q = MakeQuatObject(env, rot);
  napi_set_named_property(env, out, "position", p);
  napi_set_named_property(env, out, "rotation", q);
  return out;
}

napi_value SetRagdollBoneTransform(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  uint32_t ragdoll_id;
  int32_t index;
  double px, py, pz, qx, qy, qz, qw;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetInt32Arg(env, args[2], &index) || !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &qx) || !GetDoubleArg(env, args[7], &qy) || !GetDoubleArg(env, args[8], &qz) ||
      !GetDoubleArg(env, args[9], &qw)) {
    ThrowTypeError(env, "setRagdollBoneTransform: invalid args");
    return nullptr;
  }

  bool ok = handle->world->SetRagdollBoneTransform(
      ragdoll_id,
      index,
      RVec3(px, py, pz),
      Quat(static_cast<float>(qx), static_cast<float>(qy), static_cast<float>(qz), static_cast<float>(qw)),
      true);

  napi_value out;
  napi_get_boolean(env, ok, &out);
  return out;
}

napi_value SetRagdollJointShape(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t settings_id;
  int32_t joint_index;
  if (!GetUInt32Arg(env, args[1], &settings_id) || !GetInt32Arg(env, args[2], &joint_index)) {
    ThrowTypeError(env, "setRagdollJointShape: invalid args");
    return nullptr;
  }
  napi_valuetype vt = napi_undefined;
  if (napi_typeof(env, args[3], &vt) != napi_ok || vt != napi_object) {
    ThrowTypeError(env, "setRagdollJointShape: config must be object");
    return nullptr;
  }
  int kind = 0;
  float half_h = 0.2f, radius = 0.1f, hx = 0.1f, hy = 0.2f, hz = 0.1f;
  napi_value kind_v;
  if (napi_get_named_property(env, args[3], "kind", &kind_v) == napi_ok) {
    std::string ks;
    if (GetStringArg(env, kind_v, &ks)) {
      if (ks == "box") kind = 1;
      else if (ks == "sphere") kind = 2;
    }
  }
  auto getF3 = [&](const char *key, float &dest) {
    napi_value v;
    if (napi_get_named_property(env, args[3], key, &v) == napi_ok) {
      double d = 0.0;
      if (GetDoubleArg(env, v, &d)) dest = static_cast<float>(d);
    }
  };
  getF3("halfHeight", half_h);
  getF3("radius", radius);
  napi_value he_v;
  if (napi_get_named_property(env, args[3], "halfExtents", &he_v) == napi_ok) {
    napi_valuetype hevt = napi_undefined;
    if (napi_typeof(env, he_v, &hevt) == napi_ok && hevt == napi_object) {
      napi_value xv, yv, zv;
      double dx = 0, dy = 0, dz = 0;
      if (napi_get_named_property(env, he_v, "x", &xv) == napi_ok) GetDoubleArg(env, xv, &dx);
      if (napi_get_named_property(env, he_v, "y", &yv) == napi_ok) GetDoubleArg(env, yv, &dy);
      if (napi_get_named_property(env, he_v, "z", &zv) == napi_ok) GetDoubleArg(env, zv, &dz);
      hx = static_cast<float>(dx); hy = static_cast<float>(dy); hz = static_cast<float>(dz);
    }
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetRagdollJointShape(settings_id, joint_index, kind, half_h, radius, hx, hy, hz), &out);
  return out;
}

napi_value SetRagdollJointTransform(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  uint32_t settings_id;
  int32_t joint_index;
  double px, py, pz, qx, qy, qz, qw;
  if (!GetUInt32Arg(env, args[1], &settings_id) || !GetInt32Arg(env, args[2], &joint_index) ||
      !GetDoubleArg(env, args[3], &px) || !GetDoubleArg(env, args[4], &py) ||
      !GetDoubleArg(env, args[5], &pz) || !GetDoubleArg(env, args[6], &qx) ||
      !GetDoubleArg(env, args[7], &qy) || !GetDoubleArg(env, args[8], &qz) ||
      !GetDoubleArg(env, args[9], &qw)) {
    ThrowTypeError(env, "setRagdollJointTransform: invalid args");
    return nullptr;
  }
  napi_value out;
  napi_get_boolean(env, handle->world->SetRagdollJointTransform(
      settings_id, joint_index,
      RVec3(px, py, pz),
      Quat(static_cast<float>(qx), static_cast<float>(qy), static_cast<float>(qz), static_cast<float>(qw))), &out);
  return out;
}

napi_value SetRagdollJointConstraint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(4)
  uint32_t settings_id;
  int32_t joint_index;
  if (!GetUInt32Arg(env, args[1], &settings_id) || !GetInt32Arg(env, args[2], &joint_index)) {
    ThrowTypeError(env, "setRagdollJointConstraint: invalid args");
    return nullptr;
  }
  RagdollJointConstraintConfig cfg;
  ParseRagdollJointConstraintConfig(env, args[3], cfg);
  napi_value out;
  napi_get_boolean(env, handle->world->SetRagdollJointConstraint(settings_id, joint_index, cfg), &out);
  return out;
}

napi_value GetRagdollConstraintIds(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "getRagdollConstraintIds: invalid args");
    return nullptr;
  }
  std::vector<uint32_t> ids;
  if (!handle->world->GetRagdollConstraintIds(ragdoll_id, ids)) {
    ThrowError(env, "Ragdoll not found");
    return nullptr;
  }
  napi_value arr;
  napi_create_array_with_length(env, ids.size(), &arr);
  for (size_t i = 0; i < ids.size(); ++i) {
    napi_value v;
    napi_create_uint32(env, ids[i], &v);
    napi_set_element(env, arr, static_cast<uint32_t>(i), v);
  }
  return arr;
}

// --- SkeletonPose NAPI ---

napi_value CreateSkeletonPose(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "createSkeletonPose: expected ragdoll id"); return nullptr;
  }
  uint32_t id = handle->world->CreateSkeletonPose(ragdoll_id);
  napi_value result;
  napi_create_uint32(env, id, &result);
  return result;
}

napi_value DestroySkeletonPose(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t pose_id;
  if (!GetUInt32Arg(env, args[1], &pose_id)) {
    ThrowTypeError(env, "destroySkeletonPose: expected pose id"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->DestroySkeletonPose(pose_id), &result);
  return result;
}

napi_value SetPoseJoint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(10)
  uint32_t pose_id; int32_t ji;
  double tx, ty, tz, rx, ry, rz, rw;
  if (!GetUInt32Arg(env, args[1], &pose_id) || !GetInt32Arg(env, args[2], &ji) ||
      !GetDoubleArg(env, args[3], &tx) || !GetDoubleArg(env, args[4], &ty) ||
      !GetDoubleArg(env, args[5], &tz) || !GetDoubleArg(env, args[6], &rx) ||
      !GetDoubleArg(env, args[7], &ry) || !GetDoubleArg(env, args[8], &rz) ||
      !GetDoubleArg(env, args[9], &rw)) {
    ThrowTypeError(env, "setPoseJoint: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->SetPoseJoint(pose_id, ji,
    (float)tx, (float)ty, (float)tz, (float)rx, (float)ry, (float)rz, (float)rw), &result);
  return result;
}

napi_value GetPoseJoint(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t pose_id; int32_t ji;
  if (!GetUInt32Arg(env, args[1], &pose_id) || !GetInt32Arg(env, args[2], &ji)) {
    ThrowTypeError(env, "getPoseJoint: invalid args"); return nullptr;
  }
  Vec3 t; Quat r;
  if (!handle->world->GetPoseJoint(pose_id, ji, t, r)) return nullptr;
  napi_value obj, trans, rot;
  napi_create_object(env, &obj);
  napi_create_object(env, &trans);
  napi_create_object(env, &rot);
  SetF64Prop(env, trans, "x", t.GetX()); SetF64Prop(env, trans, "y", t.GetY()); SetF64Prop(env, trans, "z", t.GetZ());
  SetF64Prop(env, rot, "x", r.GetX()); SetF64Prop(env, rot, "y", r.GetY()); SetF64Prop(env, rot, "z", r.GetZ()); SetF64Prop(env, rot, "w", r.GetW());
  napi_set_named_property(env, obj, "translation", trans);
  napi_set_named_property(env, obj, "rotation", rot);
  return obj;
}

napi_value SetPoseRootOffset(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  uint32_t pose_id;
  double x, y, z;
  if (!GetUInt32Arg(env, args[1], &pose_id) ||
      !GetDoubleArg(env, args[2], &x) || !GetDoubleArg(env, args[3], &y) ||
      !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "setPoseRootOffset: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->SetPoseRootOffset(pose_id, x, y, z), &result);
  return result;
}

napi_value GetPoseRootOffset(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t pose_id;
  if (!GetUInt32Arg(env, args[1], &pose_id)) {
    ThrowTypeError(env, "getPoseRootOffset: invalid args"); return nullptr;
  }
  RVec3 out;
  if (!handle->world->GetPoseRootOffset(pose_id, out)) return nullptr;
  napi_value obj;
  napi_create_object(env, &obj);
  SetF64Prop(env, obj, "x", (double)out.GetX());
  SetF64Prop(env, obj, "y", (double)out.GetY());
  SetF64Prop(env, obj, "z", (double)out.GetZ());
  return obj;
}

napi_value CalculatePoseJointMatrices(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t pose_id;
  if (!GetUInt32Arg(env, args[1], &pose_id)) {
    ThrowTypeError(env, "calculatePoseJointMatrices: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->CalculatePoseJointMatrices(pose_id), &result);
  return result;
}

napi_value GetPoseJointCount(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t pose_id;
  if (!GetUInt32Arg(env, args[1], &pose_id)) {
    ThrowTypeError(env, "getPoseJointCount: invalid args"); return nullptr;
  }
  napi_value result;
  napi_create_int32(env, handle->world->GetPoseJointCount(pose_id), &result);
  return result;
}

// --- Extended Ragdoll NAPI ---

napi_value RagdollSetPose(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(3, 4)
  uint32_t ragdoll_id, pose_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetUInt32Arg(env, args[2], &pose_id)) {
    ThrowTypeError(env, "ragdollSetPose: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 4) { bool b; if (GetBoolArg(env, args[3], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollSetPose(ragdoll_id, pose_id, lock), &result);
  return result;
}

napi_value RagdollGetPose(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(3, 4)
  uint32_t ragdoll_id, pose_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetUInt32Arg(env, args[2], &pose_id)) {
    ThrowTypeError(env, "ragdollGetPose: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 4) { bool b; if (GetBoolArg(env, args[3], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollGetPose(ragdoll_id, pose_id, lock), &result);
  return result;
}

napi_value RagdollDriveToPoseKinematics(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(4, 5)
  uint32_t ragdoll_id, pose_id;
  double dt;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetUInt32Arg(env, args[2], &pose_id) ||
      !GetDoubleArg(env, args[3], &dt)) {
    ThrowTypeError(env, "ragdollDriveToPoseKinematics: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 5) { bool b; if (GetBoolArg(env, args[4], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollDriveToPoseKinematics(ragdoll_id, pose_id, (float)dt, lock), &result);
  return result;
}

napi_value RagdollDriveToPoseMotors(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  uint32_t ragdoll_id, pose_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetUInt32Arg(env, args[2], &pose_id)) {
    ThrowTypeError(env, "ragdollDriveToPoseMotors: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollDriveToPoseMotors(ragdoll_id, pose_id), &result);
  return result;
}

napi_value RagdollActivate(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(2, 3)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollActivate: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 3) { bool b; if (GetBoolArg(env, args[2], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollActivate(ragdoll_id, lock), &result);
  return result;
}

napi_value RagdollIsActive(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollIsActive: invalid args"); return nullptr;
  }
  napi_value result;
  napi_create_int32(env, handle->world->RagdollIsActive(ragdoll_id), &result);
  return result;
}

napi_value RagdollGetRootTransform(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollGetRootTransform: invalid args"); return nullptr;
  }
  RVec3 pos; Quat rot;
  if (!handle->world->RagdollGetRootTransform(ragdoll_id, pos, rot)) return nullptr;
  napi_value obj, pobj, robj;
  napi_create_object(env, &obj);
  napi_create_object(env, &pobj);
  napi_create_object(env, &robj);
  SetF64Prop(env, pobj, "x", (double)pos.GetX()); SetF64Prop(env, pobj, "y", (double)pos.GetY()); SetF64Prop(env, pobj, "z", (double)pos.GetZ());
  SetF64Prop(env, robj, "x", rot.GetX()); SetF64Prop(env, robj, "y", rot.GetY()); SetF64Prop(env, robj, "z", rot.GetZ()); SetF64Prop(env, robj, "w", rot.GetW());
  napi_set_named_property(env, obj, "position", pobj);
  napi_set_named_property(env, obj, "rotation", robj);
  return obj;
}

napi_value RagdollGetWorldSpaceBounds(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollGetWorldSpaceBounds: invalid args"); return nullptr;
  }
  Vec3 bmin, bmax;
  if (!handle->world->RagdollGetWorldSpaceBounds(ragdoll_id, bmin, bmax)) return nullptr;
  napi_value obj, minobj, maxobj;
  napi_create_object(env, &obj);
  napi_create_object(env, &minobj);
  napi_create_object(env, &maxobj);
  SetF64Prop(env, minobj, "x", bmin.GetX()); SetF64Prop(env, minobj, "y", bmin.GetY()); SetF64Prop(env, minobj, "z", bmin.GetZ());
  SetF64Prop(env, maxobj, "x", bmax.GetX()); SetF64Prop(env, maxobj, "y", bmax.GetY()); SetF64Prop(env, maxobj, "z", bmax.GetZ());
  napi_set_named_property(env, obj, "min", minobj);
  napi_set_named_property(env, obj, "max", maxobj);
  return obj;
}

napi_value RagdollSetGroupID(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(3, 4)
  uint32_t ragdoll_id, group_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) || !GetUInt32Arg(env, args[2], &group_id)) {
    ThrowTypeError(env, "ragdollSetGroupID: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 4) { bool b; if (GetBoolArg(env, args[3], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollSetGroupID(ragdoll_id, group_id, lock), &result);
  return result;
}

napi_value RagdollResetWarmStart(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollResetWarmStart: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollResetWarmStart(ragdoll_id), &result);
  return result;
}

napi_value RagdollSetLinearVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(5, 6)
  uint32_t ragdoll_id;
  double vx, vy, vz;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) ||
      !GetDoubleArg(env, args[2], &vx) || !GetDoubleArg(env, args[3], &vy) ||
      !GetDoubleArg(env, args[4], &vz)) {
    ThrowTypeError(env, "ragdollSetLinearVelocity: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 6) { bool b; if (GetBoolArg(env, args[5], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollSetLinearVelocity(ragdoll_id, (float)vx, (float)vy, (float)vz, lock), &result);
  return result;
}

napi_value RagdollAddLinearVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(5, 6)
  uint32_t ragdoll_id;
  double vx, vy, vz;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) ||
      !GetDoubleArg(env, args[2], &vx) || !GetDoubleArg(env, args[3], &vy) ||
      !GetDoubleArg(env, args[4], &vz)) {
    ThrowTypeError(env, "ragdollAddLinearVelocity: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 6) { bool b; if (GetBoolArg(env, args[5], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollAddLinearVelocity(ragdoll_id, (float)vx, (float)vy, (float)vz, lock), &result);
  return result;
}

napi_value RagdollSetLinearAndAngularVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(8, 9)
  uint32_t ragdoll_id;
  double lvx, lvy, lvz, avx, avy, avz;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) ||
      !GetDoubleArg(env, args[2], &lvx) || !GetDoubleArg(env, args[3], &lvy) ||
      !GetDoubleArg(env, args[4], &lvz) || !GetDoubleArg(env, args[5], &avx) ||
      !GetDoubleArg(env, args[6], &avy) || !GetDoubleArg(env, args[7], &avz)) {
    ThrowTypeError(env, "ragdollSetLinearAndAngularVelocity: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 9) { bool b; if (GetBoolArg(env, args[8], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollSetLinearAndAngularVelocity(ragdoll_id,
    (float)lvx, (float)lvy, (float)lvz, (float)avx, (float)avy, (float)avz, lock), &result);
  return result;
}

napi_value RagdollAddImpulse(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(5, 6)
  uint32_t ragdoll_id;
  double ix, iy, iz;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id) ||
      !GetDoubleArg(env, args[2], &ix) || !GetDoubleArg(env, args[3], &iy) ||
      !GetDoubleArg(env, args[4], &iz)) {
    ThrowTypeError(env, "ragdollAddImpulse: invalid args"); return nullptr;
  }
  bool lock = true;
  if (argc >= 6) { bool b; if (GetBoolArg(env, args[5], &b)) lock = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollAddImpulse(ragdoll_id, (float)ix, (float)iy, (float)iz, lock), &result);
  return result;
}

napi_value RagdollAddToPhysicsSystem(napi_env env, napi_callback_info info) {
  WORLD_FN_OPT(2, 3)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollAddToPhysicsSystem: invalid args"); return nullptr;
  }
  bool activate = true;
  if (argc >= 3) { bool b; if (GetBoolArg(env, args[2], &b)) activate = b; }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollAddToPhysicsSystem(ragdoll_id, activate), &result);
  return result;
}

napi_value RagdollRemoveFromPhysicsSystem(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollRemoveFromPhysicsSystem: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollRemoveFromPhysicsSystem(ragdoll_id), &result);
  return result;
}

napi_value RagdollStabilize(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t ragdoll_id;
  if (!GetUInt32Arg(env, args[1], &ragdoll_id)) {
    ThrowTypeError(env, "ragdollStabilize: invalid args"); return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, handle->world->RagdollStabilize(ragdoll_id), &result);
  return result;
}

napi_value SnapshotState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(1)
  std::vector<uint8_t> data = handle->world->SnapshotState();
  napi_value buf = nullptr;
  void *ptr = nullptr;
  napi_create_buffer(env, data.size(), &ptr, &buf);
  if (!data.empty() && ptr) memcpy(ptr, data.data(), data.size());
  return buf;
}

napi_value ApplySnapshot(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  bool is_buf = false;
  napi_is_buffer(env, args[1], &is_buf);
  if (!is_buf) { ThrowTypeError(env, "applySnapshot: expected Buffer"); return nullptr; }
  void *data = nullptr; size_t len = 0;
  napi_get_buffer_info(env, args[1], &data, &len);
  napi_value result;
  napi_get_boolean(env, handle->world->ApplySnapshot(static_cast<const uint8_t *>(data), len), &result);
  return result;
}

napi_value SaveScene(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(1)
  std::vector<uint8_t> data = handle->world->SaveScene();
  napi_value buf = nullptr;
  void *ptr = nullptr;
  napi_create_buffer(env, data.size(), &ptr, &buf);
  if (!data.empty() && ptr) memcpy(ptr, data.data(), data.size());
  return buf;
}

napi_value LoadScene(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  bool is_buf = false;
  napi_is_buffer(env, args[1], &is_buf);
  if (!is_buf) { ThrowTypeError(env, "loadScene: expected Buffer"); return nullptr; }
  void *data = nullptr; size_t len = 0;
  napi_get_buffer_info(env, args[1], &data, &len);
  int count = handle->world->LoadScene(static_cast<const uint8_t *>(data), len);
  if (count < 0) { ThrowError(env, "loadScene: failed to restore scene"); return nullptr; }
  napi_value result;
  napi_create_int32(env, count, &result);
  return result;
}

// ── CharacterVirtual NAPI wrappers ────────────────────────────────────────

napi_value CreateCharacter(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(9)
  double half_height, radius, x, y, z, mass, max_strength, max_slope_angle;
  if (!GetDoubleArg(env, args[1], &half_height) || !GetDoubleArg(env, args[2], &radius) ||
      !GetDoubleArg(env, args[3], &x) || !GetDoubleArg(env, args[4], &y) || !GetDoubleArg(env, args[5], &z) ||
      !GetDoubleArg(env, args[6], &mass) || !GetDoubleArg(env, args[7], &max_strength) ||
      !GetDoubleArg(env, args[8], &max_slope_angle)) {
    ThrowTypeError(env, "createCharacter: invalid args");
    return nullptr;
  }
  uint32_t id = handle->world->CreateCharacter(
      static_cast<float>(half_height), static_cast<float>(radius),
      x, y, z,
      static_cast<float>(mass), static_cast<float>(max_strength),
      static_cast<float>(max_slope_angle));
  napi_value result;
  napi_create_uint32(env, id, &result);
  return result;
}

napi_value DestroyCharacter(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  uint32_t id = 0;
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "destroyCharacter: invalid id"); return nullptr; }
  id = static_cast<uint32_t>(d);
  bool ok = handle->world->DestroyCharacter(id);
  napi_value result; napi_get_boolean(env, ok, &result);
  return result;
}

napi_value CharacterUpdate(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(3)
  double d, dt;
  if (!GetDoubleArg(env, args[1], &d) || !GetDoubleArg(env, args[2], &dt)) {
    ThrowTypeError(env, "characterUpdate: invalid args"); return nullptr;
  }
  bool ok = handle->world->CharacterUpdate(static_cast<uint32_t>(d), static_cast<float>(dt));
  napi_value result; napi_get_boolean(env, ok, &result);
  return result;
}

napi_value SetCharacterLinearVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  double d, vx, vy, vz;
  if (!GetDoubleArg(env, args[1], &d) || !GetDoubleArg(env, args[2], &vx) ||
      !GetDoubleArg(env, args[3], &vy) || !GetDoubleArg(env, args[4], &vz)) {
    ThrowTypeError(env, "setCharacterLinearVelocity: invalid args"); return nullptr;
  }
  bool ok = handle->world->SetCharacterLinearVelocity(
      static_cast<uint32_t>(d), static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz));
  napi_value result; napi_get_boolean(env, ok, &result);
  return result;
}

napi_value GetCharacterLinearVelocity(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "getCharacterLinearVelocity: invalid id"); return nullptr; }
  Vec3 vel;
  if (!handle->world->GetCharacterLinearVelocity(static_cast<uint32_t>(d), vel)) return nullptr;
  return MakeVec3Object(env, RVec3(vel.GetX(), vel.GetY(), vel.GetZ()));
}

napi_value SetCharacterPosition(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(5)
  double d, x, y, z;
  if (!GetDoubleArg(env, args[1], &d) || !GetDoubleArg(env, args[2], &x) ||
      !GetDoubleArg(env, args[3], &y) || !GetDoubleArg(env, args[4], &z)) {
    ThrowTypeError(env, "setCharacterPosition: invalid args"); return nullptr;
  }
  bool ok = handle->world->SetCharacterPosition(static_cast<uint32_t>(d), x, y, z);
  napi_value result; napi_get_boolean(env, ok, &result);
  return result;
}

napi_value GetCharacterPosition(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "getCharacterPosition: invalid id"); return nullptr; }
  RVec3 pos;
  if (!handle->world->GetCharacterPosition(static_cast<uint32_t>(d), pos)) return nullptr;
  return MakeVec3Object(env, pos);
}

napi_value SetCharacterRotation(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(6)
  double d, rx, ry, rz, rw;
  if (!GetDoubleArg(env, args[1], &d) || !GetDoubleArg(env, args[2], &rx) ||
      !GetDoubleArg(env, args[3], &ry) || !GetDoubleArg(env, args[4], &rz) ||
      !GetDoubleArg(env, args[5], &rw)) {
    ThrowTypeError(env, "setCharacterRotation: invalid args"); return nullptr;
  }
  bool ok = handle->world->SetCharacterRotation(
      static_cast<uint32_t>(d), static_cast<float>(rx), static_cast<float>(ry),
      static_cast<float>(rz), static_cast<float>(rw));
  napi_value result; napi_get_boolean(env, ok, &result);
  return result;
}

napi_value GetCharacterRotation(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "getCharacterRotation: invalid id"); return nullptr; }
  Quat rot;
  if (!handle->world->GetCharacterRotation(static_cast<uint32_t>(d), rot)) return nullptr;
  return MakeQuatObject(env, rot);
}

napi_value GetCharacterGroundState(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "getCharacterGroundState: invalid id"); return nullptr; }
  int state = handle->world->GetCharacterGroundState(static_cast<uint32_t>(d));
  napi_value result; napi_create_int32(env, state, &result);
  return result;
}

napi_value GetCharacterGroundNormal(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "getCharacterGroundNormal: invalid id"); return nullptr; }
  Vec3 normal;
  if (!handle->world->GetCharacterGroundNormal(static_cast<uint32_t>(d), normal)) return nullptr;
  return MakeVec3Object(env, RVec3(normal.GetX(), normal.GetY(), normal.GetZ()));
}

napi_value GetCharacterGroundBodyId(napi_env env, napi_callback_info info) {
  WORLD_FN_BEGIN(2)
  double d; if (!GetDoubleArg(env, args[1], &d)) { ThrowTypeError(env, "getCharacterGroundBodyId: invalid id"); return nullptr; }
  uint32_t body_id = handle->world->GetCharacterGroundBodyId(static_cast<uint32_t>(d));
  napi_value result; napi_create_uint32(env, body_id, &result);
  return result;
}


