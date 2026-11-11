#ifndef DIAGRAMBASE_H
#define DIAGRAMBASE_H

// 本文件移植自 gui 工程 ProcessModule/Components/DiagramComponent/Base/DiagramBase.{h,cpp}
// （gui 侧把它编进 GUI 可执行程序，re 无法链接，故整体搬入本模块，保证绘图样式逐像素一致）。
// re 侧相对 gui 的差异只有两处：不依赖 gui 工程专有的 Common/UiTranslator.h；图例默认关闭。

#include <QColor>
#include <QFont>
#include <QGraphicsView>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <map>
#include <string>
#include <utility>
#include <vector>

class QGraphicsItem;
class QGraphicsTextItem;

/**
 * 复杂图基础组件：底图 + die 方块层 + 符号层（点/三角/叉…）+ 坐标标注 + 图例 + 坐标轴/网格。
 * 视图内置：按住左键无限平移（不受内容范围限制，拖丢了缩小即可找回）、滚轮以光标为中心缩放，
 * 无滚动条；批量改色自动合并为一次重绘。标注自动贪心避让（8 方向，无处置放时留在优先方向，衬底光晕保证可读；
 * 标注字号随缩放：缩小不小于初始字号，放大按对数阻尼缓慢变大，见 applyMarkFont）。
 *
 * 坐标约定：全部接口只用真实坐标；真实坐标 y 向上，视图变换里 y 取负呈现（场景坐标 == 真实坐标，
 * 业务图元不会被镜像；文字类图元请带 ItemIgnoresTransformations）。
 * 组件另存一份晶圆中心（默认 (0, 0)，setCenter/center 读写），仅方便外部做名义↔真实换算
 * （真实 = 名义 + 中心）；组件内部不使用它，改中心不移动任何已画入的内容。
 *
 * 二次开发（底层 QGraphicsView 直接可用）：
 *   scene 坐标 == 真实坐标，业务图元直接用真实坐标构造后 scene()->addItem() 即可；
 *   z 值分区：底图=-10 die=0 符号=10 标注=20，业务图元请用 z >= 100（盖上层）或 z < -10（垫底图下面）；
 *   setDies / clearPoints / clearDieMarks / clearPointMarks 只清理组件自己的图层，不碰业务图元；
 *   业务图元不参与组件的点击命中 / 图例 / 标注 / 改色服务。
 *
 * 点击命中（一次点击只 resolve 一个对象）：先符号（容差 = 该点绘制屏幕半径 × 1.2，且不小于
 * 6 像素——画出来的圆圈一定点得中，多个点都在范围内时取相对最近），后 die（点在方块内含边界）；
 * 都不是则无对象信号。
 * 按坐标寻址符号的接口（markPoint / setPointColor / pointColor 等）一律按最近符号匹配。
 * 点击信号顺序：mapClicked 必发（先）；命中对象再发 dieClicked / pointClicked（后）。
 */
class DiagramBase : public QGraphicsView
{
    Q_OBJECT

public:
    explicit DiagramBase(QWidget *parent = nullptr);
    ~DiagramBase() override;

    // ---- 底图（可叠加多个；垫在 die 下面，参与视野适配）----
    void drawCircle(double cx, double cy, double diameter,
                    const QColor &line = QColor(120, 120, 120),
                    const QColor &fill = QColor(245, 245, 245));
    void drawSquare(double cx, double cy, double width, double height,
                    const QColor &line = QColor(120, 120, 120),
                    const QColor &fill = QColor(245, 245, 245));
    void drawTriangle(const QPointF &p1, const QPointF &p2, const QPointF &p3,
                      const QColor &line = QColor(120, 120, 120),
                      const QColor &fill = QColor(245, 245, 245));
    void clearBackdrop();           // 清空全部底图

    // ---- 晶圆中心（组件只存值不使用；外部换算用：真实 = 名义 + 中心）----
    void setCenter(double cx, double cy);
    QPointF center() const;

    // ---- die 层（真实坐标中心点；重新调用 = 整层替换，die 标注随之清除）----
    void setDies(const std::vector<QPointF> &realCenters, double dieW, double dieH,
                 const QColor &color = QColor(Qt::lightGray));

