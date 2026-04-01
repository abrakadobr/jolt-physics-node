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
        METHOD(Sphere,getType),
        METHOD(Sphere,position),
        METHOD(Sphere,comPosition),
        METHOD(Sphere,rotation),
        METHOD(Sphere,linearVelocity),
        METHOD(Sphere,transform),
        METHOD(Sphere,comTransform),
        METHOD(Sphere,setPosition),
        METHOD(Sphere,setRotation),
        METHOD(Sphere,setPositionAndRotation),
        METHOD(Sphere,on),
        METHOD(Sphere,shape),
        METHOD(Sphere, id)
      };
    };

    Sphere(napi_env env);

    void setShape(const SphereShape &shape);
    // SphereShape shape() const;
    SphereShape shape();

  private:
    napi_env        _env;
    SphereShape     _shape;

};


}
