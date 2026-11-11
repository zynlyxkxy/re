#include "ChipCalibrationController.h"

#include "ReData/S_RecipeCommon.h"
#include "GraphicsView/WaferGraphicsView.h"

#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>

ChipCalibrationController::ChipCalibrationController(WaferGraphicsView *view, QTableWidget *table,
                                                     QPushButton *btnAdd, QPushButton *btnDelete,
                                                     QPushButton *btnUp, QPushButton *btnDown,
                                                     QPushButton *btnConfirm, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_table(table)
    , m_btnAdd(btnAdd)
    , m_btnDelete(btnDelete)
    , m_btnUp(btnUp)
    , m_btnDown(btnDown)
    , m_btnConfirm(btnConfirm)
{
    connect(m_btnAdd, &QPushButton::clicked, this, &ChipCalibrationController::startPlacement);
    connect(m_btnConfirm, &QPushButton::clicked, this, &ChipCalibrationController::onConfirmClicked);
    connect(m_btnDelete, &QPushButton::clicked, this, &ChipCalibrationController::onDeleteClicked);
    connect(m_btnUp, &QPushButton::clicked, this, &ChipCalibrationController::onUpClicked);
    connect(m_btnDown, &QPushButton::clicked, this, &ChipCalibrationController::onDownClicked);
    // 选点态下点中虚色候选点 → 选中它并回填表格（图上不再有“自由拾取”路径）
    connect(m_view, &WaferGraphicsView::sig_levelingCandidateClicked,
            this, &ChipCalibrationController::onCandidateClicked);
    updateButtonStates();
}

void ChipCalibrationController::setUiEditable(bool editable)
{
    if (m_uiEditable == editable)
    {
        updateButtonStates();
        return;
    }
    m_uiEditable = editable;
    if (!m_uiEditable)
    {
        cancelPlacement();   // 退出编辑：中断未确认的选点
    }

    updateButtonStates();
}

void ChipCalibrationController::setCandidatePoints(const std::vector<S_Point2D> &points)
{
    // 下发候选点后立刻刷新按钮态：候选点为空 → Add/Delete/上移/下移 全部置灰
    m_candidates = points;
    updateButtonStates();
}

void ChipCalibrationController::startPlacement() // 进入选点态：追加待确认行 + 画出候选点
{
    if (m_placing)
        return;
    if (m_table->rowCount() >= limitedNum)   // 达到上限：不再追加行/进入选点态
        return;
    if (m_candidates.empty())                // 无候选点（chip 处方没有下对位点）：Add 不生效
        return;

    m_placing = true;
    m_pendingPlaced = false;
    m_pendingIndex = -1;   // 复位上一轮的选择：否则再选同一序号会被 onCandidateClicked 当作重复点击忽略
    m_pendingRow = m_table->rowCount();
    m_table->insertRow(m_pendingRow);
    m_table->setItem(m_pendingRow, 0, new QTableWidgetItem(QString()));
    m_table->setItem(m_pendingRow, 1, new QTableWidgetItem(QString()));
    m_table->setCurrentCell(m_pendingRow, 0);

    updateButtonStates();
    // 候选点先按虚色画上去（已被表格占用的序号不画），再开启选点态使其可点
    m_view->SetLevelingCandidates(m_candidates, 0, usedCandidateIndices());
    m_view->SetLevelingPickMode(true);
}

void ChipCalibrationController::onCandidateClicked(int index, int table) // 选中候选点：变实色 + 回填该行
{
    if (!m_placing || table != 0)
        return;                                              // 非选点态 / 非本表：忽略
    if (index < 0 || index >= static_cast<int>(m_candidates.size()))
        return;                                              // 序号越界：忽略
    if (index == m_pendingIndex)
        return;                                              // 重复点同一个：幂等

    m_pendingIndex = index;
    m_view->SetLevelingCandidateSelected(index, 0);          // 该点转实色，其余自动回虚色（即改选）
    m_pendingPlaced = true;                                  // 选好点后 Confirm 才可点

    // 坐标回填表格（保留 3 位小数）
    const S_Point2D &p = m_candidates[index];
    m_table->setItem(m_pendingRow, 0, new QTableWidgetItem(QString::number(p.x_, 'f', 3)));
    m_table->setItem(m_pendingRow, 1, new QTableWidgetItem(QString::number(p.y_, 'f', 3)));
    updateButtonStates();
}

void ChipCalibrationController::onConfirmClicked()
{
    if (!m_placing || !m_pendingPlaced || m_pendingIndex < 0)
        return;   // 未选中候选点时 Confirm 无意义（按钮本已禁用，双保险）

    // 一步建点即锁定：待确认期间图上只有候选点图元，Confirm 才把它落成正式实色标记
    m_view->SetLevelingPoint(m_pendingRow, m_candidates[m_pendingIndex],
                             QStringLiteral("Calibration"), 0, /*locked=*/true);

    m_placing = false;
    m_pendingPlaced = false;
    m_pendingRow = -1;
    m_pendingIndex = -1;
    m_view->SetLevelingPickMode(false);      // 退出选点态
    m_view->ClearLevelingCandidates();       // 清空候选点层（已确认的位置不再显示虚色点）
    updateButtonStates();
    emit dataModified();
}

QList<int> ChipCalibrationController::usedCandidateIndices() const
{
    // 表格文本按 3 位小数往返 → 用 1e-3 容差匹配，避免浮点不等
    QList<int> used;
    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        QTableWidgetItem *ix = m_table->item(r, 0), *iy = m_table->item(r, 1);
        if (!ix || !iy) continue;
        bool okX = false, okY = false;
        const double x = ix->text().toDouble(&okX), y = iy->text().toDouble(&okY);
        if (!okX || !okY) continue;
        for (int i = 0; i < m_candidates.size(); ++i)
            if (qAbs(m_candidates[i].x_ - x) <= 1e-3 && qAbs(m_candidates[i].y_ - y) <= 1e-3)
            { if (!used.contains(i)) used.append(i); break; }
    }
    return used;
}