    // ---- 符号层（真实坐标；add 只追加不清理；重设 = clearPoints + add，由上层拼）----
    // radiusPx 为名义屏幕像素半径：有 die 时符号与芯片等比（直径恒为芯片屏幕短边 1/8），
    // 缩放时点与芯片同步变化，不会出现点比芯片大；无 die 时按名义值（[2.5,12] 像素夹取）。
    // 详见 symbolScreenRadius()。点击命中容差也按该半径算（见类说明）。
    enum class SymbolShape
    {
        Dot, HollowDot,       // 实心点 / 空心点
        XMark, PlusMark,      // 叉 / 十字
        Triangle, HollowTriangle,
        Square, HollowSquare,
    };
    void addPoints(const std::vector<QPointF> &realPoints, const QColor &color = Qt::black,
                   SymbolShape shape = SymbolShape::Dot, double radiusPx = 5.0);
    void clearPoints();              // 只清符号层及符号标注，不动 die、die 标注、图例

    // ---- 颜色（裸 QColor）----
    bool setDieColor(const QPointF &pos, const QColor &color);       // false = 没命中方块
    bool dieColor(const QPointF &pos, QColor &color) const;          // 查方块当前颜色
    bool setPointColor(const QPointF &pos, const QColor &color);     // false = 当前没有符号
    bool pointColor(const QPointF &pos, QColor &color) const;        // 查符号当前颜色
    std::vector<QPointF> diesOfColor(const QColor &color) const;     // 某颜色全部 die（真实坐标）
    std::vector<QPointF> pointsOfColor(const QColor &color) const;   // 某颜色全部符号（真实坐标）

    // ---- 坐标标注（幂等：同一对象永远只有一条，重复标注 = 更新，两类共用同一槽位）----
    bool markDie(const QPointF &pos);         // 快捷：标注显示真实坐标；false = 没命中方块
    bool markPoint(const QPointF &pos);       // 快捷：标注显示真实坐标；false = 没命中符号
    bool markDieText(const QPointF &pos, const std::string &text);    // 自定义标注文本
    bool markPointText(const QPointF &pos, const std::string &text);  // 自定义标注文本
    bool unmarkDie(const QPointF &pos);              // 清除该方块标注；false = 本来就没标注
    bool unmarkPoint(const QPointF &pos);            // 清除该符号标注；false = 本来就没标注
    bool dieMarked(const QPointF &pos) const;        // 该方块当前是否已有标注（toggle 逻辑用）
    bool pointMarked(const QPointF &pos) const;
    void clearDieMarks();                            // 清全部 die 标注
    void clearPointMarks();                          // 清全部符号标注

    // ---- 图例 ----
    enum class LegendPos { TopLeft, TopRight, BottomLeft, BottomRight };
    struct LegendItem
    {
        QColor color;
        SymbolShape shape = SymbolShape::Square;    // Square=色块 ■，其余画对应符号
        std::string textKey;                        // 词条 key（英文原文，支持中英文切换）；有它时优先
        std::string text;                           // 固定文本：textKey 空、原文即译文或未加载语言文件时用
    };
    void setLegendItems(const std::vector<LegendItem> &items);   // 整体替换
    void setLegendPos(LegendPos pos);                            // 默认 TopRight
    void setLegendVisible(bool on);                              // 默认 false，需要时打开
    // 图例条目当前语言的实际显示文本：textKey 非空则每次绘制都按当前语言重新翻译
    //（语言切换无需重建控件 / 显式刷新，见 LegendBox::paint），否则回退到固定文本 text。
    static QString legendText(const LegendItem &item);

    // ---- 坐标轴 / 网格（默认开；轴带占视口左、下边缘，刻度随缩放自适应，不与图形重叠）----
    void setAxesVisible(bool on);
    void setGridVisible(bool on);               // 虚线网格，步距 = 轴刻度步距
    void setGridColor(const QColor &color);

