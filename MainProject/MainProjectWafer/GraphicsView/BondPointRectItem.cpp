#include "BondPointRectItem.h"

#include <QBrush>
#include <QPen>
#include <QGraphicsSceneMouseEvent>

BondPointRectItem::BondPointRectItem(qreal x, qreal y, qreal w, qreal h, QGraphicsItem *parent)
    : QGraphicsRectItem(x, y, w, h, parent)
{
    // 默认黄色(未选中)，选中后变蓝色
    setBrush(QBrush(Qt::yellow));
    setPen(QPen(Qt::black));
    setAcceptedMouseButtons(Qt::LeftButton);
}

void BondPointRectItem::SetNominalPos(qreal x, qreal y)
{
    m_nominalPos = QPointF(x, y);
}

QPointF BondPointRectItem::GetNominalPos() const
{
    return m_nominalPos;
}

void BondPointRectItem::SetSelected(bool selected)
{
    m_selected = selected;
    setBrush(selected ? QBrush(Qt::blue) : QBrush(Qt::yellow));
}

bool BondPointRectItem::IsSelected() const
{
    return m_selected;
}

void BondPointRectItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        // 左键点击切换选中状态
        SetSelected(!m_selected);
        emit sig_selectedStateChanged(m_selected);
        event->accept();
    }
    else
    {
        QGraphicsRectItem::mousePressEvent(event);
    }
}