void ChipCalibrationController::onDeleteClicked()
{
    if (m_placing)
        return;   // 放置态由 Confirm/取消管理

    const int row = m_table->currentRow();
    if (row < 0)
        return;

    m_table->removeRow(row);
    m_view->RemoveLevelingPoint(row, 0);   // 删除该行圆点，后续行号整体 -1（与表格行保持一致）
    updateButtonStates();
    emit dataModified();
}

void ChipCalibrationController::onUpClicked()
{
    if (m_placing)
        return;

    const int row = m_table->currentRow();
    if (row <= 0)
        return;

    {
        // 重排期间屏蔽表格信号，避免中间态干扰其它逻辑
        QSignalBlocker blocker(m_table);
        QStringList xy;
        for (int c = 0; c < m_table->columnCount(); ++c)
        {
            QTableWidgetItem *it = m_table->takeItem(row, c);
            xy << (it ? it->text() : QString());
            delete it;
        }
        m_table->removeRow(row);
        m_table->insertRow(row - 1);
        for (int c = 0; c < xy.size(); ++c)
            m_table->setItem(row - 1, c, new QTableWidgetItem(xy.at(c)));
    }

    // 两行圆点（含锁定状态）互换，其余标记不受影响
    m_view->MoveLevelingPoint(row, row - 1, 0);
    m_table->setCurrentCell(row - 1, 0);
    updateButtonStates();
    emit dataModified();
}