    // ---- 视图 ----
    void fitWafer();                                  // 复位：整图适配当前窗口
    void setZoomLimits(double minZoom, double maxZoom);   // 相对 fitWafer 视野的倍率，默认 [0.2, 20]

signals:
    // 任意点击必发（先发）；globalPos 为屏幕坐标，弹窗定位用
    void mapClicked(const QPointF &real, Qt::MouseButton btn, const QPoint &globalPos);
    // real 回传建图原值；color = 被点对象当前颜色（后发）
    void dieClicked(const QPointF &real, const QColor &color,
                    Qt::MouseButton btn, const QPoint &globalPos);
    void pointClicked(const QPointF &real, const QColor &color,
                      Qt::MouseButton btn, const QPoint &globalPos);

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    void scrollContentsBy(int dx, int dy) override;   // 平移时保持标注像素偏移
    void leaveEvent(QEvent *e) override;              // 离开视口事件透传

private:
    struct DieRec { QPointF real; QColor color; };
    struct SymRec
    {
        QPointF real;
        QColor color;
        SymbolShape shape = SymbolShape::Dot;
        double radiusPx = 5.0;        // 名义屏幕像素半径（实际尺寸见 symbolScreenRadius）
    };
    struct MarkRec
    {
        int target = -1;                 // m_dies / m_syms 下标
        bool isDie = true;
        std::string text;                // 附注行
        QGraphicsItem *outline = nullptr;
        QGraphicsTextItem *label = nullptr;
        QPointF anchor;                  // 标注锚点（场景坐标）
    };

    class DieLayer;        // z=0  全部 die
    class SymbolLayer;     // z=10 全部符号（固定像素大小）
    class AxisOverlay;     // HUD：坐标轴带 + 虚线网格（paintEvent 里按视口绘制，不随平移移动）
    class LegendBox;       // HUD：图例（同上）

    void addBackdrop(QGraphicsItem *shape);   // 底图叠加

    int  dieIndexAt(const QPointF &pos) const;                     // 方块内含边界，未命中 -1
    int  symIndexNear(const QPointF &pos, double tolScene) const;  // 最近符号，超容差 -1
    int  symIndexNearClick(const QPointF &pos) const;              // 点击命中：容差按各点绘制尺寸，取相对最近
    int  markIndex(bool isDie, int target) const;
    void upsertMark(bool isDie, int target, const std::string &text);
    void removeMarkAt(size_t i);
    void rebuildMarkGfx(MarkRec &m);
    void layoutMarks();             // 标注贪心避让布局（设备空间，随平移缩放重排）
    void repositionMarkLabels();
    void applyMarkFont(QGraphicsTextItem *label);   // 标签字号：下限=初始字号，zoom>1 对数阻尼放大
    QString markText(const MarkRec &m) const;
    void syncLegendLanguage();   // 语言变化时补一次图例重绘（UiTranslator::refresh / showEvent 调用）
    void handleClick(QMouseEvent *e);
    QRectF contentBounds() const;                    // 底图 + die + 符号包围盒，无内容给默认
    void finishSetDies();
    void afterSymbolsChanged();
    double symbolScreenRadius(double nominalPx) const;   // 符号（点）实际屏幕半径：唯一定义处
    double curScale() const { return transform().m11(); }   // 像素 / 场景单位

    std::vector<DieRec> m_dies;
    double m_dieW = 1.0, m_dieH = 1.0;
    std::map<std::pair<long long, long long>, std::vector<int>> m_dieHash;   // die 空间索引
    std::vector<SymRec> m_syms;
    std::vector<MarkRec> m_marks;
    QFont m_markFont;                    // 标注基准字体（首次用时从默认字体解析）
    int m_markFontBasePx = 0;            // 基准字号（像素；<=0 = 尚未解析）
    QPointF m_center;                                 // 晶圆中心（仅存值，外部换算用），默认 (0, 0)

    std::vector<QGraphicsItem *> m_backdrops;         // 底图（可叠加）
    DieLayer *m_dieLayer = nullptr;
    SymbolLayer *m_symLayer = nullptr;
    AxisOverlay *m_axisOverlay = nullptr;
    LegendBox *m_legendBox = nullptr;

    std::vector<LegendItem> m_legendItems;
    bool m_legendVisible = false;   // re 侧默认关闭（gui 侧为 true）：需要时由调用方 setLegendVisible(true)
    LegendPos m_legendPos = LegendPos::TopRight;

    bool m_axesOn = true, m_gridOn = true;
    QColor m_gridColor = QColor(160, 160, 160);

    double m_zoom = 1.0;          // 相对 fitWafer 视野的倍率
    double m_minZoom = 0.2, m_maxZoom = 20.0;
    double m_fitScale = 1.0;      // fit 时 像素/场景单位
    bool m_needFit = true;        // 有数据后首次显示时自动 fitWafer

    bool m_pressing = false, m_dragging = false;
    Qt::MouseButton m_pressBtn = Qt::NoButton;
    QPoint m_pressPos, m_lastPos;
};

#endif // DIAGRAMBASE_H
