#ifndef S_RECIPEWAFER_BOND_H
#define S_RECIPEWAFER_BOND_H

#include "../S_RecipeCommon.h"

struct S_RecipeWaferBond {
    int bondMode; // 0 Single, 1 Multi
    std::vector<S_Point2D> lst_bondNominalPoint;
    YLT_REFL(S_RecipeWaferBond, bondMode, lst_bondNominalPoint);
};

#endif
