#include "DiagramBase.h"

#include "istar_language.h"   // Tr()：图例词条随当前语言切换

// 差异说明（本文件移植自 gui 工程 ProcessModule/Components/DiagramComponent/Base/DiagramBase）：
// gui 侧这里还 include 了工程专有的 "Common/UiTranslator.h"（靠动态属性 trLegend 触发图例重绘）。
// re 侧没有 UiTranslator，故不引入该依赖；图例在 re 侧默认关闭（m_legendVisible=false），
// 调用 setLegendVisible(true) 后若需跟随语言切换，由调用方自行刷新视图。

#include <QFontMetrics>
#include <QGraphicsEllipseItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>

#include <cmath>
#include <limits>

namespace {

// 符号（点）尺寸规则：
//   有 die（芯片）时——与芯片等比：屏幕半径 = die 屏幕短边 × kSymDieFrac（直径 = 芯片短边 1/8）
//   无 die 时——名义屏幕像素半径 radiusPx，夹到 [kSymMinRadiusPx, kSymMaxRadiusPx]
constexpr double kSymDieFrac = 0.0625;    // 屏幕半径 / die 屏幕短边（直径 = 芯片短边 1/8）
constexpr double kSymMinRadiusPx = 2.5;   // 仅无 die 时生效：名义值下限
constexpr double kSymMaxRadiusPx = 12.0;  // 仅无 die 时生效：名义值上限

// 点击命中符号的容差（像素）：容差 = 该点绘制屏幕半径 × kClickTolFrac，但不小于 kClickTolMinPx。
// 画出来的那个圆圈一定落在可点范围内（还多 20% 余量）；点很小时也保证有 kClickTolMinPx 好点。
constexpr double kClickTolFrac = 1.7;
constexpr double kClickTolMinPx = 6.0;
// 拖动判定阈值（像素）
constexpr int kDragThresholdPx = 5;
// 轴带宽度（像素）
constexpr int kAxisBandLeft = 52;
constexpr int kAxisBandBottom = 34;

// 标注候选偏移（屏幕像素），顺序：NE E SE S SW W NW N
const QPointF kLabelDirs[8] = {
    QPointF(7, -6), QPointF(14, 0), QPointF(7, 8), QPointF(0, 14),
    QPointF(-16, 8), QPointF(-24, 0), QPointF(-16, -14), QPointF(0, -16),
};

// 标注字号随缩放（applyMarkFont）：缩小不更小（下限 = 初始字号）；
// 放大按对数阻尼——每放大一倍字号约 +kMarkFontLogGrow 倍基准，放大越深涨得越缓
constexpr double kMarkFontLogGrow = 0.3;

QString coordLine(double x, double y)
{
    return QString("(%1, %2)").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3);
}

// 经典 1/2/5 取步距
double niceStep(double raw)
{
    if (raw <= 0.0)
    {
        return 1.0;
    }
    const double p = std::pow(10.0, std::floor(std::log10(raw)));
    const double m = raw / p;
    double f = 1.0;
    if (m > 5.0)
    {
        f = 10.0;
    }
    else if (m > 2.0)
    {
        f = 5.0;
    }
    else if (m > 1.0)
    {
        f = 2.0;
    }
    return f * p;
}

int decimalsFor(double step)
{
    const double d = std::ceil(-std::log10(step));
    if (d <= 0.0)
    {
        return 0;
    }
    return static_cast<int>(std::min(d, 6.0));
}

// 四舍五入取整（qRound 在 qreal=double 构建下 int 实参有重载歧义，绕开）
int toPx(double v)
{
    return static_cast<int>(v + (v >= 0.0 ? 0.5 : -0.5));
}

// 画一个符号：radius 为当前绘制坐标系下的半径，penWidthPx 为描边宽（屏幕像素，cosmetic）
void drawSymbol(QPainter *p, const QPointF &c, double radius, const QColor &color,
                DiagramBase::SymbolShape shape, double penWidthPx = 1.5)
{
    using SS = DiagramBase::SymbolShape;
    QPen pen(color, penWidthPx);
    pen.setCosmetic(true);
    p->setPen(pen);
    const double r = radius;
    switch (shape)
    {
    case SS::Dot:
    case SS::HollowDot:
    {
        p->setBrush(shape == SS::Dot ? color : Qt::NoBrush);
        p->drawEllipse(c, r, r);
        break;
    }
    case SS::XMark:
    {
        p->setBrush(Qt::NoBrush);
        p->drawLine(QPointF(c.x() - r, c.y() - r), QPointF(c.x() + r, c.y() + r));
        p->drawLine(QPointF(c.x() - r, c.y() + r), QPointF(c.x() + r, c.y() - r));
        break;
    }
    case SS::PlusMark:
    {
        p->setBrush(Qt::NoBrush);
        p->drawLine(QPointF(c.x() - r, c.y()), QPointF(c.x() + r, c.y()));
        p->drawLine(QPointF(c.x(), c.y() - r), QPointF(c.x(), c.y() + r));
        break;
    }
    case SS::Triangle:
    case SS::HollowTriangle:
    {
        const double k = 0.866 * r;   // 等边三角形
        QPolygonF tri;
        tri << QPointF(c.x(), c.y() - r) << QPointF(c.x() + k, c.y() + 0.5 * r)
            << QPointF(c.x() - k, c.y() + 0.5 * r);
        p->setBrush(shape == SS::Triangle ? color : Qt::NoBrush);
        p->drawPolygon(tri);
        break;
    }
    case SS::Square:
    case SS::HollowSquare:
    {
        p->setBrush(shape == SS::Square ? color : Qt::NoBrush);
        p->drawRect(QRectF(c.x() - 0.9 * r, c.y() - 0.9 * r, 1.8 * r, 1.8 * r));
        break;
    }
    }
}

} // namespace

