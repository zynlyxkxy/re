#include "ChuckLevelTab.h"

#include "ui_RecipeModule.h"
#include "ReData/S_Recipe.h"
#include "GraphicsView/WaferGraphicsView.h"
#include "Point/PointCoordDelegate.h"

#include <QPushButton>
#include <QObject>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QSignalBlocker>
#include <utility>
#include <vector>

ChuckLevelTab::ChuckLevelTab(Ui::RecipeModule *ui, std::function<void()> onModified)
    : m_ui(ui)
    , m_onModified(std::move(onModified))
{
    auto config = [](QTableWidget *tw) {
        tw->setItemDelegate(new PointCoordDelegate(tw));
        tw->setSelectionBehavior(QAbstractItemView::SelectRows);
        tw->setSelectionMode(QAbstractItemView::SingleSelection);
    };
    config(m_ui->tw_upChuckAlign);
    config(m_ui->tw_downChuckAlign);
}

int ChuckLevelTab::tableIndexOf(QTableWidget *tw) const
{
    return (tw == m_ui->tw_downChuckAlign) ? 1 : 0;
}

void ChuckLevelTab::setConnect()
{
    // 上吸盘调平点位：增删/上下移
    QObject::connect(m_ui->btn_add_upChuckAlign, &QPushButton::clicked,
                     [this]() { onAddRow(m_ui->tw_upChuckAlign); });
    QObject::connect(m_ui->btn_delete_upChuckAlign, &QPushButton::clicked,
                     [this]() { onDeleteRow(m_ui->tw_upChuckAlign); });
    QObject::connect(m_ui->btn_upMove_upChuckAlign, &QPushButton::clicked,
                     [this]() { onMoveRowUp(m_ui->tw_upChuckAlign); });
    QObject::connect(m_ui->btn_dwMove_upChuckAlign, &QPushButton::clicked,
                     [this]() { onMoveRowDown(m_ui->tw_upChuckAlign); });
    // 下吸盘调平点位：增删/上下移
    QObject::connect(m_ui->btn_add_downChuckAlign, &QPushButton::clicked,
                     [this]() { onAddRow(m_ui->tw_downChuckAlign); });
    QObject::connect(m_ui->btn_delete_downChuckAlign, &QPushButton::clicked,
                     [this]() { onDeleteRow(m_ui->tw_downChuckAlign); });
    QObject::connect(m_ui->btn_upMove_downChuckAlign, &QPushButton::clicked,
                     [this]() { onMoveRowUp(m_ui->tw_downChuckAlign); });
    QObject::connect(m_ui->btn_dwMove_downChuckAlign, &QPushButton::clicked,
                     [this]() { onMoveRowDown(m_ui->tw_downChuckAlign); });

    // 调平点位行 X/Y 填写完成后自动显示该行标记；任一坐标被清空则移除该行标记
    auto bindItemChanged = [this](QTableWidget *tw, const QString &markType) {
        QObject::connect(tw, &QTableWidget::itemChanged, [this, tw, markType](QTableWidgetItem *item) {
            if (!item || item->row() < 0)
                return;
            const int row = item->row();
            QTableWidgetItem *ix = tw->item(row, 0);
            QTableWidgetItem *iy = tw->item(row, 1);
            bool okX = false, okY = false;
            const double x = ix ? ix->text().toDouble(&okX) : 0.0;
            const double y = iy ? iy->text().toDouble(&okY) : 0.0;
            if (okX && okY)
                m_ui->gv_preview_3->SetLevelingPoint(row, QPointF(x, y), markType, tableIndexOf(tw));
            else
                m_ui->gv_preview_3->RemoveLevelingPointExact(row, tableIndexOf(tw));
        });
    };
    bindItemChanged(m_ui->tw_upChuckAlign, QStringLiteral("Square"));
    bindItemChanged(m_ui->tw_downChuckAlign, QStringLiteral("Ellipse"));

    // 选中表格某行 → 该行标记点描边（gv_preview_3 上以 table 区分上/下吸盘）
    auto bindSelection = [this](QTableWidget *tw) {
        QObject::connect(tw, &QTableWidget::itemSelectionChanged, [this, tw]() {
            m_ui->gv_preview_3->SetLevelingSelectedRow(tw->currentRow(), tableIndexOf(tw));
        });
    };
    bindSelection(m_ui->tw_upChuckAlign);
    bindSelection(m_ui->tw_downChuckAlign);

    // 调平标记被拖动 → 回写对应表格行的 X/Y（保留 3 位小数）
    QObject::connect(m_ui->gv_preview_3, &WaferGraphicsView::sig_levelingPointMoved,
                     [this](int row, int table, const QPointF &nominalPos) {
        QTableWidget *tw = (table == 1) ? m_ui->tw_downChuckAlign : m_ui->tw_upChuckAlign;
        if (row < 0 || row >= tw->rowCount())
            return;
        tw->setItem(row, 0, new QTableWidgetItem(QString::number(nominalPos.x(), 'f', 3)));
        tw->setItem(row, 1, new QTableWidgetItem(QString::number(nominalPos.y(), 'f', 3)));
        m_onModified();
    });
    // 标记点被点击 → 高亮该标记并选中对应表格行
    QObject::connect(m_ui->gv_preview_3, &WaferGraphicsView::sig_levelingPointClicked,
                     [this](int row, int table) {
        QTableWidget *tw = (table == 1) ? m_ui->tw_downChuckAlign : m_ui->tw_upChuckAlign;
        if (row >= 0 && row < tw->rowCount())
            tw->selectRow(row);
    });
}

