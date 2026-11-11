#ifndef WAFERLEVELTAB_H
#define WAFERLEVELTAB_H

#include <functional>

namespace Ui { class RecipeModule; }
struct S_RecipeWaferLevel;

// 晶圆调平（globalAlign）页签控制器：
// 调平点位表(tw_waferLevelingPoints) ↔ gv_preview_2 的联动（星形标记拖动/点击/选中描边）、
// 表格行操作与 S_RecipeWaferLevel 数据读写收拢在此。
class WaferLevelTab
{
public:
    WaferLevelTab(Ui::RecipeModule *ui, std::function<void()> onModified);

    void setConnect();
    // 编辑态：gv_preview_2 标记可拖动状态切换
    void setEditable(bool editable);
    // 处方加载（原 UpdateUI 调平段）
    void load(const S_RecipeWaferLevel &level);
    // 保存读取（原 GetUIInfo 调平段）
    void save(S_RecipeWaferLevel &level) const;

private:
    // 行操作：与 gv_preview_2 标记保持同步（加行不建标记，坐标由 itemChanged 生成）
    void onAddRow();
    void onDeleteRow();
    void onMoveRowUp();
    void onMoveRowDown();

    Ui::RecipeModule *m_ui = nullptr;
    std::function<void()> m_onModified;
};

#endif // WAFERLEVELTAB_H