// ---------------- 内部图层 ----------------

class DiagramBase::DieLayer : public QGraphicsItem
{
public:
    explicit DieLayer(DiagramBase *host)
        : m_host(host)
    {
        setZValue(0);
    }

    QRectF boundingRect() const override { return m_bounds; }

    void setBounds(const QRectF &b)
    {
        prepareGeometryChange();
        m_bounds = b;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) override
    {
        const QRectF exposed = option->exposedRect.adjusted(-1.0, -1.0, 1.0, 1.0);
        const double hw = m_host->m_dieW / 2.0;
        const double hh = m_host->m_dieH / 2.0;
        QPen diePen(QColor(0, 0, 0), 1.0);   // 黑色轮廓线（借鉴旧版芯片风格），像素恒定
        diePen.setCosmetic(true);
        painter->setPen(diePen);
        for (const DieRec &d : m_host->m_dies)
        {
            const QRectF r(d.real.x() - hw, d.real.y() - hh, m_host->m_dieW, m_host->m_dieH);
            if (!exposed.intersects(r))
            {
                continue;
            }
            painter->setBrush(d.color);
            painter->drawRect(r);
        }
    }

private:
    DiagramBase *m_host;
    QRectF m_bounds;
};

class DiagramBase::SymbolLayer : public QGraphicsItem
{
public:
    explicit SymbolLayer(DiagramBase *host)
        : m_host(host)
    {
        setZValue(10);
    }

    // 符号像素大小恒定，场景半径随缩放反比变化，包围盒给足余量，paint 内部自行裁剪
    QRectF boundingRect() const override { return QRectF(-1.0e6, -1.0e6, 2.0e6, 2.0e6); }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) override
    {
        if (m_host->m_syms.empty())
        {
            return;
        }
        const double scale = painter->worldTransform().m11();
        if (scale <= 0.0)
        {
            return;
        }
        const QRectF exposed = option->exposedRect;
        for (const SymRec &s : m_host->m_syms)
        {
            // 尺寸规则统一走 symbolScreenRadius（有 die 时与芯片等比，无 die 时用名义像素值）
            const double rPx = m_host->symbolScreenRadius(s.radiusPx);
            const double r = rPx / scale;
            const double cull = r + 2.0;
            if (!exposed.adjusted(-cull, -cull, cull, cull).contains(s.real))
            {
                continue;
            }
            // 描边随尺寸收细：点很小时描边要基本消失，别把点画成一坨
            const double penPx = qBound(0.35, rPx * 0.5, 1.5);
            drawSymbol(painter, s.real, r, s.color, s.shape, penPx);
        }
    }

private:
    DiagramBase *m_host;
};

// 视口 HUD：左、下轴带 + 虚线网格。在视图 paintEvent 里按视口矩形直接画（不是子控件，
// 位置固定在视口边缘，不随平移移动；刻度值随视口范围变化）。
class DiagramBase::AxisOverlay
{
public:
    explicit AxisOverlay(DiagramBase *host)
        : m_host(host)
    {
    }

    void paint(QPainter &p, const QRect &vr)
    {
        DiagramBase *w = m_host;
        const int bw = w->m_axesOn ? kAxisBandLeft : 0;
        const int bh = w->m_axesOn ? kAxisBandBottom : 0;
        const QRect plot = vr.adjusted(bw, 0, 0, -bh);
        if (plot.width() < 4 || plot.height() < 4)
        {
            return;
        }

        // 可见场景范围按 min/max 归一化：不假设 y 方向（场景 y 与视口 y 反向时同样成立）
        const QPointF tl = w->mapToScene(plot.topLeft());
        const QPointF br = w->mapToScene(plot.bottomRight());
        const double x0 = qMin(tl.x(), br.x());
        const double x1 = qMax(tl.x(), br.x());
        const double y0 = qMin(tl.y(), br.y());
        const double y1 = qMax(tl.y(), br.y());
        if (x1 <= x0 || y1 <= y0)
        {
            return;
        }

        const QPalette pal = w->palette();

        // 虚线网格（步距 = 轴刻度步距）
        if (w->m_gridOn)
        {
            const double sx = niceStep((x1 - x0) / 8.0);
            const double sy = niceStep((y1 - y0) / 8.0);
            p.setClipRect(plot);
            p.setPen(QPen(w->m_gridColor, 1.0, Qt::DashLine));
            for (long long i = static_cast<long long>(std::ceil(x0 / sx)); i * sx <= x1; ++i)
            {
                const int px = toPx(w->mapFromScene(QPointF(i * sx, 0.0)).x());
                p.drawLine(px, plot.top(), px, plot.bottom());
            }
            for (long long j = static_cast<long long>(std::ceil(y0 / sy)); j * sy <= y1; ++j)
            {
                const int py = toPx(w->mapFromScene(QPointF(0.0, j * sy)).y());
                p.drawLine(plot.left(), py, plot.right(), py);
            }
            p.setClipping(false);
        }

        if (!w->m_axesOn)
        {
            return;
        }

        // 轴带背景 + 轴线
        const QColor bandBg = pal.color(QPalette::Window);
        const QColor mid = pal.color(QPalette::Mid);
        p.fillRect(QRect(0, 0, bw, vr.height()), bandBg);
        p.fillRect(QRect(bw, vr.height() - bh, vr.width() - bw, bh), bandBg);
        p.setPen(QPen(mid, 1.0));
        p.drawLine(bw - 1, 0, bw - 1, vr.height() - bh);
        p.drawLine(bw - 1, vr.height() - bh, vr.width() - 1, vr.height() - bh);

        const QColor tc = pal.color(QPalette::WindowText);
        const int hbTop = vr.height() - bh;

        // X 刻度（下边带）
        const double sx = niceStep((x1 - x0) / 8.0);
        const int decX = decimalsFor(sx);
        for (long long i = static_cast<long long>(std::ceil(x0 / sx)); i * sx <= x1; ++i)
        {
            const double v = i * sx;
            const int px = toPx(w->mapFromScene(QPointF(v, 0.0)).x());
            if (px < plot.left() + 2 || px > plot.right() - 2)
            {
                continue;
            }
            p.setPen(QPen(mid, 1.0));
            p.drawLine(px, hbTop, px, hbTop + 4);
            p.setPen(tc);
            p.drawText(QRect(px - 50, hbTop + 6, 100, bh - 8), Qt::AlignHCenter | Qt::AlignVCenter,
                       QString::number(v, 'f', decX));
        }

        // Y 刻度（左边带）
        const double sy = niceStep((y1 - y0) / 8.0);
        const int decY = decimalsFor(sy);
        for (long long j = static_cast<long long>(std::ceil(y0 / sy)); j * sy <= y1; ++j)
        {
            const double v = j * sy;
            const int py = toPx(w->mapFromScene(QPointF(0.0, v)).y());
            if (py < plot.top() + 2 || py > plot.bottom() - 2)
            {
                continue;
            }
            p.setPen(QPen(mid, 1.0));
            p.drawLine(bw - 5, py, bw - 1, py);
            p.setPen(tc);
            p.drawText(QRect(2, py - 9, bw - 9, 18), Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(v, 'f', decY));
        }
    }

private:
    DiagramBase *m_host;
};

