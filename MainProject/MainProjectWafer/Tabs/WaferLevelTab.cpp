#include "WaferLevelTab.h"

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

WaferLevelTab::WaferLevelTab(Ui::RecipeModule *ui, std::function<void()> onModified)
    : m_ui(ui)
    , m_onModified(std::move(onModified))
{
    // 调平点位表：x/y 列只允许输入数字，整行选中
    m_ui->tw_waferLevelingPoints->setItemDelegate(new PointCoordDelegate(m_ui->tw_waferLevelingPoints));
    m_ui->tw_waferLevelingPoints->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui->tw_waferLevelingPoints->setSelectionMode(QAbstractItemView::SingleSelection);
}

void WaferLevelTab::setConnect()
{
    // 晶圆调平点位：增删/上下移
    QObject::connect(m_ui->btn_add_waferLevelingPoints, &QPushButton::clicked, [this]() { onAddRow(); });
    QObject::connect(m_ui->btn_delete_waferLevelingPoints, &QPushButton::clicked, [this]() { onDeleteRow(); });
    QObject::connect(m_ui->btn_upMove_waferLevelingPoints, &QPushButton::clicked, [this]() { onMoveRowUp(); });
    QObject::connect(m_ui->btn_dwMove_waferLevelingPoints, &QPushButton::clicked, [this]() { onMoveRowDown(); });

    // 调平点位行 X/Y 填写完成后，自动显示该行的星形标记点；任一坐标被清空则移除该行标记
    QObject::connect(m_ui->tw_waferLevelingPoints, &QTableWidget::itemChanged, [this](QTableWidgetItem *item) {
        if (!item || item->row() < 0)
            return;
        const int row = item->row();
        QTableWidgetItem *ix = m_ui->tw_waferLevelingPoints->item(row, 0);
        QTableWidgetItem *iy = m_ui->tw_waferLevelingPoints->item(row, 1);
        bool okX = false, okY = false;
        const double x = ix ? ix->text().toDouble(&okX) : 0.0;
        const double y = iy ? iy->text().toDouble(&okY) : 0.0;
        if (okX && okY)
            m_ui->gv_preview_2->SetLevelingPoint(row, QPointF(x, y), QStringLiteral("Star"));
        else
            m_ui->gv_preview_2->RemoveLevelingPointExact(row);
    });

    // 选中表格某行 → 该行标记点描边
    QObject::connect(m_ui->tw_waferLevelingPoints, &QTableWidget::itemSelectionChanged, [this]() {
        m_ui->gv_preview_2->SetLevelingSelectedRow(m_ui->tw_waferLevelingPoints->currentRow(), 0);
    });

    // 调平标记被拖动 → 回写对应表格行的 X/Y（保留 3 位小数）
    QObject::connect(m_ui->gv_preview_2, &WaferGraphicsView::sig_levelingPointMoved,
                     [this](int row, int, const QPointF &nominalPos) {
        if (row < 0 || row >= m_ui->tw_waferLevelingPoints->rowCount())
            return;
        m_ui->tw_waferLevelingPoints->setItem(row, 0, new QTableWidgetItem(QString::number(nominalPos.x(), 'f', 3)));
        m_ui->tw_waferLevelingPoints->setItem(row, 1, new QTableWidgetItem(QString::number(nominalPos.y(), 'f', 3)));
        m_onModified();
    });
    // 标记点被点击 → 高亮该标记并选中对应表格行
    QObject::connect(m_ui->gv_preview_2, &WaferGraphicsView::sig_levelingPointClicked, [this](int row, int) {
        if (row >= 0 && row < m_ui->tw_waferLevelingPoints->rowCount())
            m_ui->tw_waferLevelingPoints->selectRow(row);
    });
}

void WaferLevelTab::setEditable(bool editable)
{
    m_ui->gv_preview_2->SetLevelingEditable(editable);
}

void WaferLevelTab::load(const S_RecipeWaferLevel &level)
{
    // 晶圆调平点位表
    m_ui->tw_waferLevelingPoints->setRowCount(static_cast<int>(level.lst_waferLevelingPoints.size()));
    for (int r = 0; r < static_cast<int>(level.lst_waferLevelingPoints.size()); ++r)
    {
        const S_NominalPoint &lp = level.lst_waferLevelingPoints[r];
        m_ui->tw_waferLevelingPoints->setItem(r, 0, new QTableWidgetItem(QString::number(lp.point.x_)));
        m_ui->tw_waferLevelingPoints->setItem(r, 1, new QTableWidgetItem(QString::number(lp.point.y_)));
    }
    // 在 gv_preview_2 上为每一行显示星形标记点（每行对应一个标记点）
    m_ui->gv_preview_2->ClearLevelingPoints();
    for (int r = 0; r < static_cast<int>(level.lst_waferLevelingPoints.size()); ++r)
    {
        const S_NominalPoint &lp = level.lst_waferLevelingPoints[r];
        m_ui->gv_preview_2->SetLevelingPoint(r, QPointF(lp.point.x_, lp.point.y_), QStringLiteral("Star"));
    }
}

