#ifndef S_RECIPE_CHIP_BOND_H
#define S_RECIPE_CHIP_BOND_H

#include "../S_RecipeCommon.h"
#include <string>
#include <vector>

// 单个键合参数：规范 key + 值文本。
// key 即翻译 key，与 config/gui_config/Languages/Language.csv 的词条一一对应。
// 参数名与 combo/check 的值都必须是规范 key 字面量——不要写显示文案（如 "Target Force N"）。
struct S_BondParam {
    std::string key;       // 参数规范 key（如 "bond_TargetForceN"）
    std::string value;     // 值：spin=数字文本；combo/check=所属参数的选项 key（如 "bond_BondHead_UpAndDown"）
    YLT_REFL(S_BondParam, key, value);
};

// 单条键合工序：命令规范 key + 参数列表（参数顺序 = 该命令参数定义表里的顺序）
struct S_BondStep {
    std::string command;   // 命令规范 key（如 "ManualForceCtl" / "TempCtl"）
    std::vector<S_BondParam> params;
    YLT_REFL(S_BondStep, command, params);
};

// 键合工序段：有序工序队列（表格行 ↔ 列表元素一一对应）
struct S_RecipeChipBond {
    double maxTargetForce = 0.0;
    double maxTargetTemp = 0.0;
    std::vector<S_BondStep> lst_BondingSequence;
    YLT_REFL(S_RecipeChipBond, lst_BondingSequence,maxTargetForce,maxTargetTemp);
};

#endif
