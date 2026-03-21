#pragma once

#include "../jolt.h"
#include "body.h"
#include "../napi/napi_base.h"

namespace JOLT {

class Sphere: public Body, public NApiBase<Sphere> {


  public:
    static constexpr const char* ClassName = "Sphere";

    static std::vector<napi_property_descriptor> Methods() {
      return {
        METHOD(Sphere,position),
        METHOD(Sphere,rotation),
        METHOD(Sphere,getType),
        METHOD(Sphere, id)
      };
    };

    Sphere(napi_env env);

  private:
    napi_env        _env;

};


}
