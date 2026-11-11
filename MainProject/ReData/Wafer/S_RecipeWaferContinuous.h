#ifndef S_RECIPEWAFER_CONTINUOUS_H
#define S_RECIPEWAFER_CONTINUOUS_H

#include "../S_RecipeCommon.h"

// 键合前后识别模板：一整列模板名（对应表格 tw_bondRecognitionTemplate 的一列）
// 字段顺序 = 表格行顺序：X Trapezoid / X Bar / Y Trapezoid / Y Bar
struct S_RecipeWaferContinuousBondTemplate {
    std::string trapezoidX;  // X Trapezoid Template
    std::string barX;        // X Bar Template
    std::string trapezoidY;  // Y Trapezoid Template
    std::string barY;        // Y Bar Template
    YLT_REFL(S_RecipeWaferContinuousBondTemplate, trapezoidX, barX, trapezoidY, barY);
};

// 单个连续键合点的补偿偏移（mm）：X 方向偏移、Y 方向偏移，各自含 X/Y 两个分量
struct S_RecipeWaferContinuousPointOffset {
    double xOffsetX = 0.0;  // X 方向偏移的 X 分量（UI: Point N X Offset 左值）
    double xOffsetY = 0.0;  // X 方向偏移的 Y 分量（UI: Point N X Offset 右值）
    double yOffsetX = 0.0;  // Y 方向偏移的 X 分量（UI: Point N Y Offset 左值）
    double yOffsetY = 0.0;  // Y 方向偏移的 Y 分量（UI: Point N Y Offset 右值）
    YLT_REFL(S_RecipeWaferContinuousPointOffset, xOffsetX, xOffsetY, yOffsetX, yOffsetY);
};

// 连续键合（Continuous 页签）：
//   - 调平台下压高度、键合次数（gb_precisionDetectSetting）
//   - 键合前后识别模板：表格 4 行 × 2 列（gb_bondRecognitionTemplate / tw_bondRecognitionTemplate），
//     每列各存 1 个元素（含该列的 4 条模板名）
//   - 点补偿偏移：固定 4 个点（Point 1~4，gb_markOffset）
struct S_RecipeWaferContinuous {
    double levelingStageDownHeight = 0.0;  // 调平台下压高度 mm
    int bondTimes = 0;                     // 连续键合次数
    std::vector<S_RecipeWaferContinuousBondTemplate> lst_beforeBondRecognitionTemplate;  // Before Bond 列
    std::vector<S_RecipeWaferContinuousBondTemplate> lst_afterBondRecognitionTemplate;   // After Bond 列
    std::vector<S_RecipeWaferContinuousPointOffset> lst_pointOffset;  // 4 个点的 X/Y 补偿偏移
    YLT_REFL(S_RecipeWaferContinuous, levelingStageDownHeight, bondTimes,
             lst_beforeBondRecognitionTemplate, lst_afterBondRecognitionTemplate, lst_pointOffset);
};

#endif
