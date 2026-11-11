#ifndef CHUCKLEVELTAB_H
#define CHUCKLEVELTAB_H

#include <functional>

class QTableWidget;

namespace Ui { class RecipeModule; }
struct S_RecipeWaferChuckLevel;

// 吸盘调平（chuckLevel）页签控制器：
// 上/下吸盘调平点表(tw_upChuckAlign/tw_downChuckAlign) ↔ gv_preview_3 的联动、
// 行操作、公差控件与 S_RecipeWaferChuckLevel 数据读写收拢在此。
class ChuckLevelTab
{
public:
    ChuckLevelTab(Ui::RecipeModule *ui, std::function<void()> onModified);

    void setConnect();
    // 编辑态：gv_preview_3 标记可拖动状态切换
    void setEditable(bool editable);
    void load(const S_RecipeWaferChuckLevel &level);
    void save(S_RecipeWaferChuckLevel &level) const;

private:
    void onAddRow(QTableWidget *tw);
    void onDeleteRow(QTableWidget *tw);
    void onMoveRowUp(QTableWidget *tw);
    void onMoveRowDown(QTableWidget *tw);
    // table==1 表示下吸盘（gv_preview_3 的标记表号）
    int tableIndexOf(QTableWidget *tw) const;

    Ui::RecipeModule *m_ui = nullptr;
    std::function<void()> m_onModified;
};

#endif // CHUCKLEVELTAB_H
