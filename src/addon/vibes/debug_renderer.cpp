
#ifdef JPH_DEBUG_RENDERER
class CollectingDebugRenderer : public DebugRenderer {
 public:
  CollectingDebugRenderer() { Initialize(); }
  struct LineV  { float x1,y1,z1,x2,y2,z2; uint32_t color; };
  struct TriV   { float x1,y1,z1,x2,y2,z2,x3,y3,z3; uint32_t color; };
  std::vector<LineV> lines;
  std::vector<TriV>  tris;
  void Clear() { lines.clear(); tris.clear(); }

  void DrawLine(RVec3Arg from, RVec3Arg to, ColorArg c) override {
    lines.push_back({(float)from.GetX(),(float)from.GetY(),(float)from.GetZ(),
                     (float)to.GetX(),(float)to.GetY(),(float)to.GetZ(), c.GetUInt32()});
  }
  void DrawTriangle(RVec3Arg v1, RVec3Arg v2, RVec3Arg v3, ColorArg c, ECastShadow) override {
    tris.push_back({(float)v1.GetX(),(float)v1.GetY(),(float)v1.GetZ(),
                    (float)v2.GetX(),(float)v2.GetY(),(float)v2.GetZ(),
                    (float)v3.GetX(),(float)v3.GetY(),(float)v3.GetZ(), c.GetUInt32()});
  }
  void DrawText3D(RVec3Arg, const string_view &, ColorArg, float) override {}

  class BatchImpl : public RefTargetVirtual {
   public:
    std::vector<Triangle> mTriangles;
    std::atomic_int mRefCount{0};
    BatchImpl(const Triangle *t, int n) : mTriangles(t, t+n) {}
    void AddRef() override { ++mRefCount; }
    void Release() override { if (--mRefCount == 0) delete this; }
  };

  Batch CreateTriangleBatch(const Triangle *t, int n) override { return new BatchImpl(t,n); }
  Batch CreateTriangleBatch(const Vertex *v, int, const uint32_t *idx, int ic) override {
    std::vector<Triangle> out; out.reserve(ic/3);
    for (int k=0; k+2<ic; k+=3) {
      Triangle tri;
      for (int j=0; j<3; j++) {
        auto &s=v[idx[k+j]];
        tri.mV[j].mPosition=Float3(s.mPosition.x,s.mPosition.y,s.mPosition.z);
        tri.mV[j].mNormal=Float3(s.mNormal.x,s.mNormal.y,s.mNormal.z);
        tri.mV[j].mColor=s.mColor; tri.mV[j].mUV=Float2(s.mUV.x,s.mUV.y);
      }
      out.push_back(tri);
    }
    return new BatchImpl(out.data(),(int)out.size());
  }
  void DrawGeometry(RMat44Arg mat, const AABox &, float, ColorArg color,
                    const GeometryRef &geom, ECullMode, ECastShadow, EDrawMode) override {
    if (!geom || geom->mLODs.empty()) return;
    const auto *impl=static_cast<const BatchImpl*>(geom->mLODs[0].mTriangleBatch.GetPtr());
    if (!impl) return;
    const uint32_t c=color.GetUInt32();
    for (const Triangle &tri : impl->mTriangles) {
      RVec3 v1=mat*Vec3(tri.mV[0].mPosition.x,tri.mV[0].mPosition.y,tri.mV[0].mPosition.z);
      RVec3 v2=mat*Vec3(tri.mV[1].mPosition.x,tri.mV[1].mPosition.y,tri.mV[1].mPosition.z);
      RVec3 v3=mat*Vec3(tri.mV[2].mPosition.x,tri.mV[2].mPosition.y,tri.mV[2].mPosition.z);
      tris.push_back({(float)v1.GetX(),(float)v1.GetY(),(float)v1.GetZ(),
                      (float)v2.GetX(),(float)v2.GetY(),(float)v2.GetZ(),
                      (float)v3.GetX(),(float)v3.GetY(),(float)v3.GetZ(),c});
    }
  }
};
#endif


