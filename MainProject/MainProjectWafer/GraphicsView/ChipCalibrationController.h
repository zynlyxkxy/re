#ifndef CHIPCALIBRATIONCONTROLLER_H
#define CHIPCALIBRATIONCONTROLLER_H

#include <QObject>
#include <QPointF>
#include <vector>
#include "ReData/S_Recipe.h"
class WaferGraphicsView;
class QTableWidget;
class QPushButton;

struct S_NominalPoint;

// Chip 标定交互控制器（gv_preview_4 + tw_ChipCalibration）：
// 状态机：点击 Add → 追加一个空行进入“选点态”（Delete/上移/下移/Add 全部禁用，
// Confirm 选好点后才可点）→ 图上按 chip 处方的下对位点显示虚色候选点，
// 点击某个候选点使其变实色并把坐标回填该行（保留 3 位小数，再点别的即改选）
// → 点击 Confirm 锁定该标记（图上不可拖/点/选、无描边）并退出选点态：
// Confirm 置灰、候选点层清空、其余按钮恢复。
// 已确认标记仍可用表格 Delete/上移/下移 管理（对应圆点由 RecipeModule 同步删除/移动）。
// 表格坐标只由“候选点选点”产生（表格单元格只读），不允许在图上自由放置点位。
class ChipCalibrationController : public QObject
{
    Q_OBJECT
public:
    ChipCalibrationController(WaferGraphicsView *view, QTableWidget *table,
                              QPushButton *btnAdd, QPushButton *btnDelete,
                              QPushButton *btnUp, QPushButton *btnDown,
                              QPushButton *btnConfirm, QObject *parent = nullptr);

    bool isPlacing() const { return m_placing; }

    // 候选点集合（chip 处方的下对位点名义坐标）：由页签在 chip 切换/排布重算后下发；
    // 为空时 Add/Delete/上移/下移 全部置灰（没有可选点位就无可操作内容），Add 也不会进入选点态
    void setCandidatePoints(const std::vector<S_Point2D> &points);

    // 处方加载：丢弃进行中的放置并重建表格行 + 锁定标记（由 RecipeModule::UpdateUI 调用）
    void loadPoints(const std::vector<S_NominalPoint> &points);
    // 表格当前全部行 → S_NominalPoint 列表（由 RecipeModule::GetUIInfo 调用）
    std::vector<S_NominalPoint> collectPoints() const;
    // 中断进行中的选点：删除待确认的空行、退出选点态并清空候选点层（重载处方/退出编辑时调用）
    void cancelPlacement();
    // 通知 RecipeModule 编辑态切换：false 时取消放置并按编辑态更新按钮状态机
    void setUiEditable(bool editable);

    std::vector<S_Point2D> getAllMarkCandidates();
    void setAllMarkCandidates(std::vector<S_Point2D> value);

signals:
    // 新增/确认/删除等使数据变化（供 RecipeModule 标记处方已修改）
    void dataModified();

private:
    void startPlacement();
    void onCandidateClicked(int index, int table);   // 候选点被点击：选中该候选点并回填表格
    void onConfirmClicked();
    void onDeleteClicked();
    void onUpClicked();
    void onDownClicked();
    void updateButtonStates();
    QList<int> usedCandidateIndices() const;         // 已被表格占用的候选点序号（不绘制、不可点）

    WaferGraphicsView *m_view = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_btnAdd = nullptr;
    QPushButton *m_btnDelete = nullptr;
    QPushButton *m_btnUp = nullptr;
    QPushButton *m_btnDown = nullptr;
    QPushButton *m_btnConfirm = nullptr;
    std::vector<S_Point2D> m_candidates;   // 候选点（chip 处方的下对位点，名义坐标）

    bool m_uiEditable = false;   // RecipeModule 当前是否处于 Edit 模式
    bool m_placing = false;      // 是否处于“选点态”
    bool m_pendingPlaced = false;// 选点态中是否已选中候选点（Confirm 可点条件）
    int m_pendingRow = -1;       // 选点态对应的表格行（Add 追加的末行）
    int m_pendingIndex = -1;     // 选点态中已选中的候选点序号（-1 表示未选）
    int limitedNum = 4;          // 最多放N个标记
};

#endif // CHIPCALIBRATIONCONTROLLER_H
