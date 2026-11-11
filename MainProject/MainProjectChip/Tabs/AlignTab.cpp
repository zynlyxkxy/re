#include "AlignTab.h"

#include "ui_RecipeModule.h"
#include "ReData/S_Recipe.h"
#include "Point/PointCoordDelegate.h"
#include "PointTableOps.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <utility>

AlignTab::AlignTab(Ui::RecipeModule *ui, std::function<void()> onModified)
    : m_ui(ui)
    , m_onModified(std::move(onModified))
{
    // 本页点位表：x/y 列只允许输入数字（其余列 markId 等保持默认编辑器）
    auto applyPointCoordDelegate = [this](QTableWidget *tw) {
        tw->setItemDelegate(new PointCoordDelegate(tw));
    };
    applyPointCoordDelegate(m_ui->tw_upAlignPoints);
    applyPointCoordDelegate(m_ui->tw_downAlignPoints);
}

void AlignTab::setConnect()
{
    // 上/下对准点位表的增删/上下移
    PointTableOps::bindRowButtons(
        m_ui->tw_upAlignPoints, m_ui->btn_add_upAlignPoints, m_ui->btn_delete_upAlignPoints,
        m_ui->btn_upMove_upAlignPoints, m_ui->btn_dwMove_upAlignPoints, m_onModified);
    PointTableOps::bindRowButtons(
        m_ui->tw_downAlignPoints, m_ui->btn_add_downAlignPoints, m_ui->btn_delete_downAlignPoints,
        m_ui->btn_upMove_downAlignPoints, m_ui->btn_dwMove_downAlignPoints, m_onModified);

    // 其余控件（spin/checkbox/表格）的值变化 → 标记已修改由 RecipeModule 的通用循环统一处理
}

void AlignTab::load(const S_RecipeChipAlign &align)
{
    auto setRowItems = [](QTableWidget *tw, int r, double x, double y, const QString &mark)
    {
        tw->setItem(r, 0, new QTableWidgetItem(QString::number(x)));
        tw->setItem(r, 1, new QTableWidgetItem(QString::number(y)));
        tw->setItem(r, 2, new QTableWidgetItem(mark));
    };
    auto resizeTable = [](QTableWidget *tw, int rows)
    {
        tw->setRowCount(0);
        tw->setRowCount(rows);
    };

    resizeTable(m_ui->tw_upAlignPoints, static_cast<int>(align.lst_upAlignPoints.size()));
    for (int r = 0; r < static_cast<int>(align.lst_upAlignPoints.size()); ++r)
    {
        const S_RecipeUpAlignNominalPoint &p = align.lst_upAlignPoints[r];
        setRowItems(m_ui->tw_upAlignPoints, r, p.point.x_, p.point.y_,
                    QString::fromStdString(p.upMarkIdWithXNCam));
    }
    resizeTable(m_ui->tw_downAlignPoints, static_cast<int>(align.lst_downAlignPoints.size()));
    for (int r = 0; r < static_cast<int>(align.lst_downAlignPoints.size()); ++r)
    {
        const S_RecipeDownAlignNominalPoint &p = align.lst_downAlignPoints[r];
        setRowItems(m_ui->tw_downAlignPoints, r, p.point.x_, p.point.y_,
                    QString::fromStdString(p.dwMarkId));
    }

    m_ui->dsb_upAlignMarkSearch_width->setValue(align.upMarkSearchSetting.searchWidth);
    m_ui->dsb_upAlignMarkSearch_height->setValue(align.upMarkSearchSetting.searchHeight);
    m_ui->dsb_upAlignMarkSearch_circles->setValue(align.upMarkSearchSetting.searchCircles);
    m_ui->dsb_dwAlignMarkSearch_width->setValue(align.dwMarkSearchSetting.searchWidth);
    m_ui->dsb_dwAlignMarkSearch_height->setValue(align.dwMarkSearchSetting.searchHeight);
    m_ui->dsb_dwAlignMarkSearch_circles->setValue(align.dwMarkSearchSetting.searchCircles);

    m_ui->dsb_fineAlignResultToleranceRange_X->setValue(align.fineAlignResultToleranceRange.xToleranceRange);
    m_ui->dsb_fineAlignResultToleranceRange_Y->setValue(align.fineAlignResultToleranceRange.yToleranceRange);
    m_ui->dsb_fineAlignResultToleranceRange_Rz->setValue(align.fineAlignResultToleranceRange.rzToleranceRange);

    m_ui->checkBox_autoProcessAlignResult->setChecked(align.autoProcessAlignResult);
    m_ui->checkBox_levelPointForFineAlign->setChecked(align.levelPointForFineAlign);
    m_ui->dsb_checkAlignTimes->setValue(align.checkAlignTimes);
}

void AlignTab::save(S_RecipeChipAlign &align) const
{
    auto cellVal = [](QTableWidget *tw, int r, int c) {
        QTableWidgetItem *it = tw->item(r, c);
        return it ? it->text().toDouble() : 0.0;
    };
    auto cellText = [](QTableWidget *tw, int r, int c) {
        QTableWidgetItem *it = tw->item(r, c);
        return it ? it->text() : QString();
    };

    align.lst_upAlignPoints.clear();
    for (int r = 0; r < m_ui->tw_upAlignPoints->rowCount(); ++r)
    {
        S_RecipeUpAlignNominalPoint p;
        p.point = S_Point2D(cellVal(m_ui->tw_upAlignPoints, r, 0), cellVal(m_ui->tw_upAlignPoints, r, 1));
        p.upMarkIdWithXNCam = cellText(m_ui->tw_upAlignPoints, r, 2).toStdString();
        align.lst_upAlignPoints.push_back(p);
    }
    align.lst_downAlignPoints.clear();
    for (int r = 0; r < m_ui->tw_downAlignPoints->rowCount(); ++r)
    {
        S_RecipeDownAlignNominalPoint p;
        p.point = S_Point2D(cellVal(m_ui->tw_downAlignPoints, r, 0), cellVal(m_ui->tw_downAlignPoints, r, 1));
        p.dwMarkId = cellText(m_ui->tw_downAlignPoints, r, 2).toStdString();
        align.lst_downAlignPoints.push_back(p);
    }

    align.upMarkSearchSetting.searchWidth = m_ui->dsb_upAlignMarkSearch_width->value();
    align.upMarkSearchSetting.searchHeight = m_ui->dsb_upAlignMarkSearch_height->value();
    align.upMarkSearchSetting.searchCircles = static_cast<int>(m_ui->dsb_upAlignMarkSearch_circles->value());
    align.dwMarkSearchSetting.searchWidth = m_ui->dsb_dwAlignMarkSearch_width->value();
    align.dwMarkSearchSetting.searchHeight = m_ui->dsb_dwAlignMarkSearch_height->value();
    align.dwMarkSearchSetting.searchCircles = static_cast<int>(m_ui->dsb_dwAlignMarkSearch_circles->value());

    align.fineAlignResultToleranceRange.xToleranceRange = m_ui->dsb_fineAlignResultToleranceRange_X->value();
    align.fineAlignResultToleranceRange.yToleranceRange = m_ui->dsb_fineAlignResultToleranceRange_Y->value();
    align.fineAlignResultToleranceRange.rzToleranceRange = m_ui->dsb_fineAlignResultToleranceRange_Rz->value();

    align.autoProcessAlignResult = m_ui->checkBox_autoProcessAlignResult->isChecked();
    align.levelPointForFineAlign = m_ui->checkBox_levelPointForFineAlign->isChecked();
    align.checkAlignTimes = static_cast<int>(m_ui->dsb_checkAlignTimes->value());
}
