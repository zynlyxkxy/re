#ifndef S_RECIPE_CHIP_MATERIAL_H
#define S_RECIPE_CHIP_MATERIAL_H

#include "../S_RecipeCommon.h"

struct S_RecipeUpChipMaterialInfo {
    double chipWidth = 0.0;
    double chipHeight = 0.0;
    double chipThickness = 0.0;
    YLT_REFL(S_RecipeUpChipMaterialInfo, chipWidth, chipHeight, chipThickness);
};

struct S_RecipeDwChipMaterialInfo {
    double chipWidth = 0.0;
    double chipHeight = 0.0;
    double bump = 0.0;
    YLT_REFL(S_RecipeDwChipMaterialInfo, chipWidth, chipHeight, bump);
};

struct S_RecipeBlueWaferInfo {
    int waferSize = 0;
    int waferNotchOrient = 0;
    double expansionDepth = 0.0;
    double xChipPitch = 0.0;
    double yChipPitch = 0.0;
    YLT_REFL(S_RecipeBlueWaferInfo, waferSize, waferNotchOrient, expansionDepth, xChipPitch, yChipPitch);
};

struct S_RecipeChipMaterial {
    S_RecipeUpChipMaterialInfo upChipMaterialInfo;
    S_RecipeDwChipMaterialInfo dwChipMaterialInfo;
    S_RecipeBlueWaferInfo blueWaferInfo;
    YLT_REFL(S_RecipeChipMaterial, upChipMaterialInfo, dwChipMaterialInfo, blueWaferInfo);
};

#endif
