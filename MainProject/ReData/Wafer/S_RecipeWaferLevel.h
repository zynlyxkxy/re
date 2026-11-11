#ifndef S_RECIPEWAFER_LEVEL_H
#define S_RECIPEWAFER_LEVEL_H

#include "../S_RecipeCommon.h"


struct S_RecipeWaferLevel {
    std::vector<S_NominalPoint> lst_waferLevelingPoints;
    YLT_REFL(S_RecipeWaferLevel, lst_waferLevelingPoints);
};

#endif
