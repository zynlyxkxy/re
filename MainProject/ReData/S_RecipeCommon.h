#ifndef S_RECIPE_COMMON_H
#define S_RECIPE_COMMON_H

#include "istar_basic_value_point.h"
#include "istar_serializer.hpp"

using namespace ISTAR::BasicPlatform::BasicValue;

struct S_NominalPoint {
    S_Point2D point;
    YLT_REFL(S_NominalPoint, point);
};

struct S_TakePhotoNominalPoint {
    S_Point2D point;
    std::string markId;
    YLT_REFL(S_TakePhotoNominalPoint, point, markId);
};

struct S_DualMarkNominalPoint {
    S_Point2D point;
    std::string markIdDown;
    std::string markIdUp;
    YLT_REFL(S_DualMarkNominalPoint, point, markIdDown, markIdUp);
};

struct S_MarkSearchSetting {
    double searchWidth = 0.0;
    double searchHeight = 0.0;
    int searchCircles = 0;
    YLT_REFL(S_MarkSearchSetting, searchWidth, searchHeight, searchCircles);
};



#endif
