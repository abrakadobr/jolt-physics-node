#pragma once

#include "../jolt.h"
#include "body.h"
#include "../napi/napi_base.h"

namespace JOLT {

class ConvexHull: public Body, public NApiBase<ConvexHull> {


  public:
    static constexpr const char* ClassName = "ConvexHull";

    static std::vector<napi_property_descriptor> Methods() {
      return {
        METHOD(ConvexHull,getType),
        METHOD(ConvexHull,position),
        METHOD(ConvexHull,comPosition),
        METHOD(ConvexHull,rotation),
        METHOD(ConvexHull,linearVelocity),
        METHOD(ConvexHull,transform),
        METHOD(ConvexHull,comTransform),
        METHOD(ConvexHull,on),
        METHOD(ConvexHull, id)
      };
    };

    ConvexHull(napi_env env);

  private:
    napi_env        _env;

};


}
