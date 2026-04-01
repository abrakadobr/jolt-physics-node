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

/*
JPH::Mat44 JsConvert<JPH::Mat44>::from(napi_env env, napi_value v) {
    float f[4][4];
    for (int row = 0; row < 4; ++row) {
        napi_value rowArr;
        napi_get_element(env, v, row, &rowArr);
        for (int col = 0; col < 4; ++col) {
            napi_value elem;
            napi_get_element(env, rowArr, col, &elem);
            double d;
            napi_get_value_double(env, elem, &d);
            f[row][col] = static_cast<float>(d);
        }
    }
    // JPH::Mat44 ctor takes columns: Vec4(col0_row0..3), Vec4(col1_row0..3), ...
    return JPH::Mat44(
        JPH::Vec4(f[0][0], f[1][0], f[2][0], f[3][0]),
        JPH::Vec4(f[0][1], f[1][1], f[2][1], f[3][1]),
        JPH::Vec4(f[0][2], f[1][2], f[2][2], f[3][2]),
        JPH::Vec4(f[0][3], f[1][3], f[2][3], f[3][3])
    );
}

napi_value JsConvert<JPH::Mat44>::to(napi_env env, JPH::Mat44 v) {
    napi_value result;
    napi_create_array_with_length(env, 4, &result);
    for (int row = 0; row < 4; ++row) {
        napi_value rowArr;
        napi_create_array_with_length(env, 4, &rowArr);
        for (int col = 0; col < 4; ++col) {
            napi_value elem;
            napi_create_double(env, v(row, col), &elem);
            napi_set_element(env, rowArr, col, elem);
        }
        napi_set_element(env, result, row, rowArr);
    }
    return result;
}
*/

// JS -> C++
JPH::Mat44 JsConvert<JPH::Mat44>::from(napi_env env, napi_value v) {
    float m[16];

    for (uint32_t i = 0; i < 16; ++i) {
        napi_value elem;
        napi_get_element(env, v, i, &elem);

        double d;
        napi_get_value_double(env, elem, &d);
        m[i] = static_cast<float>(d);
    }

    // column-major:
    // m[0..3]   = col0
    // m[4..7]   = col1
    // m[8..11]  = col2
    // m[12..15] = col3

    return JPH::Mat44(
        JPH::Vec4(m[0],  m[1],  m[2],  m[3]),
        JPH::Vec4(m[4],  m[5],  m[6],  m[7]),
        JPH::Vec4(m[8],  m[9],  m[10], m[11]),
        JPH::Vec4(m[12], m[13], m[14], m[15])
    );
}

// C++ -> JS
napi_value JsConvert<JPH::Mat44>::to(napi_env env, JPH::Mat44 v) {
    napi_value arr;
    napi_create_array_with_length(env, 16, &arr);

    for (uint32_t col = 0; col < 4; ++col) {
        for (uint32_t row = 0; row < 4; ++row) {
            uint32_t i = col * 4 + row; // column-major индекс

            napi_value elem;
            napi_create_double(env, v(row, col), &elem);
            napi_set_element(env, arr, i, elem);
        }
    }

    return arr;
}
// ─── EventBodyTransform ──────────────────────────────────────────────────────

napi_value JsConvert<EventBodyTransform>::to(napi_env env, const EventBodyTransform& v) {
    napi_value obj;
    napi_create_object(env, &obj);
    napi_set_named_property(env, obj, "body",     JsConvert<JPH::BodyID>::to(env, v.body));
    napi_set_named_property(env, obj, "position", JsConvert<JPH::Vec3>::to(env, v.position));
    napi_set_named_property(env, obj, "rotation", JsConvert<JPH::Quat>::to(env, v.rotation));
    return obj;
}

// ─── EventBodyVelocity ───────────────────────────────────────────────────────

napi_value JsConvert<EventBodyVelocity>::to(napi_env env, const EventBodyVelocity& v) {
    napi_value obj;
    napi_create_object(env, &obj);
    napi_set_named_property(env, obj, "body",           JsConvert<JPH::BodyID>::to(env, v.body));
    napi_set_named_property(env, obj, "linearVelocity", JsConvert<JPH::Vec3>::to(env, v.linearVelocity));
    return obj;
}

// ─── EventBodyContact ────────────────────────────────────────────────────────

napi_value JsConvert<EventBodyContact>::to(napi_env env, const EventBodyContact& v) {
    napi_value obj;
    napi_create_object(env, &obj);
    napi_set_named_property(env, obj, "body1", JsConvert<JPH::BodyID>::to(env, v.body1));
    napi_set_named_property(env, obj, "body2", JsConvert<JPH::BodyID>::to(env, v.body2));
    return obj;
}

// ─── EventBodyContactSelf ────────────────────────────────────────────────────

napi_value JsConvert<EventBodyContactSelf>::to(napi_env env, const EventBodyContactSelf& v) {
    napi_value obj;
    napi_create_object(env, &obj);
    napi_set_named_property(env, obj, "other", JsConvert<JPH::BodyID>::to(env, v.other));
    return obj;
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