void WaferLevelTab::save(S_RecipeWaferLevel &level) const
{
    level.lst_waferLevelingPoints.clear();
    for (int r = 0; r < m_ui->tw_waferLevelingPoints->rowCount(); ++r)
    {
        S_NominalPoint lp;
        QTableWidgetItem *ix = m_ui->tw_waferLevelingPoints->item(r, 0);
        QTableWidgetItem *iy = m_ui->tw_waferLevelingPoints->item(r, 1);
        lp.point = S_Point2D(ix ? ix->text().toDouble() : 0.0,
                             iy ? iy->text().toDouble() : 0.0);
        level.lst_waferLevelingPoints.push_back(lp);
    }
}

void WaferLevelTab::onAddRow()
{
    const int row = m_ui->tw_waferLevelingPoints->rowCount();
    m_ui->tw_waferLevelingPoints->insertRow(row);
    m_ui->tw_waferLevelingPoints->setCurrentCell(row, 0);
    m_onModified();
}

void WaferLevelTab::onDeleteRow()
{
    const int row = m_ui->tw_waferLevelingPoints->currentRow();
    if (row < 0)
        return;
    m_ui->tw_waferLevelingPoints->removeRow(row);
    // 删除该行时同步删除其对应标记点，并使其后所有标记行号 -1
    m_ui->gv_preview_2->RemoveLevelingPoint(row, 0);
    m_onModified();
}

void WaferLevelTab::onMoveRowUp()
{
    int row = m_ui->tw_waferLevelingPoints->currentRow();
    if (row <= 0)
        return;

    QList<QTableWidgetItem *> items;
    QList<QWidget *> widgets;
    for (int c = 0; c < m_ui->tw_waferLevelingPoints->columnCount(); ++c)
    {
        items.append(m_ui->tw_waferLevelingPoints->takeItem(row, c));
        widgets.append(m_ui->tw_waferLevelingPoints->cellWidget(row, c));
        m_ui->tw_waferLevelingPoints->removeCellWidget(row, c);
    }
    {   // 重排期间屏蔽表格信号，避免半行中间态触发 itemChanged 误删其它行标记
        QSignalBlocker blocker(m_ui->tw_waferLevelingPoints);
        m_ui->tw_waferLevelingPoints->removeRow(row);
        m_ui->tw_waferLevelingPoints->insertRow(row - 1);
        for (int c = 0; c < m_ui->tw_waferLevelingPoints->columnCount(); ++c)
        {
            m_ui->tw_waferLevelingPoints->setItem(row - 1, c, items.at(c));
            if (widgets.at(c))
                m_ui->tw_waferLevelingPoints->setCellWidget(row - 1, c, widgets.at(c));
        }
    }
    // 行号互换，同步交换两行标记点
    m_ui->gv_preview_2->MoveLevelingPoint(row, row - 1, 0);

    m_ui->tw_waferLevelingPoints->setCurrentCell(row - 1, 0);
    m_onModified();
}

void WaferLevelTab::onMoveRowDown()
{
    int row = m_ui->tw_waferLevelingPoints->currentRow();
    if (row < 0 || row >= m_ui->tw_waferLevelingPoints->rowCount() - 1)
        return;

    QList<QTableWidgetItem *> items;
    QList<QWidget *> widgets;
    for (int c = 0; c < m_ui->tw_waferLevelingPoints->columnCount(); ++c)
    {
        items.append(m_ui->tw_waferLevelingPoints->takeItem(row, c));
        widgets.append(m_ui->tw_waferLevelingPoints->cellWidget(row, c));
        m_ui->tw_waferLevelingPoints->removeCellWidget(row, c);
    }
    {   // 重排期间屏蔽表格信号，避免半行中间态触发 itemChanged 误删其它行标记
        QSignalBlocker blocker(m_ui->tw_waferLevelingPoints);
        m_ui->tw_waferLevelingPoints->removeRow(row);
        m_ui->tw_waferLevelingPoints->insertRow(row + 1);
        for (int c = 0; c < m_ui->tw_waferLevelingPoints->columnCount(); ++c)
        {
            m_ui->tw_waferLevelingPoints->setItem(row + 1, c, items.at(c));
            if (widgets.at(c))
                m_ui->tw_waferLevelingPoints->setCellWidget(row + 1, c, widgets.at(c));
        }
    }
    // 行号互换，同步交换两行标记点
    m_ui->gv_preview_2->MoveLevelingPoint(row, row + 1, 0);

    m_ui->tw_waferLevelingPoints->setCurrentCell(row + 1, 0);
    m_onModified();
}