// 视口覆盖层：图例
class DiagramBase::LegendBox : public QWidget
{
public:
    explicit LegendBox(DiagramBase *host)
        : m_host(host)
    {
    }

    // 在视图 paintEvent 里按视口矩形绘制：位置固定在视口角落，不随平移移动
    void paint(QPainter &p, const QRect &vr)
    {
        DiagramBase *w = m_host;
        if (!w->m_legendVisible || w->m_legendItems.empty())
        {
            return;
        }

        const QFontMetrics fm(p.font());
        const int rowH = qMax(20, fm.height() + 6);
        int textW = 0;
        for (const LegendItem &it : w->m_legendItems)
        {
            textW = qMax(textW, fm.boundingRect(legendText(it)).width());
        }

        const int boxW = 2 * kMargin + kSwatch + textW;
        const int boxH = 2 * kMargin + rowH * static_cast<int>(w->m_legendItems.size());
        const bool left = (w->m_legendPos == LegendPos::TopLeft ||
                           w->m_legendPos == LegendPos::BottomLeft);
        const bool top = (w->m_legendPos == LegendPos::TopLeft ||
                          w->m_legendPos == LegendPos::TopRight);
        const QRect box(left ? vr.left() + 8 : vr.right() - boxW - 8,
                        top ? vr.top() + 8 : vr.bottom() - boxH - 8, boxW, boxH);

        p.save();
        p.setRenderHint(QPainter::Antialiasing);
        QColor bg = w->palette().color(QPalette::Window);
        bg.setAlpha(225);
        p.setBrush(bg);
        p.setPen(w->palette().color(QPalette::Mid));
        p.drawRoundedRect(box.adjusted(0, 0, -1, -1), 4.0, 4.0);

        const double cx = box.left() + kMargin + kSwatch / 2.0;
        int y = box.top() + kMargin;
        for (const LegendItem &it : w->m_legendItems)
        {
            drawSymbol(&p, QPointF(cx, y + rowH / 2.0), 5.0, it.color, it.shape);
            p.setPen(w->palette().color(QPalette::WindowText));
            p.drawText(QRect(box.left() + kMargin + kSwatch, y, textW, rowH),
                       Qt::AlignVCenter | Qt::AlignLeft, legendText(it));
            y += rowH;
        }
        p.restore();
    }

private:
    static constexpr int kMargin = 6;
    static constexpr int kSwatch = 20;   // 符号位宽

    DiagramBase *m_host;
};

// ---------------- 构造 ----------------

DiagramBase::DiagramBase(QWidget *parent)
    : QGraphicsView(parent)
{
    setScene(new QGraphicsScene(this));
    setRenderHint(QPainter::Antialiasing);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::NoAnchor);
    setCursor(Qt::OpenHandCursor);   // 可拖动提示：按下变握拳

    m_dieLayer = new DieLayer(this);
    m_symLayer = new SymbolLayer(this);
    scene()->addItem(m_dieLayer);
    scene()->addItem(m_symLayer);

    m_axisOverlay = new AxisOverlay(this);   // HUD 不是子控件，在 paintEvent 里绘制
    m_legendBox = new LegendBox(this);

    // 场景矩形给到极大：平移不受内容范围限制，拖出视野后缩小总能找回
    setSceneRect(-1.0e6, -1.0e6, 2.0e6, 2.0e6);

    viewport()->setMouseTracking(true);
}

DiagramBase::~DiagramBase()
{
    delete m_axisOverlay;
    delete m_legendBox;
}

// ---------------- 底图 ----------------

void DiagramBase::drawCircle(double cx, double cy, double diameter, const QColor &line,
                             const QColor &fill)
{
    if (diameter <= 0.0)
    {
        return;
    }
    const double r = diameter / 2.0;
    auto *ellipse = new QGraphicsEllipseItem(cx - r, cy - r, diameter, diameter);
    ellipse->setPen(QPen(line, 0));    // 0 = 发丝线（像素恒定）
    ellipse->setBrush(fill);
    addBackdrop(ellipse);
}

