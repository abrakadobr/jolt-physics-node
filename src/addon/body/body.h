#pragma once

#include "body_shapes.h"

namespace JOLT {

  class Body {

    public:

      explicit Body();
      ~Body();

      BodyShapeType   getBodyShape() const;
      void            setBodyShape(const BodyShapeType &type);

    protected:
      BodyShapeType     _bodyShape;
  };

}
