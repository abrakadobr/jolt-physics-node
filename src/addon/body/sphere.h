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
        METHOD(Sphere,comPosition),
        METHOD(Sphere,rotation),
        METHOD(Sphere,linearVelocity),
        METHOD(Sphere,transform),
        METHOD(Sphere,comTransform),
        METHOD(Sphere,getType),
        METHOD(Sphere,on),
        // emit — template method, cannot be exposed via METHOD
        METHOD(Sphere, id)
      };
    };

    Sphere(napi_env env);

  private:
    napi_env        _env;

};


}
