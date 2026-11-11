#ifndef OPERATIONMODETAB_H
#define OPERATIONMODETAB_H

#include <functional>

namespace Ui { class RecipeModule; }
struct S_RecipeWaferOperationMode;

// 运行模式（OperationMode）页签控制器：
// 精度/产率 互斥勾选与 S_RecipeWaferOperationMode 数据读写。
class OperationModeTab
{
public:
    OperationModeTab(Ui::RecipeModule *ui, std::function<void()> onModified);

    void setConnect();   // 本页无按钮/联动，保留统一接口（改动标记由模块通用循环处理）
    void load(const S_RecipeWaferOperationMode &opMode);
    void save(S_RecipeWaferOperationMode &opMode) const;

private:
    Ui::RecipeModule *m_ui = nullptr;
    std::function<void()> m_onModified;
    int m_lastOperationMode = -1;   // 加载时的运行模式（均未勾选时保存沿用原值）
};

#endif // OPERATIONMODETAB_H
