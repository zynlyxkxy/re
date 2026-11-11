#include "LevelTab.h"

#include "ui_RecipeModule.h"
#include "ReData/S_Recipe.h"
#include "Point/PointCoordDelegate.h"
#include "PointTableOps.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <utility>

LevelTab::LevelTab(Ui::RecipeModule *ui, std::function<void()> onModified)
    : m_ui(ui)
    , m_onModified(std::move(onModified))
{
    // 三张调平点位表：x/y 列只允许输入数字（其余列 markId 等保持默认编辑器）
    auto applyPointCoordDelegate = [this](QTableWidget *tw) {
        tw->setItemDelegate(new PointCoordDelegate(tw));
    };
    applyPointCoordDelegate(m_ui->tw_coarseLevelPoints);
    applyPointCoordDelegate(m_ui->tw_upLevelPoints);
    applyPointCoordDelegate(m_ui->tw_dwLevelPoints);
}

void LevelTab::setConnect()
{
    // 粗调平/上调平/下调平点位表的增删/上下移
    PointTableOps::bindRowButtons(
        m_ui->tw_coarseLevelPoints, m_ui->btn_add_coarseLevelPoints, m_ui->btn_delete_coarseLevelPoints,
        m_ui->btn_upMove_coarseLevelPoints, m_ui->btn_dwMove_coarseLevelPoints, m_onModified);
    PointTableOps::bindRowButtons(
        m_ui->tw_upLevelPoints, m_ui->btn_add_upLevelPoints, m_ui->btn_delete_upLevelPoints,
        m_ui->btn_upMove_upLevelPoints, m_ui->btn_dwMove_upLevelPoints, m_onModified);
    PointTableOps::bindRowButtons(
        m_ui->tw_dwLevelPoints, m_ui->btn_add_dwLevelPoints, m_ui->btn_delete_dwLevelPoints,
        m_ui->btn_upMove_dwLevelPoints, m_ui->btn_dwMove_dwLevelPoints, m_onModified);

    // 其余控件（spin/checkbox/表格）的值变化 → 标记已修改由 RecipeModule 的通用循环统一处理
}

void LevelTab::load(const S_RecipeChipLevel &level)
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

    resizeTable(m_ui->tw_coarseLevelPoints, static_cast<int>(level.lst_coarseLevelPoints.size()));
    for (int r = 0; r < static_cast<int>(level.lst_coarseLevelPoints.size()); ++r)
    {
        const S_TakePhotoNominalPoint &p = level.lst_coarseLevelPoints[r];
        setRowItems(m_ui->tw_coarseLevelPoints, r, p.point.x_, p.point.y_, QString::fromStdString(p.markId));
    }
    resizeTable(m_ui->tw_upLevelPoints, static_cast<int>(level.lst_upLevelPoints.size()));
    for (int r = 0; r < static_cast<int>(level.lst_upLevelPoints.size()); ++r)
    {
        const S_TakePhotoNominalPoint &p = level.lst_upLevelPoints[r];
        setRowItems(m_ui->tw_upLevelPoints, r, p.point.x_, p.point.y_, QString::fromStdString(p.markId));
    }
    resizeTable(m_ui->tw_dwLevelPoints, static_cast<int>(level.lst_dwLevelPoints.size()));
    for (int r = 0; r < static_cast<int>(level.lst_dwLevelPoints.size()); ++r)
    {
        const S_TakePhotoNominalPoint &p = level.lst_dwLevelPoints[r];
        setRowItems(m_ui->tw_dwLevelPoints, r, p.point.x_, p.point.y_, QString::fromStdString(p.markId));
    }

    m_ui->dsb_upLevelMarkSearch_width->setValue(level.upMarkSearchSetting.searchWidth);
    m_ui->dsb_upLevelMarkSearch_height->setValue(level.upMarkSearchSetting.searchHeight);
    m_ui->dsb_upLevelMarkSearch_circles->setValue(level.upMarkSearchSetting.searchCircles);
    m_ui->dsb_dwLevelMarkSearch_width->setValue(level.dwMarkSearchSetting.searchWidth);
    m_ui->dsb_dwLevelMarkSearch_height->setValue(level.dwMarkSearchSetting.searchHeight);
    m_ui->dsb_dwLevelMarkSearch_circles->setValue(level.dwMarkSearchSetting.searchCircles);

    m_ui->dsb_levelOffsetRx->setValue(level.levelSetting.rxLevelOffset);
    m_ui->dsb_levelOffsetRy->setValue(level.levelSetting.ryLevelOffset);
    m_ui->dsb_checkFineLevelTimes->setValue(level.levelSetting.checkFineLevelTimes);

    m_ui->dsb_levelFocalPlaneOffset_upChip->setValue(level.upChipLevelFocalPlaneOffset);
    m_ui->dsb_levelFocalPlaneOffset_dwChip->setValue(level.dwChipLevelFocalPlaneOffset);

    m_ui->dsb_fineLevelResultToleranceRange_Rx->setValue(level.fineLevelResultToleranceRange.rxToleranceRange);
    m_ui->dsb_fineLevelResultToleranceRange_Ry->setValue(level.fineLevelResultToleranceRange.ryToleranceRange);
    m_ui->dsb_fineLevelResultToleranceRange_Z->setValue(level.fineLevelResultToleranceRange.zToleranceRange);
}