void DiagramBase::drawSquare(double cx, double cy, double width, double height, const QColor &line,
                             const QColor &fill)
{
    if (width <= 0.0 || height <= 0.0)
    {
        return;
    }
    auto *rect = new QGraphicsRectItem(cx - width / 2.0, cy - height / 2.0, width, height);
    rect->setPen(QPen(line, 0));
    rect->setBrush(fill);
    addBackdrop(rect);
}

void DiagramBase::drawTriangle(const QPointF &p1, const QPointF &p2, const QPointF &p3,
                               const QColor &line, const QColor &fill)
{
    QPolygonF tri;
    tri << p1 << p2 << p3;
    auto *poly = new QGraphicsPolygonItem(tri);
    poly->setPen(QPen(line, 0));
    poly->setBrush(fill);
    addBackdrop(poly);
}

void DiagramBase::addBackdrop(QGraphicsItem *shape)
{
    shape->setZValue(-10);
    scene()->addItem(shape);
    m_backdrops.push_back(shape);

    m_needFit = true;
    if (isVisible())
    {
        fitWafer();
    }
}

void DiagramBase::clearBackdrop()
{
    for (QGraphicsItem *it : m_backdrops)
    {
        scene()->removeItem(it);
        delete it;
    }
    m_backdrops.clear();

    m_needFit = true;
    if (isVisible())
    {
        fitWafer();
    }
}

// ---------------- 晶圆中心 ----------------

void DiagramBase::setCenter(double cx, double cy)
{
    m_center = QPointF(cx, cy);
}

QPointF DiagramBase::center() const
{
    return m_center;
}

// ---------------- die 层 ----------------

void DiagramBase::setDies(const std::vector<QPointF> &realCenters, double dieW, double dieH,
                          const QColor &color)
{
    m_dieW = qMax(dieW, 1.0e-9);
    m_dieH = qMax(dieH, 1.0e-9);
    m_dies.clear();
    m_dies.reserve(realCenters.size());
    for (const QPointF &c : realCenters)
    {
        m_dies.push_back(DieRec{ c, color });
    }
    finishSetDies();
}

void DiagramBase::finishSetDies()
{
    // die 已整体替换，旧 die 标注目标失效
    clearDieMarks();

    // 空间索引：die 矩形覆盖到的所有格子都登记
    m_dieHash.clear();
    const double hw = m_dieW / 2.0;
    const double hh = m_dieH / 2.0;
    for (int i = 0; i < static_cast<int>(m_dies.size()); ++i)
    {
        const QPointF &c = m_dies[static_cast<size_t>(i)].real;
        const long long x0 = static_cast<long long>(std::floor((c.x() - hw) / m_dieW));
        const long long x1 = static_cast<long long>(std::floor((c.x() + hw) / m_dieW));
        const long long y0 = static_cast<long long>(std::floor((c.y() - hh) / m_dieH));
        const long long y1 = static_cast<long long>(std::floor((c.y() + hh) / m_dieH));
        for (long long gx = x0; gx <= x1; ++gx)
        {
            for (long long gy = y0; gy <= y1; ++gy)
            {
                m_dieHash[{ gx, gy }].push_back(i);
            }
        }
    }

    // die 图层包围盒
    QRectF bounds;
    for (const DieRec &d : m_dies)
    {
        bounds = bounds.united(QRectF(d.real.x() - hw, d.real.y() - hh, m_dieW, m_dieH));
    }
    if (bounds.isEmpty())
    {
        bounds = QRectF(-m_dieW, -m_dieH, 2.0 * m_dieW, 2.0 * m_dieH);
    }
    m_dieLayer->setBounds(bounds);

    layoutMarks();
    m_needFit = true;
    if (isVisible())
    {
        fitWafer();
    }
    m_dieLayer->update();
}

// ---------------- 符号层 ----------------

void DiagramBase::addPoints(const std::vector<QPointF> &realPoints, const QColor &color,
                            SymbolShape shape, double radiusPx)
{
    for (const QPointF &c : realPoints)
    {
        m_syms.push_back(SymRec{ c, color, shape, qMax(radiusPx, 1.0) });
    }
    afterSymbolsChanged();
}

void DiagramBase::afterSymbolsChanged()
{
    m_symLayer->update();
}

void DiagramBase::clearPoints()
{
    clearPointMarks();
    m_syms.clear();
    layoutMarks();
    m_symLayer->update();
}

// ---------------- 颜色 ----------------

bool DiagramBase::setDieColor(const QPointF &pos, const QColor &color)
{
    const int i = dieIndexAt(pos);
    if (i < 0)
    {
        return false;
    }
    m_dies[static_cast<size_t>(i)].color = color;
    m_dieLayer->update();
    return true;
}

bool DiagramBase::dieColor(const QPointF &pos, QColor &color) const
{
    const int i = dieIndexAt(pos);
    if (i < 0)
    {
        return false;
    }
    color = m_dies[static_cast<size_t>(i)].color;
    return true;
}

bool DiagramBase::setPointColor(const QPointF &pos, const QColor &color)
{
    const int i = symIndexNear(pos, std::numeric_limits<double>::infinity());
    if (i < 0)
    {
        return false;
    }
    m_syms[static_cast<size_t>(i)].color = color;
    m_symLayer->update();
    return true;
}

bool DiagramBase::pointColor(const QPointF &pos, QColor &color) const
{
    const int i = symIndexNear(pos, std::numeric_limits<double>::infinity());
    if (i < 0)
    {
        return false;
    }
    color = m_syms[static_cast<size_t>(i)].color;
    return true;
}

