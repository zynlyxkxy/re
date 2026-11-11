#include "LevelingMarkItem.h"

#include <QBrush>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QPen>
#include <QPolygonF>

namespace {
// ── 可见尺寸：只由芯片短边决定（与芯片等比，不设任何像素上下限）──
// 半径 = 芯片短边 × 该类标记自己的比例；想调某一类标记的大小，只改它对应的那个数
constexpr qreal kSymDieFrac = 0.4;        // 十字 / 晶圆调平菱形 / 方：直径 = 芯片短边 80%（当前取值）
constexpr qreal kCalibDieFrac = 0.08;   // Chip 标定圆点与候选点：直径 = 芯片短边 1/8（gui 原始比例，圆点密集故取小）
constexpr qreal kDefaultRadius = 1.5;     // 芯片尺寸未知（未下发芯片尺寸）时的缺省半径（场景单位）
constexpr qreal kClickTolFrac = 1.7;      // 点击命中容差 = 绘制半径 × 1.7（gui 规则）
// ── 命中区（不可见）：本项目保留的最小抓取区，保证小芯片上仍点得中/拖得动 ──
constexpr qreal kHitRatio = 0.6;          // 拖动标记的命中区半宽 = 芯片短边 × 60%
constexpr qreal kHitDefault = 7.0;        // 芯片尺寸未知时的命中区半宽
constexpr int kDimAlpha = 70;             // 虚色（候选点未选中）透明度
}

LevelingMarkItem::LevelingMarkItem(int row, const QPointF &sceneCenter, const QString &markType, int table,
                                   double chipWidth, double chipHeight, bool locked, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_row(row)
    , m_markType(markType)
    , m_table(table)
    , m_chipShortSide((chipWidth > 0.0 && chipHeight > 0.0) ? qMin(chipWidth, chipHeight) : 0.0)
    , m_locked(locked)
{
    setPos(sceneCenter);  //决定这个“原点”落在场景的哪个点
    // 命中区（不可见）：不小于可见符号，并保留本项目的最小抓取区
    m_hitHalf = qMax(symbolRadius(), m_chipShortSide > 0.0 ? m_chipShortSide * kHitRatio : kHitDefault);
    updateInteractionFlags();  //是否未可编辑状态
}

qreal LevelingMarkItem::symbolRadius() const
{
    // 与芯片等比：场景半径 = 芯片短边 × 该类型的比例（Chip 标定圆点/候选点用更小的 kCalibDieFrac）。
    // 芯片本身随缩放变化，所以屏幕上符号与芯片始终同比例；不设任何像素上下限。
    const qreal frac = (m_markType == QStringLiteral("Calibration")) ? kCalibDieFrac : kSymDieFrac;
    if (m_chipShortSide > 0.0)
        return m_chipShortSide * frac;

    // 芯片尺寸未知：只给一个很小的固定缺省半径（不设上下限）
    return kDefaultRadius;
}

QRectF LevelingMarkItem::boundingRect() const  // 图元的包围盒：取命中区（可见符号比它小）
{
    return QRectF(-m_hitHalf, -m_hitHalf, m_hitHalf * 2, m_hitHalf * 2);
}

QPainterPath LevelingMarkItem::shape() const   //修改命中区
{
    QPainterPath path;
    if (m_clickOnly)
    {
        // 候选点：命中容差 = 绘制半径 × 1.7（gui 规则，无像素上下限）；
        // 上限压到命中区半宽内，保证 shape 始终落在 boundingRect 里（场景索引按 boundingRect 粗筛）
        const qreal tol = qMin(symbolRadius() * kClickTolFrac, m_hitHalf);
        path.addEllipse(QPointF(0, 0), tol, tol);
        return path;
    }
    // 普通标记：命中区用一个不可见的圆形下限（比可见符号大），小芯片上仍点得中、拖得动
    path.addEllipse(QPointF(0, 0), m_hitHalf, m_hitHalf);
    return path;
}

void LevelingMarkItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) //绘制
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing);
    drawMark(painter);  //调用设置标记形状
}

