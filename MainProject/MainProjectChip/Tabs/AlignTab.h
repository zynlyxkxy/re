#ifndef ALIGNTAB_H
#define ALIGNTAB_H

#include <functional>

namespace Ui { class RecipeModule; }
struct S_RecipeChipAlign;

// 精对准（fineAlign）页签控制器：
// 本页控件的数据 load/save（原 RecipeModule::UpdateUI/GetUIInfo 的 S_RecipeChipAlign 段）
// 与按钮/表格连接（原 RecipeModule::setConnections 中本页段）收拢在此。
// 通用样式、语言重放（含 GroupBox 标题清单）、控件通用 mark 循环仍由 RecipeModule 统一负责。
class AlignTab
{
public:
    AlignTab(Ui::RecipeModule *ui, std::function<void()> onModified);

    // 本页控件的全部 connect 集中在此（RecipeModule 不再出现本页控件连接）
    void setConnect();

    // 处方加载：对准段数据 → 本页控件（原 UpdateUI 对准段）
    void load(const S_RecipeChipAlign &align);
    // 保存读取：本页控件 → 对准段数据（原 GetUIInfo 对准段）
    void save(S_RecipeChipAlign &align) const;

private:
    Ui::RecipeModule *m_ui = nullptr;
    std::function<void()> m_onModified;   // 数据改动 → RecipeModule::markDataModified
};

#endif // ALIGNTAB_H
