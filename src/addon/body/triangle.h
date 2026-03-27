#pragma once

#include "../jolt.h"
#include "body.h"
#include "../napi/napi_base.h"

namespace JOLT {

class Triangle: public Body, public NApiBase<Triangle> {


  public:
    static constexpr const char* ClassName = "Triangle";

    static std::vector<napi_property_descriptor> Methods() {
      return {
        METHOD(Triangle,position),
        METHOD(Triangle,rotation),
        METHOD(Triangle,getType),
        METHOD(Triangle,on),
        // METHOD(Triangle,emit),
        METHOD(Triangle, id)
      };
    };

    Triangle(napi_env env);

  private:
    napi_env        _env;

};


}