void LevelTab::save(S_RecipeChipLevel &level) const
{
    auto cellVal = [](QTableWidget *tw, int r, int c) {
        QTableWidgetItem *it = tw->item(r, c);
        return it ? it->text().toDouble() : 0.0;
    };
    auto cellText = [](QTableWidget *tw, int r, int c) {
        QTableWidgetItem *it = tw->item(r, c);
        return it ? it->text() : QString();
    };

    level.lst_coarseLevelPoints.clear();
    for (int r = 0; r < m_ui->tw_coarseLevelPoints->rowCount(); ++r)
    {
        S_TakePhotoNominalPoint p;
        p.point = S_Point2D(cellVal(m_ui->tw_coarseLevelPoints, r, 0), cellVal(m_ui->tw_coarseLevelPoints, r, 1));
        p.markId = cellText(m_ui->tw_coarseLevelPoints, r, 2).toStdString();
        level.lst_coarseLevelPoints.push_back(p);
    }
    level.lst_upLevelPoints.clear();
    for (int r = 0; r < m_ui->tw_upLevelPoints->rowCount(); ++r)
    {
        S_TakePhotoNominalPoint p;
        p.point = S_Point2D(cellVal(m_ui->tw_upLevelPoints, r, 0), cellVal(m_ui->tw_upLevelPoints, r, 1));
        p.markId = cellText(m_ui->tw_upLevelPoints, r, 2).toStdString();
        level.lst_upLevelPoints.push_back(p);
    }
    level.lst_dwLevelPoints.clear();
    for (int r = 0; r < m_ui->tw_dwLevelPoints->rowCount(); ++r)
    {
        S_TakePhotoNominalPoint p;
        p.point = S_Point2D(cellVal(m_ui->tw_dwLevelPoints, r, 0), cellVal(m_ui->tw_dwLevelPoints, r, 1));
        p.markId = cellText(m_ui->tw_dwLevelPoints, r, 2).toStdString();
        level.lst_dwLevelPoints.push_back(p);
    }

    level.upMarkSearchSetting.searchWidth = m_ui->dsb_upLevelMarkSearch_width->value();
    level.upMarkSearchSetting.searchHeight = m_ui->dsb_upLevelMarkSearch_height->value();
    level.upMarkSearchSetting.searchCircles = static_cast<int>(m_ui->dsb_upLevelMarkSearch_circles->value());
    level.dwMarkSearchSetting.searchWidth = m_ui->dsb_dwLevelMarkSearch_width->value();
    level.dwMarkSearchSetting.searchHeight = m_ui->dsb_dwLevelMarkSearch_height->value();
    level.dwMarkSearchSetting.searchCircles = static_cast<int>(m_ui->dsb_dwLevelMarkSearch_circles->value());

    level.levelSetting.rxLevelOffset = m_ui->dsb_levelOffsetRx->value();
    level.levelSetting.ryLevelOffset = m_ui->dsb_levelOffsetRy->value();
    level.levelSetting.checkFineLevelTimes = static_cast<int>(m_ui->dsb_checkFineLevelTimes->value());

    level.upChipLevelFocalPlaneOffset = m_ui->dsb_levelFocalPlaneOffset_upChip->value();
    level.dwChipLevelFocalPlaneOffset = m_ui->dsb_levelFocalPlaneOffset_dwChip->value();

    level.fineLevelResultToleranceRange.rxToleranceRange = m_ui->dsb_fineLevelResultToleranceRange_Rx->value();
    level.fineLevelResultToleranceRange.ryToleranceRange = m_ui->dsb_fineLevelResultToleranceRange_Ry->value();
    level.fineLevelResultToleranceRange.zToleranceRange = m_ui->dsb_fineLevelResultToleranceRange_Z->value();
    // zToleranceRange 无对应控件，保持默认值
}
