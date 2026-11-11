#ifndef S_RECIPEWAFER_MATERIAL_H
#define S_RECIPEWAFER_MATERIAL_H

#include "../S_RecipeCommon.h"

enum WaferLayoutMode {
    ChipAndWaferCenterCoincide = 0,
    ChipCornerAndWaferCenterCoincide = 1
};
struct S_RecipeDwChipInfo {
    std::string chipRecipeName;
    double chipWidth = 0.0;
    double chipHeight = 0.0;
    YLT_REFL(S_RecipeDwChipInfo, chipRecipeName, chipWidth, chipHeight);
};

struct S_RecipeWaferMaterialOneChipTypeInfo {
    double waferSize = 0.0;
    double waferEdgeLength = 0.0;
    double waferThickness = 0.0;
    int waferNotchOrient = 0;
    double xChipPitch = 0.0;
    double yChipPitch = 0.0;
    int waferLayoutMode = 0;
    S_RecipeDwChipInfo chipRecipe;
    std::vector<S_Point2D> lst_nominalPoints;
    YLT_REFL(S_RecipeWaferMaterialOneChipTypeInfo, waferSize, waferEdgeLength,
             waferThickness, waferNotchOrient, xChipPitch, yChipPitch, waferLayoutMode, chipRecipe, lst_nominalPoints);
};

struct S_RecipeWaferMaterial {
    int materialEditMode; // 0 OneChip, 1 Custom
    S_RecipeWaferMaterialOneChipTypeInfo oneChipTypeInfo;
    YLT_REFL(S_RecipeWaferMaterial, materialEditMode, oneChipTypeInfo);
};

#endif
