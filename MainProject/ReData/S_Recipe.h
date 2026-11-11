#ifndef S_RECIPE_H
#define S_RECIPE_H

#include "Chip/S_RecipeChipMaterial.h"
#include "Chip/S_RecipeChipLevel.h"
#include "Chip/S_RecipeChipAlign.h"
#include "Chip/S_RecipeChipBond.h"
#include "Wafer/S_RecipeWaferMaterial.h"
#include "Wafer/S_RecipeWaferLevel.h"
#include "Wafer/S_RecipeWaferBond.h"
#include "Wafer/S_RecipeWaferOperationMode.h"
#include "Wafer/S_RecipeWaferChuckLevel.h"
#include "Wafer/S_RecipeWaferChipCalibration.h"
#include "Wafer/S_RecipeWaferContinuous.h"

struct S_ChipRecipe {
    S_RecipeChipMaterial material;
    S_RecipeChipLevel level;
    S_RecipeChipAlign align;
    S_RecipeChipBond bond;
    YLT_REFL(S_ChipRecipe, material, level, align, bond);
};

struct S_RecipeContentWafer {
    S_RecipeWaferMaterial material;
    S_RecipeWaferLevel waferLevel;
    S_RecipeWaferChuckLevel chuckLevel;
    S_RecipeWaferBond bondPointSelection;
    S_RecipeWaferOperationMode operationMode;
    S_RecipeWaferChipCalibration chipCalibration;
    S_RecipeWaferContinuous continuous;
    YLT_REFL(S_RecipeContentWafer, material, waferLevel, chuckLevel, bondPointSelection, operationMode,chipCalibration, continuous);
};

#endif
