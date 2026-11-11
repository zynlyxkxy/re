#ifndef S_RECIPEWAFER_CHIPCALIBARTION_H
#define S_RECIPEWAFER_CHIPCALIBARTION_H

#include "../S_RecipeCommon.h"


struct S_RecipeWaferChipCalibration {
    std::vector<S_NominalPoint> lst_chipCalibrationPoints;
    std::vector<S_Point2D> lst_allChipMarkCalibrationPoints;
    YLT_REFL(S_RecipeWaferChipCalibration, lst_chipCalibrationPoints, lst_allChipMarkCalibrationPoints);
};

#endif