void ChipCalibrationController::onDownClicked()
{
    if (m_placing)
        return;

    const int row = m_table->currentRow();
    if (row < 0 || row >= m_table->rowCount() - 1)
        return;

    {
        QSignalBlocker blocker(m_table);
        QStringList xy;
        for (int c = 0; c < m_table->columnCount(); ++c)
        {
            QTableWidgetItem *it = m_table->takeItem(row, c);
            xy << (it ? it->text() : QString());
            delete it;
        }
        m_table->removeRow(row);
        m_table->insertRow(row + 1);
        for (int c = 0; c < xy.size(); ++c)
            m_table->setItem(row + 1, c, new QTableWidgetItem(xy.at(c)));
    }

    m_view->MoveLevelingPoint(row, row + 1, 0);
    m_table->setCurrentCell(row + 1, 0);
    updateButtonStates();
    emit dataModified();
}

void ChipCalibrationController::cancelPlacement()
{
    // 丢弃未确认内容：删除待选点行（Add 追加的末行）。
    // 选点态期间图上只有候选点图元、没有待确认标记，RemoveLevelingPointExact 仅为双保险。
    if (m_placing && m_pendingRow >= 0 && m_pendingRow < m_table->rowCount())
    {
        m_view->RemoveLevelingPointExact(m_pendingRow, 0);
        m_table->removeRow(m_pendingRow);
    }
    m_placing = false;
    m_pendingPlaced = false;
    m_pendingRow = -1;
    m_pendingIndex = -1;
    m_view->SetLevelingPickMode(false);      // 退出选点态
    m_view->ClearLevelingCandidates();       // 清空候选点层，避免虚色点残留在图上
    updateButtonStates();
}

void ChipCalibrationController::loadPoints(const std::vector<S_NominalPoint> &points)
{
    cancelPlacement();   // 先丢弃进行中的放置（Add 追加的末行随之下方重建）

    m_view->ClearLevelingPoints();
    m_table->setRowCount(static_cast<int>(points.size()));
    for (int r = 0; r < static_cast<int>(points.size()); ++r)
    {
        m_table->setItem(r, 0, new QTableWidgetItem(QString::number(points[r].point.x_)));
        m_table->setItem(r, 1, new QTableWidgetItem(QString::number(points[r].point.y_)));
        // 处方里的 Chip 标定点都是已确认点：直接创建为锁定标记
        m_view->SetLevelingPoint(r, QPointF(points[r].point.x_, points[r].point.y_),
                                 QStringLiteral("Calibration"), 0, /*locked=*/true);
    }
    updateButtonStates();
}

std::vector<S_NominalPoint> ChipCalibrationController::collectPoints() const
{
    std::vector<S_NominalPoint> out;
    out.reserve(static_cast<size_t>(m_table->rowCount()));
    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        S_NominalPoint np;
        const QTableWidgetItem *ix = m_table->item(r, 0);
        const QTableWidgetItem *iy = m_table->item(r, 1);
        np.point = S_Point2D(ix ? ix->text().toDouble() : 0.0,
                             iy ? iy->text().toDouble() : 0.0);
        out.push_back(np);
    }
    return out;
}

void ChipCalibrationController::updateButtonStates()
{
    const bool hasCandidates = !m_candidates.empty();   // 无候选点：整组按钮置灰

    // 常规态（可编辑、不在选点态、且有候选点）：Add/Delete/上移/下移可用，Confirm 置灰
    const bool idle = m_uiEditable && !m_placing && hasCandidates;
    const bool canAddMore = m_table->rowCount() < limitedNum;   // 上限控制：最多 limitedNum 个

    // 选点态：全部置灰，仅 Confirm 在选好点后可点
    m_btnAdd->setEnabled(idle && canAddMore);
    m_btnDelete->setEnabled(idle);
    m_btnUp->setEnabled(idle);
    m_btnDown->setEnabled(idle);
    m_btnConfirm->setEnabled(m_uiEditable && m_placing && m_pendingPlaced);
}

std::vector<S_Point2D> ChipCalibrationController::getAllMarkCandidates()
{
    return m_candidates;
}
void ChipCalibrationController::setAllMarkCandidates(std::vector<S_Point2D> value)
{
    m_candidates = value;
}
