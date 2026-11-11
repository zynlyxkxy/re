#ifndef S_RECIPEWAFER_OPERATIONMODE_H
#define S_RECIPEWAFER_OPERATIONMODE_H

#include "../S_RecipeCommon.h"

struct S_RecipeWaferOperationMode {
    int operationMode; // 0 Precision, 1 Throughput
    YLT_REFL(S_RecipeWaferOperationMode, operationMode);
};

#endif
