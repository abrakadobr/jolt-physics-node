#pragma once

#include "../jolt.h"
#include "body.h"
#include "../napi/napi_base.h"

namespace JOLT {

class Capsule: public Body, public NApiBase<Capsule> {


  public:
    static constexpr const char* ClassName = "Capsule";

    static std::vector<napi_property_descriptor> Methods() {
      return {
        METHOD(Capsule,position),
        METHOD(Capsule,rotation),
        METHOD(Capsule,getType),
        METHOD(Capsule,on),
        // METHOD(Capsule,emit),
        METHOD(Capsule, id)
      };
    };

    Capsule(napi_env env);

  private:
    napi_env        _env;

};


}
