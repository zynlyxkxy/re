#ifndef LEVELTAB_H
#define LEVELTAB_H

#include <functional>

namespace Ui { class RecipeModule; }
struct S_RecipeChipLevel;

// 调平对焦（coarseAlign）页签控制器：
// 本页控件的数据 load/save（原 RecipeModule::UpdateUI/GetUIInfo 的 S_RecipeChipLevel 段）、
// 三张调平点表（粗调平/上调平/下调平）配置与按钮连接收拢在此。
// 通用样式/语言重放（含 GroupBox 标题清单）/控件通用 mark 循环仍由 RecipeModule 统一负责。
class LevelTab
{
public:
    LevelTab(Ui::RecipeModule *ui, std::function<void()> onModified);

    // 本页控件的全部 connect（三张调平点表增删/上下移按钮）
    void setConnect();

    // 处方加载：调平段数据 → 本页控件（原 UpdateUI 调平段）
    void load(const S_RecipeChipLevel &level);
    // 保存读取：本页控件 → 调平段数据（原 GetUIInfo 调平段）
    void save(S_RecipeChipLevel &level) const;

private:
    Ui::RecipeModule *m_ui = nullptr;
    std::function<void()> m_onModified;   // 数据改动 → RecipeModule::markDataModified
};

#endif // LEVELTAB_H
