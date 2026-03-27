#pragma once

#include "js_convert.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Vec4.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>

namespace JOLT {

/* JPH::BodyID → uint32_t */
template<> struct JsConvert<JPH::BodyID> {
    static JPH::BodyID from(napi_env env, napi_value v);
    static napi_value to(napi_env env, JPH::BodyID v);
};

/* JPH::Vec3 → {x, y, z} */
template<> struct JsConvert<JPH::Vec3> {
    static JPH::Vec3 from(napi_env env, napi_value v);
    static napi_value to(napi_env env, JPH::Vec3 v);
};

/* JPH::Vec4 → {x, y, z, w} */
template<> struct JsConvert<JPH::Vec4> {
    static JPH::Vec4 from(napi_env env, napi_value v);
    static napi_value to(napi_env env, JPH::Vec4 v);
};

/* JPH::Quat → {x, y, z, w} */
template<> struct JsConvert<JPH::Quat> {
    static JPH::Quat from(napi_env env, napi_value v);
    static napi_value to(napi_env env, JPH::Quat v);
};

/* JPH::Mat44 → Float32Array(16), column-major */
template<> struct JsConvert<JPH::Mat44> {
    static JPH::Mat44 from(napi_env env, napi_value v);
    static napi_value to(napi_env env, JPH::Mat44 v);
};

/* EventBodyTransform → {body, position, rotation}  (to only) */
template<> struct JsConvert<EventBodyTransform> {
    static napi_value to(napi_env env, const EventBodyTransform& v);
};

/* EventBodyVelocity → {body, linearVelocity}  (to only) */
template<> struct JsConvert<EventBodyVelocity> {
    static napi_value to(napi_env env, const EventBodyVelocity& v);
};

/* EventBodyContact → {body1, body2}  (to only) */
template<> struct JsConvert<EventBodyContact> {
    static napi_value to(napi_env env, const EventBodyContact& v);
};

/* EventBodyContactSelf → {other}  (to only) */
template<> struct JsConvert<EventBodyContactSelf> {
    static napi_value to(napi_env env, const EventBodyContactSelf& v);
};

/* JPH::RayCastResult → {bodyId, fraction, subShapeId}  (to only) */
template<> struct JsConvert<JPH::RayCastResult> {
    static napi_value to(napi_env env, const JPH::RayCastResult& v);
};

/* JPH::CollideShapeResult → {bodyId, contactPointOn1, contactPointOn2,
                               penetrationAxis, penetrationDepth,
                               subShapeId1, subShapeId2}  (to only) */
template<> struct JsConvert<JPH::CollideShapeResult> {
    static napi_value to(napi_env env, const JPH::CollideShapeResult& v);
};

/* JPH::RVec3 → {x, y, z}
   Without JPH_DOUBLE_PRECISION RVec3 = Vec3, so this block is compiled
   only when they are distinct types to avoid redefinition. */
#ifdef JPH_DOUBLE_PRECISION
template<> struct JsConvert<JPH::RVec3> {
    static JPH::RVec3 from(napi_env env, napi_value v);
    static napi_value to(napi_env env, JPH::RVec3 v);
};
#endif

}