std::vector<QPointF> DiagramBase::diesOfColor(const QColor &color) const
{
    std::vector<QPointF> out;
    for (const DieRec &d : m_dies)
    {
        if (d.color == color)
        {
            out.push_back(d.real);
        }
    }
    return out;
}

std::vector<QPointF> DiagramBase::pointsOfColor(const QColor &color) const
{
    std::vector<QPointF> out;
    for (const SymRec &s : m_syms)
    {
        if (s.color == color)
        {
            out.push_back(s.real);
        }
    }
    return out;
}

// ---------------- 坐标标注 ----------------

int DiagramBase::markIndex(bool isDie, int target) const
{
    for (size_t i = 0; i < m_marks.size(); ++i)
    {
        if (m_marks[i].isDie == isDie && m_marks[i].target == target)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool DiagramBase::markDie(const QPointF &pos)
{
    const int t = dieIndexAt(pos);
    if (t < 0)
    {
        return false;
    }
    upsertMark(true, t, std::string());       // 空文本 = 快捷模式，自动显示真实坐标
    return true;
}

bool DiagramBase::markPoint(const QPointF &pos)
{
    const int t = symIndexNear(pos, std::numeric_limits<double>::infinity());
    if (t < 0)
    {
        return false;
    }
    upsertMark(false, t, std::string());
    return true;
}

bool DiagramBase::markDieText(const QPointF &pos, const std::string &text)
{
    const int t = dieIndexAt(pos);
    if (t < 0)
    {
        return false;
    }
    upsertMark(true, t, text);
    return true;
}

bool DiagramBase::markPointText(const QPointF &pos, const std::string &text)
{
    const int t = symIndexNear(pos, std::numeric_limits<double>::infinity());
    if (t < 0)
    {
        return false;
    }
    upsertMark(false, t, text);
    return true;
}

bool DiagramBase::unmarkDie(const QPointF &pos)
{
    const int t = dieIndexAt(pos);
    if (t < 0)
    {
        return false;
    }
    const int i = markIndex(true, t);
    if (i < 0)
    {
        return false;
    }
    removeMarkAt(static_cast<size_t>(i));
    return true;
}

bool DiagramBase::unmarkPoint(const QPointF &pos)
{
    const int t = symIndexNear(pos, std::numeric_limits<double>::infinity());
    if (t < 0)
    {
        return false;
    }
    const int i = markIndex(false, t);
    if (i < 0)
    {
        return false;
    }
    removeMarkAt(static_cast<size_t>(i));
    return true;
}

bool DiagramBase::dieMarked(const QPointF &pos) const
{
    const int t = dieIndexAt(pos);
    return t >= 0 && markIndex(true, t) >= 0;
}

bool DiagramBase::pointMarked(const QPointF &pos) const
{
    const int t = symIndexNear(pos, std::numeric_limits<double>::infinity());
    return t >= 0 && markIndex(false, t) >= 0;
}

void DiagramBase::upsertMark(bool isDie, int target, const std::string &text)
{
    int i = markIndex(isDie, target);
    if (i < 0)
    {
        MarkRec m;
        m.isDie = isDie;
        m.target = target;
        m.text = text;
        m_marks.push_back(m);
        i = static_cast<int>(m_marks.size()) - 1;
    }
    else
    {
        m_marks[static_cast<size_t>(i)].text = text;
    }
    rebuildMarkGfx(m_marks[static_cast<size_t>(i)]);
    layoutMarks();
}

void DiagramBase::removeMarkAt(size_t i)
{
    MarkRec &m = m_marks[i];
    if (m.outline)
    {
        scene()->removeItem(m.outline);
        delete m.outline;
        m.outline = nullptr;
    }
    if (m.label)
    {
        scene()->removeItem(m.label);
        delete m.label;
        m.label = nullptr;
    }
    m_marks.erase(m_marks.begin() + static_cast<std::ptrdiff_t>(i));
}

void DiagramBase::clearDieMarks()
{
    for (size_t i = m_marks.size(); i-- > 0; )
    {
        if (m_marks[i].isDie)
        {
            removeMarkAt(i);
        }
    }
}

void DiagramBase::clearPointMarks()
{
    for (size_t i = m_marks.size(); i-- > 0; )
    {
        if (!m_marks[i].isDie)
        {
            removeMarkAt(i);
        }
    }
}

QString DiagramBase::markText(const MarkRec &m) const
{
    if (!m.text.empty())
    {
        return QString::fromStdString(m.text);   // 自定义标注：文本原样
    }
    const QPointF &real = m.isDie ? m_dies[static_cast<size_t>(m.target)].real
                                  : m_syms[static_cast<size_t>(m.target)].real;
    return coordLine(real.x(), real.y());        // 快捷标注：真实坐标
}

void DiagramBase::rebuildMarkGfx(MarkRec &m)
{
    if (m.outline)
    {
        scene()->removeItem(m.outline);
        delete m.outline;
        m.outline = nullptr;
    }
    if (m.label)
    {
        scene()->removeItem(m.label);
        delete m.label;
        m.label = nullptr;
    }

    QPen pen(QColor(30, 30, 140), 2.0);
    pen.setCosmetic(true);
    if (m.isDie)
    {
        const DieRec &d = m_dies[static_cast<size_t>(m.target)];
        const double hw = m_dieW / 2.0;
        const double hh = m_dieH / 2.0;
        auto *rect = new QGraphicsRectItem(d.real.x() - hw, d.real.y() - hh, m_dieW, m_dieH);
        rect->setPen(pen);
        rect->setBrush(Qt::NoBrush);
        rect->setZValue(20);
        scene()->addItem(rect);
        m.outline = rect;
        m.anchor = QPointF(d.real.x() + hw, d.real.y() - hh);   // 方块右上角
    }
    else
    {
        const SymRec &s = m_syms[static_cast<size_t>(m.target)];
        // 环的像素半径跟随符号尺寸规则（符号大小随缩放变化，环也要跟着变）
        const double rr = symbolScreenRadius(s.radiusPx) + 4.0;
        auto *ring = new QGraphicsEllipseItem(QRectF(-rr, -rr, rr * 2.0, rr * 2.0));
        ring->setPen(pen);
        ring->setBrush(Qt::NoBrush);
        ring->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        ring->setPos(s.real);
        ring->setZValue(20);
        scene()->addItem(ring);
        m.outline = ring;
        m.anchor = s.real;
    }

    auto *label = new QGraphicsTextItem;
    label->setHtml(QString("<span style='background-color:rgba(255,255,240,215);'>%1</span>")
                       .arg(markText(m).toHtmlEscaped().replace('\n', "<br>")));
    label->setDefaultTextColor(QColor(20, 20, 20));
    applyMarkFont(label);   // 建标签即按当前 zoom 定字号（新标可能在放大态下打出）
    label->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    label->setZValue(21);
    scene()->addItem(label);
    m.label = label;
    m.label->setPos(m.anchor);   // 先落锚点，layoutMarks 再定终位
}

// 标注贪心避让（设备空间 8 方向）
void DiagramBase::layoutMarks()
{
    if (m_marks.empty())
    {
        return;
    }
    const QTransform tr = viewportTransform();

    std::vector<QRectF> placed;
    placed.reserve(m_marks.size());
    for (MarkRec &m : m_marks)
    {
        if (!m.label)
        {
            continue;
        }
        const QPointF anchorDev = tr.map(m.anchor);
        const QRectF br = m.label->boundingRect();   // 像素尺寸（不随缩放）
        const QSizeF size(br.width(), br.height());

        bool ok = false;
        for (int d = 0; d < 8 && !ok; ++d)
        {
            const QPointF topLeft = anchorDev + kLabelDirs[d];
            const QRectF rect(topLeft, size);
            bool hit = false;
            for (const QRectF &r : placed)
            {
                if (rect.intersects(r))
                {
                    hit = true;
                    break;
                }
            }
            if (hit)
            {
                continue;
            }
            m.label->setPos(mapToScene(topLeft.toPoint()));
            placed.push_back(rect);
            ok = true;
        }
        if (!ok)
        {
            // 无处可放：留在优先方向（衬底光晕保证可读）
            const QPointF topLeft = anchorDev + kLabelDirs[0];
            m.label->setPos(mapToScene(topLeft.toPoint()));
            placed.push_back(QRectF(topLeft, size));
        }
    }
}

void DiagramBase::repositionMarkLabels()
{
    // 标签字号随 zoom 变化（平移时 zoom 不变，applyMarkFont 内部同字号直接跳过）
    for (MarkRec &m : m_marks)
    {
        applyMarkFont(m.label);
    }

    // 符号尺寸依赖缩放：标注环的像素半径跟着符号一起更新（平移/缩放后都会走到这里）
    for (MarkRec &m : m_marks)
    {
        if (m.isDie || !m.outline || m.target < 0 || m.target >= static_cast<int>(m_syms.size()))
        {
            continue;
        }
        if (auto *ring = qgraphicsitem_cast<QGraphicsEllipseItem *>(m.outline))
        {
            const double rr = symbolScreenRadius(m_syms[static_cast<size_t>(m.target)].radiusPx) + 4.0;
            ring->setRect(QRectF(-rr, -rr, rr * 2.0, rr * 2.0));
        }
    }
    layoutMarks();
}

// 标签字号：下限 = 初始字号（缩小不更小，与旧版行为一致）；zoom > 1 后对数阻尼放大
//（px = 基准 × (1 + kMarkFontLogGrow·log2(zoom))，zoom=20 时约 2.3 倍）。整像素量化 +
// 字号不变跳过：滚轮连续缩放不"呼吸"，平移高频调用零开销
void DiagramBase::applyMarkFont(QGraphicsTextItem *label)
{
    if (label == nullptr)
    {
        return;
    }
    if (m_markFontBasePx <= 0)
    {
        // 初始字号从默认字体解析（点值按 96dpi 折像素；一般 9pt → 12px）
        const QFont f = label->font();
        const int ps = f.pixelSize();
        m_markFontBasePx = ps > 0 ? ps : qMax(1, qRound(f.pointSizeF() * 96.0 / 72.0));
        m_markFont = f;
    }
    const double zoom = qMax(m_zoom, 1.0);
    const int px = qRound(m_markFontBasePx * (1.0 + kMarkFontLogGrow * std::log2(zoom)));
    if (label->font().pixelSize() == px)
    {
        return;
    }
    QFont f = m_markFont;
    f.setPixelSize(px);
    label->setFont(f);
}

// ---------------- 图例 ----------------

// 图例文字支持中英文切换：条目只存词条 key（英文原文），
// 每次绘制按当前语言翻译一次，语言切换无需重建控件也不需要外部刷新调用。
// 未加载语言文件 / 词条缺失时 Tr() 原样返回 key，此处回退到固定文本 text（可为空 → 显示 key）。
QString DiagramBase::legendText(const LegendItem &item)
{
    if (item.textKey.empty())
    {
        return QString::fromStdString(item.text);
    }
    const std::string translated = Tr(item.textKey);
    if (translated != item.textKey)
    {
        return QString::fromStdString(translated);
    }
    return QString::fromStdString(item.text.empty() ? item.textKey : item.text);
}

void DiagramBase::setLegendItems(const std::vector<LegendItem> &items)
{
    m_legendItems = items;
    viewport()->update();
}

void DiagramBase::setLegendPos(LegendPos pos)
{
    m_legendPos = pos;
    viewport()->update();
}

void DiagramBase::setLegendVisible(bool on)
{
    m_legendVisible = on;
    // gui 侧这里用动态属性 trLegend 让 UiTranslator 统一刷新图例文本；re 侧没有 UiTranslator，
    // 保留该属性仅作兼容记号（无副作用），文本语言由 legendText() 每次绘制时重新翻译。
    setProperty("trLegend", true);
    viewport()->update();
}

// ---------------- 坐标轴 / 网格 ----------------

void DiagramBase::setAxesVisible(bool on)
{
    m_axesOn = on;
    viewport()->update();
    fitWafer();   // 轴带占用视口，视野需要重排
}

void DiagramBase::setGridVisible(bool on)
{
    m_gridOn = on;
    viewport()->update();
}

void DiagramBase::setGridColor(const QColor &color)
{
    m_gridColor = color;
    viewport()->update();
}

// ---------------- 视图 ----------------

void DiagramBase::fitWafer()
{
    m_needFit = false;
    QRectF cb = contentBounds();
    const double mx = cb.width() * 0.03 + 1.0e-9;
    const double my = cb.height() * 0.03 + 1.0e-9;
    cb.adjust(-mx, -my, mx, my);

    const int bw = m_axesOn ? kAxisBandLeft : 0;
    const int bh = m_axesOn ? kAxisBandBottom : 0;
    const double vw = qMax(viewport()->width() - bw, 20);
    const double vh = qMax(viewport()->height() - bh, 20);

    const double s = qMin(vw / cb.width(), vh / cb.height());
    const QPointF c = cb.center();
    const double tx = bw + vw / 2.0;   // 内容目标中心（视口坐标）
    const double ty = vh / 2.0;

    // 滚动条 0 = 无偏移（与本函数下面手设的矩阵平移一致）；
    // 场景矩形已给到极大，平移量程由滚动条范围提供，这里不能用 minimum()——那是场景边缘
    horizontalScrollBar()->setValue(0);
    verticalScrollBar()->setValue(0);
    // y 轴向上：真实坐标 y 越大越靠上（场景坐标仍是真实坐标，靠视图变换翻转 y；m22 取负）
    setTransform(QTransform(s, 0.0, 0.0, -s, tx - c.x() * s, ty + c.y() * s));
    m_fitScale = s;
    m_zoom = 1.0;
    repositionMarkLabels();
}

void DiagramBase::setZoomLimits(double minZoom, double maxZoom)
{
    m_minZoom = qMax(minZoom, 0.01);
    m_maxZoom = qMax(maxZoom, m_minZoom);
}

// ---------------- 交互 ----------------

void DiagramBase::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton)
    {
        m_pressing = true;
        m_dragging = false;
        m_pressBtn = e->button();
        m_pressPos = m_lastPos = e->pos();
        if (e->button() == Qt::LeftButton)
        {
            setCursor(Qt::ClosedHandCursor);   // 按住 = 可拖动
        }
        e->accept();
        return;
    }
    QGraphicsView::mousePressEvent(e);
}

