#ifndef MATERIALTAB_H
#define MATERIALTAB_H

#include <functional>

namespace Ui { class RecipeModule; }
struct S_RecipeChipMaterial;

// 物料信息（materialInfo）页签控制器：
// 本页控件的数据 load/save（原 RecipeModule::UpdateUI/GetUIInfo 的 S_RecipeChipMaterial 段）
// 与本页控件初始化收拢在此。本页没有按钮等信号连接，setConnect 保持空实现。
// 通用样式/语言重放/控件通用 mark 循环仍由 RecipeModule 统一负责。
class MaterialTab
{
public:
    MaterialTab(Ui::RecipeModule *ui, std::function<void()> onModified);

    // 本页控件的全部 connect（当前为空——物料页无按钮/联动，改动标记由模块通用循环处理）
    void setConnect();

    // 处方加载：物料段数据 → 本页控件（原 UpdateUI 物料段）
    void load(const S_RecipeChipMaterial &material);
    // 保存读取：本页控件 → 物料段数据（原 GetUIInfo 物料段）
    void save(S_RecipeChipMaterial &material) const;

private:
    Ui::RecipeModule *m_ui = nullptr;
    std::function<void()> m_onModified;   // 数据改动 → RecipeModule::markDataModified
};

#endif // MATERIALTAB_H
