#ifndef S_RECIPE_CHIP_LEVEL_H
#define S_RECIPE_CHIP_LEVEL_H

#include "../S_RecipeCommon.h"

struct S_LevelSetting{
    double rxLevelOffset = 0.0;
    double ryLevelOffset = 0.0;
    int checkFineLevelTimes = 0;
    YLT_REFL(S_LevelSetting, rxLevelOffset, ryLevelOffset, checkFineLevelTimes);
};

struct S_FineLevelResultToleranceRange{
    double rxToleranceRange;
    double ryToleranceRange;
    double zToleranceRange;
    YLT_REFL(S_FineLevelResultToleranceRange, rxToleranceRange, ryToleranceRange, zToleranceRange);
};

struct S_RecipeChipLevel {

    std::vector<S_TakePhotoNominalPoint> lst_coarseLevelPoints;
    std::vector<S_TakePhotoNominalPoint> lst_upLevelPoints;
    S_MarkSearchSetting upMarkSearchSetting;
    std::vector<S_TakePhotoNominalPoint> lst_dwLevelPoints;
    S_MarkSearchSetting dwMarkSearchSetting;
    S_LevelSetting  levelSetting;
    double upChipLevelFocalPlaneOffset = 0.0;
    double dwChipLevelFocalPlaneOffset = 0.0;
    S_FineLevelResultToleranceRange fineLevelResultToleranceRange;
    YLT_REFL(S_RecipeChipLevel, lst_coarseLevelPoints, lst_upLevelPoints, upMarkSearchSetting,
             lst_dwLevelPoints, dwMarkSearchSetting, levelSetting,
             upChipLevelFocalPlaneOffset, dwChipLevelFocalPlaneOffset,
             fineLevelResultToleranceRange);
};

#endif