void LevelingMarkItem::drawMark(QPainter *painter) const
{
    // 形状几何与描边约定照抄 gui 的 DiagramBase::drawSymbol：
    //   十字 → PlusMark（两条直线，不填充）、星 → XMark（两条对角斜线）、
    //   方 → Square（边长 1.8r 的实心方）、Chip 标定/候选点 → Dot（实心圆点）。
    // 半径见 symbolRadius()：芯片短边 × 0.0625（直径 = 芯片短边 1/8），与 gui 完全一致。
    // 描边用 cosmetic（屏幕像素恒定）、宽度随符号大小收细 qBound(0.35, rPx*0.5, 1.5)，也是 gui 规则。
    const qreal r = symbolRadius();
    const qreal scale = painter->worldTransform().m11();
    const qreal rPx = r * (scale > 0.0 ? scale : 1.0);
    const qreal penPx = qBound(0.35, rPx * 0.5, 1.5);

    // 颜色：候选点未选中时半透明（虚色），否则原色（实色）；表格行选中改用深灰描边（本项目交互反馈）
    const auto penOf = [&](const QColor &color) {
        QColor c = color;
        if (m_dimmed)
            c.setAlpha(kDimAlpha);
        QPen pen(m_markSelected ? QColor(40, 40, 40) : c, m_markSelected ? qMax(penPx, 1.5) : penPx);
        pen.setCosmetic(true);
        return pen;
    };
    const auto brushOf = [&](const QColor &color) {
        QColor c = color;
        if (m_dimmed)
            c.setAlpha(kDimAlpha);
        return QBrush(c);
    };

    if (m_markType == QStringLiteral("Cross"))
    {
        // 加号十字（gui PlusMark）：两线过中心，不填充
        const QColor c(0, 153, 255);
        painter->setPen(penOf(c));
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(-r, 0.0), QPointF(r, 0.0));
        painter->drawLine(QPointF(0.0, -r), QPointF(0.0, r));
    }
    else if (m_markType == QStringLiteral("Star"))
    {
        // 晶圆调平标记（WaferLevelTab 传 "Star"）：橙色实心菱形，对角线 2r（顶点在 ±r 的上下左右）
        const QColor c(230, 80, 0);
        QPolygonF diamond;
        diamond << QPointF(0.0, -r) << QPointF(r, 0.0) << QPointF(0.0, r) << QPointF(-r, 0.0);
        painter->setPen(penOf(c));
        painter->setBrush(brushOf(c));
        painter->drawPolygon(diamond);
    }
    else if (m_markType == QStringLiteral("Square"))
    {
        // 实心方（gui Square：边长 1.8r）
        const QColor c(76, 175, 80);
        painter->setPen(penOf(c));
        painter->setBrush(brushOf(c));
        painter->drawRect(QRectF(-0.9 * r, -0.9 * r, 1.8 * r, 1.8 * r));
    }
    else
    {
        // Chip 标定点 / 候选点 / 未知类型：实心圆点（gui Dot）
        const QColor c = (m_markType == QStringLiteral("Calibration")) ? QColor(0, 0, 139) : QColor(50, 120, 220);
        painter->setPen(penOf(c));
        painter->setBrush(brushOf(c));
        painter->drawEllipse(QPointF(0.0, 0.0), r, r);
    }
}

void LevelingMarkItem::setMarkSelected(bool selected) // 表格行选中时描边高亮
{
    if (m_markSelected == selected) // 一样时跳过
        return;
    m_markSelected = selected;
    update(); // 重绘
}

void LevelingMarkItem::setLocked(bool locked)
{
    if (m_locked == locked)
        return;
    m_locked = locked;
    updateInteractionFlags();  //更新是否可以交互的状态
    update(); // 重绘
}

void LevelingMarkItem::setDimmed(bool dimmed) // 虚色/实色切换（仅重绘，不改交互）
{
    if (m_dimmed == dimmed)
        return;
    m_dimmed = dimmed;
    update(); // 重绘
}

void LevelingMarkItem::setClickOnly(bool clickOnly) // 候选点模式：只点不拖
{
    if (m_clickOnly == clickOnly)
        return;
    m_clickOnly = clickOnly;
    updateInteractionFlags();
    update(); // 重绘
}

void LevelingMarkItem::updateInteractionFlags()
{
    if (m_clickOnly)
    {
        // 候选点：只接收鼠标事件（不可拖动/选中），悬停显示手型
        setFlags(QGraphicsItem::GraphicsItemFlags());
        setAcceptHoverEvents(true);
        setCursor(Qt::PointingHandCursor);
        return;
    }

    unsetCursor();
    if (m_locked)
    {
        // 锁定点：彻底不可交互（无拖动/选中/悬停）
        setFlags(QGraphicsItem::GraphicsItemFlags());
        setAcceptHoverEvents(false);
    }
    else
    {
        setFlags(ItemIsMovable | ItemSendsGeometryChanges | ItemIsSelectable);
        setAcceptHoverEvents(true);
    }
}

void LevelingMarkItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_clickOnly)
    {
        // 候选点：单击即通知外部“选中该候选点”，不做拖动/选中处理
        emit sig_candidateClicked(m_row, m_table);
        event->accept();
        return;
    }

    if (m_locked)
    {
        QGraphicsObject::mousePressEvent(event);   // 锁定点不响应
        return;
    }

    m_mousePressed = true;
    emit sig_clicked(m_row, m_table);
    QGraphicsObject::mousePressEvent(event);  // 基类：走选中/拖拽的标准流程
}

void LevelingMarkItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsObject::mouseReleaseEvent(event);
    if (m_locked || m_clickOnly || !m_mousePressed)
        return;
    emit sig_moved(m_row, m_table, pos());
    m_mousePressed = false;
}

QVariant LevelingMarkItem::itemChange(GraphicsItemChange change, const QVariant &value)  //拖动时的实时坐标回传
{
    if (change == ItemPositionHasChanged && !m_locked && m_mousePressed) {
        emit sig_moved(m_row, m_table, value.toPointF());
    }
    return QGraphicsObject::itemChange(change, value);
}