void DiagramBase::mouseMoveEvent(QMouseEvent *e)
{
    if (m_pressing && m_pressBtn == Qt::LeftButton)
    {
        if (!m_dragging && (qAbs(e->pos().x() - m_pressPos.x()) +
                            qAbs(e->pos().y() - m_pressPos.y())) > kDragThresholdPx)
        {
            m_dragging = true;
        }
        if (m_dragging)
        {
            QScrollBar *h = horizontalScrollBar();
            QScrollBar *v = verticalScrollBar();
            h->setValue(h->value() - (e->pos().x() - m_lastPos.x()));
            v->setValue(v->value() - (e->pos().y() - m_lastPos.y()));
            m_lastPos = e->pos();
            e->accept();
            return;
        }
    }
    QGraphicsView::mouseMoveEvent(e);
}

void DiagramBase::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_pressing && e->button() == m_pressBtn)
    {
        m_pressing = false;
        const bool wasDrag = m_dragging;
        m_dragging = false;
        if (e->button() == Qt::LeftButton)
        {
            setCursor(Qt::OpenHandCursor);
        }
        if (!wasDrag)
        {
            handleClick(e);
        }
        e->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(e);
}

void DiagramBase::handleClick(QMouseEvent *e)
{
    const QPointF sp = mapToScene(e->pos());
    const QPoint gp = e->globalPos();
    emit mapClicked(sp, e->button(), gp);

    // 符号优先（容差 = 该点绘制圆圈 × 1.2，见 symIndexNearClick），其次 die
    const int si = symIndexNearClick(sp);
    if (si >= 0)
    {
        const SymRec &s = m_syms[static_cast<size_t>(si)];
        emit pointClicked(s.real, s.color, e->button(), gp);
        return;
    }
    const int di = dieIndexAt(sp);
    if (di >= 0)
    {
        const DieRec &d = m_dies[static_cast<size_t>(di)];
        emit dieClicked(d.real, d.color, e->button(), gp);
    }
}

