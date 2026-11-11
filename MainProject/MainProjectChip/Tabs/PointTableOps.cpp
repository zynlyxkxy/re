#include "PointTableOps.h"

#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QWidget>

namespace PointTableOps {

void addRow(QTableWidget *tw)
{
    const int row = tw->rowCount();
    tw->insertRow(row);
    tw->setCurrentCell(row, 0);
}

void deleteRow(QTableWidget *tw)
{
    const int row = tw->currentRow();
    if (row < 0)
        return;
    tw->removeRow(row);
}

void moveRowUp(QTableWidget *tw)
{
    const int row = tw->currentRow();
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
    tw->removeRow(row);

    tw->insertRow(row - 1);
    for (int c = 0; c < tw->columnCount(); ++c)
    {
        tw->setItem(row - 1, c, items.at(c));
        if (widgets.at(c))
            tw->setCellWidget(row - 1, c, widgets.at(c));
    }
    tw->setCurrentCell(row - 1, 0);
}

void moveRowDown(QTableWidget *tw)
{
    const int row = tw->currentRow();
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
    tw->removeRow(row);

    tw->insertRow(row + 1);
    for (int c = 0; c < tw->columnCount(); ++c)
    {
        tw->setItem(row + 1, c, items.at(c));
        if (widgets.at(c))
            tw->setCellWidget(row + 1, c, widgets.at(c));
    }
    tw->setCurrentCell(row + 1, 0);
}

void bindRowButtons(QTableWidget *tw, QPushButton *add, QPushButton *del,
                    QPushButton *up, QPushButton *dw,
                    const std::function<void()> &onModified)
{
    auto notify = [onModified]() {
        if (onModified)
            onModified();
    };
    QObject::connect(add, &QPushButton::clicked, [tw, notify]() { addRow(tw); notify(); });
    QObject::connect(del, &QPushButton::clicked, [tw, notify]() { deleteRow(tw); notify(); });
    QObject::connect(up, &QPushButton::clicked, [tw, notify]() { moveRowUp(tw); notify(); });
    QObject::connect(dw, &QPushButton::clicked, [tw, notify]() { moveRowDown(tw); notify(); });
}

} // namespace PointTableOps
