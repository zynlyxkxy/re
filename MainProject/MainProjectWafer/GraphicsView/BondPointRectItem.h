#ifndef BONDPOINTRECTITEM_H
#define BONDPOINTRECTITEM_H

#include <QObject>
#include <QGraphicsRectItem>
#include <QPointF>

// 键合选点用的可点击矩形项：
// 每个矩形代表一个芯片位，点击可在“选中(蓝色)/未选中(黄色)”之间切换，
// 并发出 sig_selectedStateChanged 信号供视图统计选中数量。
class BondPointRectItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
public:
    explicit BondPointRectItem(qreal x, qreal y, qreal w, qreal h, QGraphicsItem *parent = nullptr);

    // 设置名义坐标（芯片中心在设计坐标系下的坐标，用于回写选点数据）
    void SetNominalPos(qreal x, qreal y);
    // 获取名义坐标
    QPointF GetNominalPos() const;

    // 设置选中状态（仅更新外观与状态，不触发点击信号）
    void SetSelected(bool selected);
    // 当前是否选中
    bool IsSelected() const;

signals:
    // 点击产生的选中状态变化信号
    void sig_selectedStateChanged(bool selected);

protected:
    // 鼠标点击：切换选中状态并发出信号
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QPointF m_nominalPos;        // 名义坐标
    bool    m_selected = false;  // 选中标志
};

#endif // BONDPOINTRECTITEM_H
