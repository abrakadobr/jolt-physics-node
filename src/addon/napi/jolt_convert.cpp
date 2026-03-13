#include "jolt_convert.h"

namespace JOLT {

// ─── helpers ────────────────────────────────────────────────────────────────

static float GetFloatProp(napi_env env, napi_value obj, const char* key) {
    napi_value v;
    napi_get_named_property(env, obj, key, &v);
    double d = 0.0;
    napi_get_value_double(env, v, &d);
    return static_cast<float>(d);
}

static napi_value F(napi_env env, double v) {
    napi_value r;
    napi_create_double(env, v, &r);
    return r;
}

// ─── BodyID ────────────────────────────────────────────────────────────────────

JPH::BodyID JsConvert<JPH::BodyID>::from(napi_env env, napi_value v) {
  uint32_t vv;
  if (napi_get_value_uint32(env, v, &vv) == napi_ok)
    return JPH::BodyID(vv);
  return JPH::BodyID(0xffffffff); // invalid body id
}

napi_value JsConvert<JPH::BodyID>::to(napi_env env, JPH::BodyID v) {
    napi_value r;
    napi_create_uint32(env, v.GetIndexAndSequenceNumber(), &r);
    return r;
}

// ─── Vec3 ────────────────────────────────────────────────────────────────────

JPH::Vec3 JsConvert<JPH::Vec3>::from(napi_env env, napi_value v) {
    return JPH::Vec3(
        GetFloatProp(env, v, "x"),
        GetFloatProp(env, v, "y"),
        GetFloatProp(env, v, "z")
    );
}

napi_value JsConvert<JPH::Vec3>::to(napi_env env, JPH::Vec3 v) {
    napi_value obj;
    napi_create_object(env, &obj);
    napi_set_named_property(env, obj, "x", F(env, v.GetX()));
    napi_set_named_property(env, obj, "y", F(env, v.GetY()));
    napi_set_named_property(env, obj, "z", F(env, v.GetZ()));
    return obj;
}

// ─── Vec4 ────────────────────────────────────────────────────────────────────

JPH::Vec4 JsConvert<JPH::Vec4>::from(napi_env env, napi_value v) {
    return JPH::Vec4(
        GetFloatProp(env, v, "x"),
        GetFloatProp(env, v, "y"),
        GetFloatProp(env, v, "z"),
        GetFloatProp(env, v, "w")
    );
}

napi_value JsConvert<JPH::Vec4>::to(napi_env env, JPH::Vec4 v) {
    napi_value obj;
    napi_create_object(env, &obj);
    napi_set_named_property(env, obj, "x", F(env, v.GetX()));
    napi_set_named_property(env, obj, "y", F(env, v.GetY()));
    napi_set_named_property(env, obj, "z", F(env, v.GetZ()));
    napi_set_named_property(env, obj, "w", F(env, v.GetW()));
    return obj;
}

// ─── Quat ────────────────────────────────────────────────────────────────────

JPH::Quat JsConvert<JPH::Quat>::from(napi_env env, napi_value v) {
    return JPH::Quat(
        GetFloatProp(env, v, "x"),
        GetFloatProp(env, v, "y"),
        GetFloatProp(env, v, "z"),
        GetFloatProp(env, v, "w")
    );
}

napi_value JsConvert<JPH::Quat>::to(napi_env env, JPH::Quat v) {
    napi_value obj;
    napi_create_object(env, &obj);
    napi_set_named_property(env, obj, "x", F(env, v.GetX()));
    napi_set_named_property(env, obj, "y", F(env, v.GetY()));
    napi_set_named_property(env, obj, "z", F(env, v.GetZ()));
    napi_set_named_property(env, obj, "w", F(env, v.GetW()));
    return obj;
}

// ─── Mat44 ── Float32Array(16), column-major ─────────────────────────────────

JPH::Mat44 JsConvert<JPH::Mat44>::from(napi_env env, napi_value v) {
    void* data;
    size_t len;
    napi_value arraybuf;
    size_t offset;
    napi_get_typedarray_info(env, v, nullptr, &len, &data, &arraybuf, &offset);
    auto* f = static_cast<float*>(data);
    // column-major: f[col*4 + row]
    return JPH::Mat44(
        JPH::Vec4(f[0],  f[1],  f[2],  f[3]),   // col 0
        JPH::Vec4(f[4],  f[5],  f[6],  f[7]),   // col 1
        JPH::Vec4(f[8],  f[9],  f[10], f[11]),  // col 2
        JPH::Vec4(f[12], f[13], f[14], f[15])   // col 3
    );
}

napi_value JsConvert<JPH::Mat44>::to(napi_env env, JPH::Mat44 v) {
    void* raw;
    napi_value arraybuf;
    napi_create_arraybuffer(env, 16 * sizeof(float), &raw, &arraybuf);
    auto* f = static_cast<float*>(raw);
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            f[col * 4 + row] = v(row, col);
    napi_value result;
    napi_create_typedarray(env, napi_float32_array, 16, arraybuf, 0, &result);
    return result;
}

// ─── RayCastResult ───────────────────────────────────────────────────────────

napi_value JsConvert<JPH::RayCastResult>::to(napi_env env, const JPH::RayCastResult& v) {
    napi_value obj;
    napi_create_object(env, &obj);
    napi_set_named_property(env, obj, "bodyId",
        JsConvert<uint32_t>::to(env, v.mBodyID.GetIndexAndSequenceNumber()));
    napi_set_named_property(env, obj, "fraction",
        F(env, v.mFraction));
    napi_set_named_property(env, obj, "subShapeId",
        JsConvert<uint32_t>::to(env, v.mSubShapeID2.GetValue()));
    return obj;
}

// ─── CollideShapeResult ──────────────────────────────────────────────────────

napi_value JsConvert<JPH::CollideShapeResult>::to(napi_env env, const JPH::CollideShapeResult& v) {
    napi_value obj;
    napi_create_object(env, &obj);
    napi_set_named_property(env, obj, "contactPointOn1",
        JsConvert<JPH::Vec3>::to(env, v.mContactPointOn1));
    napi_set_named_property(env, obj, "contactPointOn2",
        JsConvert<JPH::Vec3>::to(env, v.mContactPointOn2));
    napi_set_named_property(env, obj, "penetrationAxis",
        JsConvert<JPH::Vec3>::to(env, v.mPenetrationAxis));
    napi_set_named_property(env, obj, "penetrationDepth",
        F(env, v.mPenetrationDepth));
    napi_set_named_property(env, obj, "subShapeId1",
        JsConvert<uint32_t>::to(env, v.mSubShapeID1.GetValue()));
    napi_set_named_property(env, obj, "subShapeId2",
        JsConvert<uint32_t>::to(env, v.mSubShapeID2.GetValue()));
    napi_set_named_property(env, obj, "bodyId",
        JsConvert<uint32_t>::to(env, v.mBodyID2.GetIndexAndSequenceNumber()));
    return obj;
}

// ─── RVec3 (double precision only) ───────────────────────────────────────────

#ifdef JPH_DOUBLE_PRECISION
JPH::RVec3 JsConvert<JPH::RVec3>::from(napi_env env, napi_value v) {
    napi_value vx, vy, vz;
    napi_get_named_property(env, v, "x", &vx);
    napi_get_named_property(env, v, "y", &vy);
    napi_get_named_property(env, v, "z", &vz);
    double x = 0.0, y = 0.0, z = 0.0;
    napi_get_value_double(env, vx, &x);
    napi_get_value_double(env, vy, &y);
    napi_get_value_double(env, vz, &z);
    return JPH::RVec3(x, y, z);
}

napi_value JsConvert<JPH::RVec3>::to(napi_env env, JPH::RVec3 v) {
    napi_value obj;
    napi_create_object(env, &obj);
    napi_set_named_property(env, obj, "x", F(env, v.GetX()));
    napi_set_named_property(env, obj, "y", F(env, v.GetY()));
    napi_set_named_property(env, obj, "z", F(env, v.GetZ()));
    return obj;
}
#endif

}