void ChuckLevelTab::setEditable(bool editable)
{
    m_ui->gv_preview_3->SetLevelingEditable(editable);
}

void ChuckLevelTab::load(const S_RecipeWaferChuckLevel &level)
{
    // 上吸盘
    m_ui->tw_upChuckAlign->setRowCount(static_cast<int>(level.lst_upChuckLevelingPoints.size()));
    for (int r = 0; r < static_cast<int>(level.lst_upChuckLevelingPoints.size()); ++r)
    {
        const S_NominalPoint &lp = level.lst_upChuckLevelingPoints[r];
        m_ui->tw_upChuckAlign->setItem(r, 0, new QTableWidgetItem(QString::number(lp.point.x_)));
        m_ui->tw_upChuckAlign->setItem(r, 1, new QTableWidgetItem(QString::number(lp.point.y_)));
    }
    m_ui->gv_preview_3->ClearLevelingPoints();
    for (int r = 0; r < static_cast<int>(level.lst_upChuckLevelingPoints.size()); ++r)
    {
        const S_NominalPoint &lp = level.lst_upChuckLevelingPoints[r];
        m_ui->gv_preview_3->SetLevelingPoint(r, QPointF(lp.point.x_, lp.point.y_), QStringLiteral("Square"));
    }
    // 下吸盘
    m_ui->tw_downChuckAlign->setRowCount(static_cast<int>(level.lst_downChuckLevelingPoints.size()));
    for (int r = 0; r < static_cast<int>(level.lst_downChuckLevelingPoints.size()); ++r)
    {
        const S_NominalPoint &lp = level.lst_downChuckLevelingPoints[r];
        m_ui->tw_downChuckAlign->setItem(r, 0, new QTableWidgetItem(QString::number(lp.point.x_)));
        m_ui->tw_downChuckAlign->setItem(r, 1, new QTableWidgetItem(QString::number(lp.point.y_)));
    }
    for (int r = 0; r < static_cast<int>(level.lst_downChuckLevelingPoints.size()); ++r)
    {
        const S_NominalPoint &lp = level.lst_downChuckLevelingPoints[r];
        m_ui->gv_preview_3->SetLevelingPoint(r, QPointF(lp.point.x_, lp.point.y_), QStringLiteral("Ellipse"), 1);
    }

    m_ui->dsb_chuckevelResultToleranceRangeRx->setValue(level.chuckLevelResultToleranceRange.rxToleranceRange);
    m_ui->dsb_chuckevelResultToleranceRangeRy->setValue(level.chuckLevelResultToleranceRange.ryToleranceRange);
}

