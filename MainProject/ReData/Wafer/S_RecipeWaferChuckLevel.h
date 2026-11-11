#ifndef S_RECIPEWAFER_CHUCKLEVEL_H
#define S_RECIPEWAFER_CHUCKLEVEL_H

#include "../S_RecipeCommon.h"

struct S_ChuckLevelResultToleranceRange{
    double rxToleranceRange;
    double ryToleranceRange;
    YLT_REFL(S_ChuckLevelResultToleranceRange, rxToleranceRange, ryToleranceRange);
};

struct S_RecipeWaferChuckLevel {
    std::vector<S_NominalPoint> lst_upChuckLevelingPoints;
    std::vector<S_NominalPoint> lst_downChuckLevelingPoints;
    S_ChuckLevelResultToleranceRange chuckLevelResultToleranceRange;
    YLT_REFL(S_RecipeWaferChuckLevel, lst_upChuckLevelingPoints, lst_downChuckLevelingPoints, chuckLevelResultToleranceRange);
};

#endif
