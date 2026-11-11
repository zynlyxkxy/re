#ifndef BONDPOINTTAB_H
#define BONDPOINTTAB_H

#include <functional>
#include <QList>
#include <QPointF>

namespace Ui { class RecipeModule; }
struct S_RecipeWaferBond;

// 键合选点（bonding）页签控制器：
// gv_bond 芯片选点（编辑点位 进入/退出、全选/清空、单选/多片、已选数量）、
// 排布重算后的选点失效清空与 S_RecipeWaferBond 数据读写收拢在此。
class BondPointTab
{
public:
    BondPointTab(Ui::RecipeModule *ui, std::function<void()> onModified);

    void setConnect();
    // 编辑态：退出编辑时复位“编辑点位”状态并同步 gv_bond 可编辑状态
    void setEditable(bool editable);
    // 处方加载（原 UpdateUI 键合段：模式 + 按保存点位绘制可点击芯片）
    void load(const S_RecipeWaferBond &bond, const QList<QPointF> &chipPositions,
              double chipWidth, double chipHeight);
    // 保存读取（原 GetUIInfo 键合段）
    void save(S_RecipeWaferBond &bond) const;
    // 排布重算后旧键合点（绝对坐标）失效：清空重选（模块 applyLayoutParams 调用）
    void resetOnLayoutChange(const QList<QPointF> &chipPositions,
                             double chipWidth, double chipHeight);

private:
    void updateEditUiState();
    void switchBondSelectMode();

    Ui::RecipeModule *m_ui = nullptr;
    std::function<void()> m_onModified;

    bool m_uiEditable = false;   // 模块当前是否处于 Edit 模式
    bool m_editActive = false;   // 点击过“编辑点位”后可编辑 gv_bond
    int m_lastBondMode = -1;     // 加载时的键合模式（均未勾选时保存沿用原值）
};

#endif // BONDPOINTTAB_H
