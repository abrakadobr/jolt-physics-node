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
        METHOD(Triangle,getType),
        METHOD(Triangle,position),
        METHOD(Triangle,comPosition),
        METHOD(Triangle,rotation),
        METHOD(Triangle,linearVelocity),
        METHOD(Triangle,transform),
        METHOD(Triangle,comTransform),
        METHOD(Triangle,on),
        METHOD(Triangle, id)
      };
    };

    Triangle(napi_env env);


    void setShape(const TriangleShape &shape);
    TriangleShape shape() const;
    TriangleShape shape();
  private:
    napi_env        _env;
    TriangleShape     _shape;

};


}
