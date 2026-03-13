
class CollisionManager {

  bool QueryAABB(RVec3Arg min, RVec3Arg max, const QueryFilters &filters, std::vector<uint32_t> &out_bodies) const {
    const AABox box(min, max);
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    AllHitCollisionCollector<CollideShapeBodyCollector> collector;
    mPhysicsSystem.GetBroadPhaseQuery().CollideAABox(box, collector, {}, layer_filter);
    if (!collector.HadHit()) return false;

    out_bodies.clear();
    out_bodies.reserve(collector.mHits.size());
    for (const BodyID &hit : collector.mHits) {
      if (!filters.exclude_ids.empty()) {
        const uint32_t raw = hit.GetIndexAndSequenceNumber();
        bool excluded = false;
        for (uint32_t excl : filters.exclude_ids) {
          if (excl == raw) { excluded = true; break; }
        }
        if (excluded) continue;
      }
      out_bodies.push_back(hit.GetIndexAndSequenceNumber());
    }
    return !out_bodies.empty();
  }

  struct ShapeQueryHit {
    uint32_t body_id;
    float fraction;
    float penetration_depth;
    Vec3 point;
    Vec3 normal;
    uint32_t material_index = 0;
  };

  bool CollideSphereAll(RVec3Arg center, float radius, float max_separation, const QueryFilters &filters, std::vector<ShapeQueryHit> &out_hits) const {
    SphereShape sphere(radius);
    CollideShapeSettings settings;
    settings.mMaxSeparationDistance = max_separation;
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    IgnoreMultipleBodiesFilter body_filter;
    body_filter.Reserve(static_cast<uint>(filters.exclude_ids.size()));
    for (uint32_t id : filters.exclude_ids) body_filter.IgnoreBody(BodyID(id));
    AllHitCollisionCollector<CollideShapeCollector> collector;
    mPhysicsSystem.GetNarrowPhaseQuery().CollideShape(
        &sphere,
        Vec3::sReplicate(1.0f),
        RMat44::sTranslation(center),
        settings,
        RVec3::sZero(),
        collector,
        {},
        layer_filter,
        body_filter);
    if (!collector.HadHit()) return false;

    collector.Sort();
    out_hits.clear();
    out_hits.reserve(collector.mHits.size());
    for (const CollideShapeResult &hit : collector.mHits) {
      const Vec3 normal = -hit.mPenetrationAxis.NormalizedOr(Vec3::sAxisY());
      uint32_t mat_index = 0;
      {
        const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), hit.mBodyID2);
        if (lock.Succeeded())
          mat_index = GetHitMaterialIndex(lock.GetBody(), hit.mSubShapeID2);
      }
      out_hits.push_back({hit.mBodyID2.GetIndexAndSequenceNumber(), 0.0f, hit.mPenetrationDepth, hit.mContactPointOn2, normal, mat_index});
    }
    return true;
  }

  bool CastSphereAll(RVec3Arg origin, Vec3Arg direction, float max_dist, float radius, const QueryFilters &filters, std::vector<ShapeQueryHit> &out_hits) const {
    const Vec3 dir = direction.NormalizedOr(Vec3::sAxisX()) * max_dist;
    SphereShape sphere(radius);
    RShapeCast cast(&sphere, Vec3::sReplicate(1.0f), RMat44::sTranslation(origin), dir);
    ShapeCastSettings settings;
    settings.mReturnDeepestPoint = true;
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    IgnoreMultipleBodiesFilter body_filter;
    body_filter.Reserve(static_cast<uint>(filters.exclude_ids.size()));
    for (uint32_t id : filters.exclude_ids) body_filter.IgnoreBody(BodyID(id));

    AllHitCollisionCollector<CastShapeCollector> collector;
    mPhysicsSystem.GetNarrowPhaseQuery().CastShape(cast, settings, RVec3::sZero(), collector, {}, layer_filter, body_filter);
    if (!collector.HadHit()) return false;

    collector.Sort();
    out_hits.clear();
    out_hits.reserve(collector.mHits.size());
    for (const ShapeCastResult &hit : collector.mHits) {
      const Vec3 normal = -hit.mPenetrationAxis.NormalizedOr(Vec3::sAxisY());
      uint32_t mat_index = 0;
      {
        const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), hit.mBodyID2);
        if (lock.Succeeded())
          mat_index = GetHitMaterialIndex(lock.GetBody(), hit.mSubShapeID2);
      }
      out_hits.push_back({hit.mBodyID2.GetIndexAndSequenceNumber(), hit.mFraction, hit.mPenetrationDepth, hit.mContactPointOn2, normal, mat_index});
    }
    return true;
  }

  bool CastBoxAll(RVec3Arg origin, Vec3Arg direction, float max_dist,
                  float hx, float hy, float hz,
                  const QueryFilters &filters, std::vector<ShapeQueryHit> &out_hits) const {
    const Vec3 dir = direction.NormalizedOr(Vec3::sAxisX()) * max_dist;
    BoxShape box(Vec3(hx, hy, hz));
    RShapeCast cast(&box, Vec3::sReplicate(1.0f), RMat44::sTranslation(origin), dir);
    ShapeCastSettings settings;
    settings.mReturnDeepestPoint = true;
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    IgnoreMultipleBodiesFilter body_filter;
    body_filter.Reserve(static_cast<uint>(filters.exclude_ids.size()));
    for (uint32_t id : filters.exclude_ids) body_filter.IgnoreBody(BodyID(id));

    AllHitCollisionCollector<CastShapeCollector> collector;
    mPhysicsSystem.GetNarrowPhaseQuery().CastShape(cast, settings, RVec3::sZero(), collector, {}, layer_filter, body_filter);
    if (!collector.HadHit()) return false;

    collector.Sort();
    out_hits.clear();
    out_hits.reserve(collector.mHits.size());
    for (const ShapeCastResult &hit : collector.mHits) {
      const Vec3 normal = -hit.mPenetrationAxis.NormalizedOr(Vec3::sAxisY());
      uint32_t mat_index = 0;
      {
        const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), hit.mBodyID2);
        if (lock.Succeeded())
          mat_index = GetHitMaterialIndex(lock.GetBody(), hit.mSubShapeID2);
      }
      out_hits.push_back({hit.mBodyID2.GetIndexAndSequenceNumber(), hit.mFraction, hit.mPenetrationDepth, hit.mContactPointOn2, normal, mat_index});
    }
    return true;
  }

  bool CastCapsuleAll(RVec3Arg origin, Vec3Arg direction, float max_dist,
                      float half_height, float radius,
                      const QueryFilters &filters, std::vector<ShapeQueryHit> &out_hits) const {
    const Vec3 dir = direction.NormalizedOr(Vec3::sAxisX()) * max_dist;
    CapsuleShape capsule(half_height, radius);
    RShapeCast cast(&capsule, Vec3::sReplicate(1.0f), RMat44::sTranslation(origin), dir);
    ShapeCastSettings settings;
    settings.mReturnDeepestPoint = true;
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    IgnoreMultipleBodiesFilter body_filter;
    body_filter.Reserve(static_cast<uint>(filters.exclude_ids.size()));
    for (uint32_t id : filters.exclude_ids) body_filter.IgnoreBody(BodyID(id));

    AllHitCollisionCollector<CastShapeCollector> collector;
    mPhysicsSystem.GetNarrowPhaseQuery().CastShape(cast, settings, RVec3::sZero(), collector, {}, layer_filter, body_filter);
    if (!collector.HadHit()) return false;

    collector.Sort();
    out_hits.clear();
    out_hits.reserve(collector.mHits.size());
    for (const ShapeCastResult &hit : collector.mHits) {
      const Vec3 normal = -hit.mPenetrationAxis.NormalizedOr(Vec3::sAxisY());
      uint32_t mat_index = 0;
      {
        const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), hit.mBodyID2);
        if (lock.Succeeded())
          mat_index = GetHitMaterialIndex(lock.GetBody(), hit.mSubShapeID2);
      }
      out_hits.push_back({hit.mBodyID2.GetIndexAndSequenceNumber(), hit.mFraction, hit.mPenetrationDepth, hit.mContactPointOn2, normal, mat_index});
    }
    return true;
  }

  bool RayCastClosest(RVec3Arg origin, Vec3Arg direction, double max_dist, const QueryFilters &filters, uint32_t &out_body, double &out_fraction, Vec3 &out_normal, uint32_t &out_material) const {
    const Vec3 dir = direction.NormalizedOr(Vec3::sAxisX()) * static_cast<float>(max_dist);
    RRayCast ray(origin, dir);
    RayCastResult hit;
    hit.Reset();
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    IgnoreMultipleBodiesFilter body_filter;
    body_filter.Reserve(static_cast<uint>(filters.exclude_ids.size()));
    for (uint32_t id : filters.exclude_ids) body_filter.IgnoreBody(BodyID(id));

    if (!mPhysicsSystem.GetNarrowPhaseQuery().CastRay(ray, hit, {}, layer_filter, body_filter)) {
      return false;
    }

    out_body = hit.mBodyID.GetIndexAndSequenceNumber();
    out_fraction = hit.mFraction;

    const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), hit.mBodyID);
    if (lock.Succeeded()) {
      const Body &body = lock.GetBody();
      out_normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, ray.GetPointOnRay(hit.mFraction));
      out_material = GetHitMaterialIndex(body, hit.mSubShapeID2);
    } else {
      out_normal = Vec3::sZero();
      out_material = 0;
    }

    return true;
  }

  struct RayHitInfo {
    uint32_t body_id;
    float fraction;
    Vec3 normal;
    uint32_t material_index = 0;
  };

  bool RayCastAll(RVec3Arg origin, Vec3Arg direction, double max_dist, const QueryFilters &filters, std::vector<RayHitInfo> &out_hits) const {
    const Vec3 dir = direction.NormalizedOr(Vec3::sAxisX()) * static_cast<float>(max_dist);
    RRayCast ray(origin, dir);
    RayCastSettings settings;
    MaskObjectLayerFilter layer_filter(filters.layer_mask);
    IgnoreMultipleBodiesFilter body_filter;
    body_filter.Reserve(static_cast<uint>(filters.exclude_ids.size()));
    for (uint32_t id : filters.exclude_ids) body_filter.IgnoreBody(BodyID(id));
    AllHitCollisionCollector<CastRayCollector> collector;
    mPhysicsSystem.GetNarrowPhaseQuery().CastRay(ray, settings, collector, {}, layer_filter, body_filter);
    if (!collector.HadHit()) return false;

    collector.Sort();
    out_hits.clear();
    out_hits.reserve(collector.mHits.size());

    for (const RayCastResult &hit : collector.mHits) {
      Vec3 normal = Vec3::sZero();
      uint32_t mat_index = 0;
      const BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), hit.mBodyID);
      if (lock.Succeeded()) {
        const Body &b = lock.GetBody();
        normal = b.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, ray.GetPointOnRay(hit.mFraction));
        mat_index = GetHitMaterialIndex(b, hit.mSubShapeID2);
      }
      out_hits.push_back({hit.mBodyID.GetIndexAndSequenceNumber(), hit.mFraction, normal, mat_index});
    }

    return true;
  }

  bool AreBodiesInContact(uint32_t a, uint32_t b) const {
    return mPhysicsSystem.WereBodiesInContact(BodyID(a), BodyID(b));
  }




}
