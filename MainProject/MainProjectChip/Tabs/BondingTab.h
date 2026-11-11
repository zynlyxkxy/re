#ifndef BONDINGTAB_H
#define BONDINGTAB_H

#include "Bonding/BondingParamDelegate.h"

#include <QList>
#include <QStringList>
#include <functional>
#include <vector>

class QWidget;
class QTableWidget;

namespace Ui { class RecipeModule; }
struct S_RecipeChipBond;
struct TheoryBondData;
struct S_BondStep;
struct S_BondParam;

// 键合工序（bonding）页签控制器：
// 动态工序表的初始化/增删/上下移/命令下拉联动、模拟曲线与处方校验全部收拢在此
// （原 RecipeModule 中 bonding_* 全家、onSimulationCurveClicked/getTheoryBondData、
//  onVerifyRecipeClicked/validateRecipe/applyBondingCheckFlags，及其私有状态
//  selectCmd/m_bondingEditable）。
// 通用样式/语言重放（含 GroupBox 标题清单）/控件通用 mark 循环仍由 RecipeModule 负责，
// 命令下拉与已建工序表的语言重建通过 retranslateCommandCombo()/retranslateTable()
// 由模块 applyLanguage 调用。
class BondingTab
{
public:
    BondingTab(Ui::RecipeModule *ui, QWidget *dialogParent, std::function<void()> onModified);

    // 本页控件全部 connect（增删/上下移/命令下拉/选中联动/校验/模拟）
    void setConnect();
    // 编辑态：键合表勾选列随编辑模式切换可勾选状态
    void setEditable(bool editable);
    // 处方加载（原 bonding_loadUi）
    void load(const S_RecipeChipBond &bond);
    // 保存读取（原 bonding_dumpUi）
    void save(S_RecipeChipBond &bond) const;
    // 语言切换时重建命令下拉（文本 = Tr(raw)，raw 数据不随语言变）
    void retranslateCommandCombo();
    // 语言切换时就地重译已建好的工序表（不重建、不丢数据）
    void retranslateTable();

private:
    QList<BondingParamItem> getBondingParamItems(const QString &cmd) const;
    std::vector<S_BondParam> bonding_rowToParams(int dataRow, const QString &rawCmd) const;
    // params：与 paramKeys 等长的参数值文本（即 S_BondParam::value；combo/check 为规范选项 key）
    // paramKeys：参数规范 key（处方自带 key），列超出命令定义时用它还原
    void bonding_updateTableRow(int row, const QString &rawCmd, const QStringList &params,
                                const QStringList &paramKeys = QStringList());
    void bonding_updateRowHeaderLabels();
    // 列表头（Command / P1..Pn）按当前语言重放，并重新应用列宽自适应模式
    void bonding_updateHorizontalHeaderLabels();
    // 统计键合工序表中所有 Target Force / Target Temperature 的最大值（供 save 写入处方）
    void calcBondingMaxTargets(double &maxForce, double &maxTemp) const;
    void bonding_setColumnCount(int paramCount);
    void bonding_updateColumnCount();
    void applyBondingCheckFlags();
    void bondingTabWidget_add(QTableWidget *widget);
    void bondingTabWidget_delete(QTableWidget *widget);
    void bondingTabWidget_up(QTableWidget *widget);
    void bondingTabWidget_down(QTableWidget *widget);

    void onSimulationCurveClicked();
    void getTheoryBondData(TheoryBondData &dt, const std::vector<S_BondStep> &steps);
    void onVerifyRecipeClicked();

    enum RecipeErrorType
    {
        TempNotEqual,
        ForceTimeSetError,
        WaitTempTimeSetError,
        Success,
    };
    struct ValidationResult
    {
        RecipeErrorType type = Success;
        int rowB = -1;
        int rowA = -1;
        double valB = 0;
        double valA = 0;
    };
    ValidationResult validateRecipe();

    Ui::RecipeModule *m_ui = nullptr;
    QWidget *m_dialogParent = nullptr;   // 模态对话框父窗口（模块 m_mainWidget）
    std::function<void()> m_onModified;  // 数据改动 → RecipeModule::markDataModified

    QString selectCmd;                    // 命令下拉当前项的 raw 命令名（新增工序用）
    bool m_bondingEditable = false;
};

#endif // BONDINGTAB_H
