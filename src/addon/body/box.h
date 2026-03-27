#pragma once

#include "../jolt.h"
#include "body.h"
#include "../napi/napi_base.h"

namespace JOLT {

class Box: public Body, public NApiBase<Box> {


  public:
    static constexpr const char* ClassName = "Box";

    static std::vector<napi_property_descriptor> Methods() {
      return {
        METHOD(Box,position),
        METHOD(Box,comPosition),
        METHOD(Box,rotation),
        METHOD(Box,linearVelocity),
        METHOD(Box,transform),
        METHOD(Box,comTransform),
        METHOD(Box,on),
        METHOD(Box, id)
      };
    };

    Box(napi_env env);

  private:
    napi_env        _env;

};


}