void DiagramBase::wheelEvent(QWheelEvent *e)
{
    const double delta = static_cast<double>(e->delta());
    if (qFuzzyIsNull(delta))
    {
        return;
    }
    double f = delta > 0.0 ? 1.15 : 1.0 / 1.15;
    const double nz = qBound(m_minZoom, m_zoom * f, m_maxZoom);
    f = nz / m_zoom;
    if (f <= 0.0)
    {
        return;
    }
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    scale(f, f);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    m_zoom = nz;
    repositionMarkLabels();
    e->accept();
}

// ---------------- 事件杂项 ----------------

void DiagramBase::resizeEvent(QResizeEvent *e)
{
    QGraphicsView::resizeEvent(e);
    if (m_needFit)
    {
        fitWafer();
    }
}

// 语言切换后图例可能仍是旧语言：图静止时视图不会自己 invalidate。
// 调用方两个：UiTranslator::refresh（当前可见页，实时切换）、showEvent（切页回来补一次）。
// 不读 ISTARLanguage 的当前语言枚举（它只有 SetLanguageType、没有读接口），而是拿一个
// 基准词条的译文当探针：译文变了就是换过语言。无硬编码 key 之外的状态，也不需要每组语言一个基准。
void DiagramBase::syncLegendLanguage()
{
    static const std::string kBaseKey = "Chip";   // 任意中英不同、且已登记的词条
    static std::string lastBase;

    const std::string base = Tr(kBaseKey);
    if (base == lastBase)
    {
        return;                                   // 语言没变，不打扰绘制
    }
    lastBase = base;
    viewport()->update();                         // 下一帧按新语言重画图例
}

