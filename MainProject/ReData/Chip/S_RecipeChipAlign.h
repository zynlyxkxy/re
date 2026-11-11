#ifndef S_RECIPE_CHIP_ALIGN_H
#define S_RECIPE_CHIP_ALIGN_H

#include "../S_RecipeCommon.h"

struct S_RecipeUpAlignNominalPoint {
    S_Point2D point;
    std::string upMarkIdWithXNCam;
    std::string upMarkIdWithXPCam;
    YLT_REFL(S_RecipeUpAlignNominalPoint, point, upMarkIdWithXNCam, upMarkIdWithXPCam);
};

struct S_RecipeDownAlignNominalPoint {
    S_Point2D point;
    std::string dwMarkId;
    YLT_REFL(S_RecipeDownAlignNominalPoint, point, dwMarkId);
};

struct S_FineAlignResultToleranceRange{
    double xToleranceRange;
    double yToleranceRange;
    double rzToleranceRange;
    YLT_REFL(S_FineAlignResultToleranceRange, xToleranceRange, yToleranceRange, rzToleranceRange);
};

struct S_RecipeChipAlign {
    std::vector<S_RecipeUpAlignNominalPoint> lst_upAlignPoints;
    std::vector<S_RecipeDownAlignNominalPoint> lst_downAlignPoints;
    S_MarkSearchSetting upMarkSearchSetting;
    S_MarkSearchSetting dwMarkSearchSetting;
    S_FineAlignResultToleranceRange fineAlignResultToleranceRange;
    bool autoProcessAlignResult = false;
    bool levelPointForFineAlign = false;
    int checkAlignTimes = 0;
    YLT_REFL(S_RecipeChipAlign, lst_upAlignPoints, lst_downAlignPoints,
             upMarkSearchSetting, dwMarkSearchSetting,
             fineAlignResultToleranceRange, autoProcessAlignResult, levelPointForFineAlign,
             checkAlignTimes);
};

#endif
