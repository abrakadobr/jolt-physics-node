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
        METHOD(Capsule,getType),
        METHOD(Capsule,position),
        METHOD(Capsule,comPosition),
        METHOD(Capsule,rotation),
        METHOD(Capsule,linearVelocity),
        METHOD(Capsule,transform),
        METHOD(Capsule,comTransform),
        METHOD(Capsule,on),
        METHOD(Capsule, id)
      };
    };

    Capsule(napi_env env);


    void setShape(const CapsuleShape &shape);
    CapsuleShape shape() const;
    CapsuleShape shape();
  private:
    napi_env        _env;
    CapsuleShape    _capsuleShape;

};


}
