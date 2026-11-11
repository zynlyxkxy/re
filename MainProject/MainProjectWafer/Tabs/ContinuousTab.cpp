#include "ContinuousTab.h"

#include "ui_RecipeModule.h"
#include "ReData/S_Recipe.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <utility>

namespace {
// 连续键合点偏移固定 4 个点（UI: Point 1~4）
constexpr int kPointOffsetCount = 4;
// 键合前后识别模板表格：4 行 × 2 列，列 0=Before Bond，列 1=After Bond
constexpr int kTemplateRowCount = 4;
constexpr int kTemplateColumnBefore = 0;
constexpr int kTemplateColumnAfter = 1;

// 表格行 → 结构体字段：行序 = X Trapezoid / X Bar / Y Trapezoid / Y Bar
using TemplateField = std::string S_RecipeWaferContinuousBondTemplate::*;
constexpr TemplateField kTemplateFields[kTemplateRowCount] = {
    &S_RecipeWaferContinuousBondTemplate::trapezoidX,
    &S_RecipeWaferContinuousBondTemplate::barX,
    &S_RecipeWaferContinuousBondTemplate::trapezoidY,
    &S_RecipeWaferContinuousBondTemplate::barY,
};
}

ContinuousTab::ContinuousTab(Ui::RecipeModule *ui, std::function<void()> onModified)
    : m_ui(ui)
    , m_onModified(std::move(onModified))
{
}

void ContinuousTab::setConnect()
{
    // 本页无页内按钮/视图联动；spinbox/lineEdit 的值变化 → 标记已修改
    // 由 RecipeModule::setConnections() 的通用循环统一处理（表格走 itemChanged）。
}

std::array<QDoubleSpinBox *, 4> ContinuousTab::offsetSpinBoxes(int index) const
{
    switch (index)
    {
    case 0: return {m_ui->point1X_X, m_ui->point1X_Y, m_ui->point1Y_X, m_ui->point1Y_Y};
    case 1: return {m_ui->point2X_X, m_ui->point2X_Y, m_ui->point2Y_X, m_ui->point2Y_Y};
    case 2: return {m_ui->point3X_X, m_ui->point3X_Y, m_ui->point3Y_X, m_ui->point3Y_Y};
    case 3: return {m_ui->point4X_X, m_ui->point4X_Y, m_ui->point4Y_X, m_ui->point4Y_Y};
    default: return {nullptr, nullptr, nullptr, nullptr};
    }
}

void ContinuousTab::load(const S_RecipeWaferContinuous &continuous)
{
    m_ui->dsb_levelingStageDownHeight->setValue(continuous.levelingStageDownHeight);
    m_ui->dsb_bondTimes->setValue(continuous.bondTimes);

    // 键合前后识别模板：前/后两列各取 1 个元素（处方为空时按空模板填充）
    QTableWidget *twTemplate = m_ui->tw_bondRecognitionTemplate;
    S_RecipeWaferContinuousBondTemplate beforeSet;
    S_RecipeWaferContinuousBondTemplate afterSet;
    if (!continuous.lst_beforeBondRecognitionTemplate.empty())
        beforeSet = continuous.lst_beforeBondRecognitionTemplate.front();
    if (!continuous.lst_afterBondRecognitionTemplate.empty())
        afterSet = continuous.lst_afterBondRecognitionTemplate.front();
    for (int r = 0; r < twTemplate->rowCount() && r < kTemplateRowCount; ++r)
    {
        twTemplate->setItem(r, kTemplateColumnBefore,
                            new QTableWidgetItem(QString::fromStdString(beforeSet.*kTemplateFields[r])));
        twTemplate->setItem(r, kTemplateColumnAfter,
                            new QTableWidgetItem(QString::fromStdString(afterSet.*kTemplateFields[r])));
    }

    // Point 1~4 偏移：处方里点数不足时按 0 填充，超出部分忽略
    const std::vector<S_RecipeWaferContinuousPointOffset> &offsets = continuous.lst_pointOffset;
    for (int i = 0; i < kPointOffsetCount; ++i)
    {
        const S_RecipeWaferContinuousPointOffset offset =
            (i < static_cast<int>(offsets.size())) ? offsets[i] : S_RecipeWaferContinuousPointOffset();
        const std::array<QDoubleSpinBox *, 4> sbs = offsetSpinBoxes(i);
        sbs[0]->setValue(offset.xOffsetX);
        sbs[1]->setValue(offset.xOffsetY);
        sbs[2]->setValue(offset.yOffsetX);
        sbs[3]->setValue(offset.yOffsetY);
    }
}

void ContinuousTab::save(S_RecipeWaferContinuous &continuous) const
{
    continuous.levelingStageDownHeight = m_ui->dsb_levelingStageDownHeight->value();
    // Bonding Times 输入框 decimals=0，取值即整数
    continuous.bondTimes = static_cast<int>(m_ui->dsb_bondTimes->value());

    // 键合前后识别模板：表格两列 → before/after 各 1 个元素（行序 = 字段序）
    const QTableWidget *twTemplate = m_ui->tw_bondRecognitionTemplate;
    S_RecipeWaferContinuousBondTemplate beforeSet;
    S_RecipeWaferContinuousBondTemplate afterSet;
    for (int r = 0; r < twTemplate->rowCount() && r < kTemplateRowCount; ++r)
    {
        const QTableWidgetItem *before = twTemplate->item(r, kTemplateColumnBefore);
        const QTableWidgetItem *after = twTemplate->item(r, kTemplateColumnAfter);
        beforeSet.*kTemplateFields[r] = before ? before->text().toStdString() : std::string();
        afterSet.*kTemplateFields[r] = after ? after->text().toStdString() : std::string();
    }
    continuous.lst_beforeBondRecognitionTemplate.assign(1, beforeSet);
    continuous.lst_afterBondRecognitionTemplate.assign(1, afterSet);

    continuous.lst_pointOffset.clear();
    continuous.lst_pointOffset.reserve(kPointOffsetCount);
    for (int i = 0; i < kPointOffsetCount; ++i)
    {
        const std::array<QDoubleSpinBox *, 4> sbs = offsetSpinBoxes(i);
        S_RecipeWaferContinuousPointOffset offset;
        offset.xOffsetX = sbs[0]->value();
        offset.xOffsetY = sbs[1]->value();
        offset.yOffsetX = sbs[2]->value();
        offset.yOffsetY = sbs[3]->value();
        continuous.lst_pointOffset.push_back(offset);
    }
}