void DiagramBase::showEvent(QShowEvent *e)
{
    syncLegendLanguage();                         // 切页回来时对上当前语言
    QGraphicsView::showEvent(e);
    if (m_needFit)
    {
        fitWafer();
    }
}

void DiagramBase::paintEvent(QPaintEvent *e)
{
    QGraphicsView::paintEvent(e);

    // HUD 直接画在视口上：轴带 / 网格 / 图例固定，不随内容平移
    QPainter p(viewport());
    const QRect vr = viewport()->rect();
    m_axisOverlay->paint(p, vr);
    m_legendBox->paint(p, vr);
}

void DiagramBase::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    repositionMarkLabels();
}

void DiagramBase::leaveEvent(QEvent *e)
{
    QGraphicsView::leaveEvent(e);
}

// ---------------- 内部工具 ----------------

// 符号（点）屏幕半径的唯一定义处（SymbolLayer 绘制、标注环尺寸、点击命中容差共用）
double DiagramBase::symbolScreenRadius(double nominalPx) const
{
    if (!m_dies.empty())
    {
        // 有 die：与芯片等比缩放——半径恒为 die 屏幕短边的 kSymDieFrac（直径 = 芯片短边 1/8），
        // 芯片多大点就多大，绝不出现"点比芯片大"
        return qMin(m_dieW, m_dieH) * curScale() * kSymDieFrac;
    }
    // 无 die 可参照：用名义像素半径（不随缩放变化）
    return qBound(kSymMinRadiusPx, nominalPx, kSymMaxRadiusPx);
}

// 点击命中符号：容差按各点自己的绘制尺寸算（屏幕半径 × kClickTolFrac，不低于 kClickTolMinPx），
// 画出来的圆圈必然可点；容差不同，比较用归一化距离，取相对最近的那个
int DiagramBase::symIndexNearClick(const QPointF &pos) const
{
    const double scale = qMax(curScale(), 1.0e-9);
    int best = -1;
    double bestNorm = 1.0;   // 归一化距离平方，命中即 <= 1

    for (size_t i = 0; i < m_syms.size(); ++i)
    {
        const double tolPx = qMax(symbolScreenRadius(m_syms[i].radiusPx) * kClickTolFrac,
                                  kClickTolMinPx);
        const double tolScene = tolPx / scale;
        const double dx = m_syms[i].real.x() - pos.x();
        const double dy = m_syms[i].real.y() - pos.y();
        const double norm = (dx * dx + dy * dy) / (tolScene * tolScene);
        if (norm > 1.0)
        {
            continue;   // 超出该点容差：不命中
        }
        if (best < 0 || norm < bestNorm)
        {
            bestNorm = norm;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int DiagramBase::dieIndexAt(const QPointF &pos) const
{
    if (m_dies.empty())
    {
        return -1;
    }
    const long long gx = static_cast<long long>(std::floor(pos.x() / m_dieW));
    const long long gy = static_cast<long long>(std::floor(pos.y() / m_dieH));
    const auto it = m_dieHash.find({ gx, gy });
    if (it == m_dieHash.end())
    {
        return -1;
    }
    const double hw = m_dieW / 2.0;
    const double hh = m_dieH / 2.0;
    for (int idx : it->second)
    {
        const DieRec &d = m_dies[static_cast<size_t>(idx)];
        if (QRectF(d.real.x() - hw, d.real.y() - hh, m_dieW, m_dieH).contains(pos))
        {
            return idx;
        }
    }
    return -1;
}

int DiagramBase::symIndexNear(const QPointF &pos, double tolScene) const
{
    int best = -1;
    double bestD2 = tolScene * tolScene;
    for (size_t i = 0; i < m_syms.size(); ++i)
    {
        const double dx = m_syms[i].real.x() - pos.x();
        const double dy = m_syms[i].real.y() - pos.y();
        const double d2 = dx * dx + dy * dy;
        if (d2 <= bestD2)
        {
            bestD2 = d2;
            best = static_cast<int>(i);
        }
    }
    return best;
}

QRectF DiagramBase::contentBounds() const
{
    QRectF b;
    for (const QGraphicsItem *it : m_backdrops)
    {
        b = b.united(it->boundingRect());
    }
    const double hw = m_dieW / 2.0;
    const double hh = m_dieH / 2.0;
    for (const DieRec &d : m_dies)
    {
        b = b.united(QRectF(d.real.x() - hw, d.real.y() - hh, m_dieW, m_dieH));
    }
    for (const SymRec &s : m_syms)
    {
        b = b.united(QRectF(s.real.x(), s.real.y(), 0.0, 0.0));
    }
    if (b.isEmpty())
    {
        b = QRectF(-10.0, -10.0, 20.0, 20.0);
    }
    return b;
}