void ChuckLevelTab::save(S_RecipeWaferChuckLevel &level) const
{
    auto collect = [](QTableWidget *tw) {
        std::vector<S_NominalPoint> out;
        out.reserve(static_cast<size_t>(tw->rowCount()));
        for (int r = 0; r < tw->rowCount(); ++r)
        {
            S_NominalPoint lp;
            QTableWidgetItem *ix = tw->item(r, 0);
            QTableWidgetItem *iy = tw->item(r, 1);
            lp.point = S_Point2D(ix ? ix->text().toDouble() : 0.0,
                                 iy ? iy->text().toDouble() : 0.0);
            out.push_back(lp);
        }
        return out;
    };
    level.lst_upChuckLevelingPoints = collect(m_ui->tw_upChuckAlign);
    level.lst_downChuckLevelingPoints = collect(m_ui->tw_downChuckAlign);

    level.chuckLevelResultToleranceRange.rxToleranceRange = m_ui->dsb_chuckevelResultToleranceRangeRx->value();
    level.chuckLevelResultToleranceRange.ryToleranceRange = m_ui->dsb_chuckevelResultToleranceRangeRy->value();
}

void ChuckLevelTab::onAddRow(QTableWidget *tw)
{
    const int row = tw->rowCount();
    tw->insertRow(row);
    tw->setCurrentCell(row, 0);
    m_onModified();
}

void ChuckLevelTab::onDeleteRow(QTableWidget *tw)
{
    const int row = tw->currentRow();
    if (row < 0)
        return;
    tw->removeRow(row);
    // 删除该行时同步删除其对应标记点，并使其后所有标记行号 -1
    m_ui->gv_preview_3->RemoveLevelingPoint(row, tableIndexOf(tw));
    m_onModified();
}

void ChuckLevelTab::onMoveRowUp(QTableWidget *tw)
{
    int row = tw->currentRow();
    if (row <= 0)
        return;

    QList<QTableWidgetItem *> items;
    QList<QWidget *> widgets;
    for (int c = 0; c < tw->columnCount(); ++c)
    {
        items.append(tw->takeItem(row, c));
        widgets.append(tw->cellWidget(row, c));
        tw->removeCellWidget(row, c);
    }
    {   // 重排期间屏蔽表格信号，避免半行中间态触发 itemChanged 误删其它行标记
        QSignalBlocker blocker(tw);
        tw->removeRow(row);
        tw->insertRow(row - 1);
        for (int c = 0; c < tw->columnCount(); ++c)
        {
            tw->setItem(row - 1, c, items.at(c));
            if (widgets.at(c))
                tw->setCellWidget(row - 1, c, widgets.at(c));
        }
    }
    // 行号互换，同步交换两行标记点
    m_ui->gv_preview_3->MoveLevelingPoint(row, row - 1, tableIndexOf(tw));

    tw->setCurrentCell(row - 1, 0);
    m_onModified();
}

void ChuckLevelTab::onMoveRowDown(QTableWidget *tw)
{
    int row = tw->currentRow();
    if (row < 0 || row >= tw->rowCount() - 1)
        return;

    QList<QTableWidgetItem *> items;
    QList<QWidget *> widgets;
    for (int c = 0; c < tw->columnCount(); ++c)
    {
        items.append(tw->takeItem(row, c));
        widgets.append(tw->cellWidget(row, c));
        tw->removeCellWidget(row, c);
    }
    {   // 重排期间屏蔽表格信号，避免半行中间态触发 itemChanged 误删其它行标记
        QSignalBlocker blocker(tw);
        tw->removeRow(row);
        tw->insertRow(row + 1);
        for (int c = 0; c < tw->columnCount(); ++c)
        {
            tw->setItem(row + 1, c, items.at(c));
            if (widgets.at(c))
                tw->setCellWidget(row + 1, c, widgets.at(c));
        }
    }
    // 行号互换，同步交换两行标记点
    m_ui->gv_preview_3->MoveLevelingPoint(row, row + 1, tableIndexOf(tw));

    tw->setCurrentCell(row + 1, 0);
    m_onModified();
}
