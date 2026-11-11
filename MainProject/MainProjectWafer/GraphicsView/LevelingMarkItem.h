#ifndef LEVELINGMARKITEM_H
#define LEVELINGMARKITEM_H

#include <QObject>
#include <QGraphicsObject>
#include <QPointF>
#include <QPainterPath>

// 晶圆调平点位标记项：
// 在 gv_preview_2 上按名义坐标 (x, y) 显示一个可拖动的标记符号（十字/星/方/芯片标定圆点），
// 与调平点位表格的行一一对应；拖动时发出 sig_moved(row, 场景坐标) 供视图回写表格。
// 另支持“候选点”形态（clickOnly + dimmed）：只响应左键点击、半透明绘制，用于 Chip 标定的可选点位。
//
// 造型对齐 gui 工程的图形引擎（gui/MainProject/ProcessModule/Components/DiagramComponent/Base/DiagramBase
// 的 drawSymbol）：四种形状映射到 gui 的符号集 ——
//   十字 → PlusMark（两线）、晶圆调平("Star") → 橙色实心菱形、方 → Square（1.8r 实心方）、
//   Chip 标定 → Dot（实心圆点）；
// 尺寸只由芯片短边决定（半径 = 芯片短边 × 该类标记的比例：十字/菱形/方用 kSymDieFrac，
// Chip 标定圆点用更小的 kCalibDieFrac，均不设上下限），描边用 cosmetic 屏幕像素宽；
// 拖动/锁定/选中/候选点等交互仍是本项目自己的。
class LevelingMarkItem : public QGraphicsObject
{
    Q_OBJECT
public:
    // chipWidth/chipHeight：芯片尺寸（场景单位），>0 时符号与芯片等比（半径 = 芯片短边 × kSymDieFrac）；
    // 两个都为 0（尺寸未知）时用固定缺省半径，同样不设上下限；
    // 命中区另有一个不可见下限（m_hitHalf = 芯片短边 × 60%，未知时 7.0），保证小芯片上也能点中/拖动；
    // locked=true 时标记不可拖动/点击/选中（用于 Chip 标定中已 Confirm 的点）
    explicit LevelingMarkItem(int row, const QPointF &sceneCenter, const QString &markType, int table = 0,
                              double chipWidth = 0.0, double chipHeight = 0.0,
                              bool locked = false, QGraphicsItem *parent = nullptr);

    int row() const { return m_row; }
    int table() const { return m_table; }
    QString markType() const { return m_markType; }
    bool locked() const { return m_locked; }
    void setRow(int row) { m_row = row; }
    void setMarkType(const QString &markType) { m_markType = markType; }
    // 表格行选中时描边高亮
    void setMarkSelected(bool selected);
    // 锁定/解锁：锁定后不可拖动、点击、选中（已 Confirm 的 Chip 标定点）
    void setLocked(bool locked);
    // 虚色/实色：dimmed=true 时半透明绘制（候选点未被选中），false 时与原标记同色（实色）
    void setDimmed(bool dimmed);
    bool isDimmed() const { return m_dimmed; }
    // 候选点模式：只响应左键点击（不可拖动/选中/描边，悬停显示手型）；
    // 候选点的序号复用 row 字段（不占表格行）
    void setClickOnly(bool clickOnly);
    bool clickOnly() const { return m_clickOnly; }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

signals:
    // 拖动过程中/释放时发出的当前场景坐标
    void sig_moved(int row, int table, const QPointF &scenePos);
    // 标记点被点击时发出（供视图高亮该标记并同步表格行选中）
    void sig_clicked(int row, int table);
    // 候选点被点击时发出（仅 clickOnly 模式发出；index 为候选点序号，table 为标记表号）
    void sig_candidateClicked(int index, int table);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    QPainterPath shape() const override;  //让命中形状与画出来的圆点一致

private:
    void drawMark(QPainter *painter) const;
    // 依据 m_clickOnly / m_locked 更新交互 flags
    // （候选点：只点不拖；锁定：完全不可交互）
    void updateInteractionFlags();
    // 可见符号半径（场景单位）：与芯片等比，每类标记各有自己的比例——
    //   十字 / 晶圆调平菱形 / 方：芯片短边 × kSymDieFrac
    //   Chip 标定圆点与候选点：芯片短边 × kCalibDieFrac（圆点更小）
    // 不设任何像素上下限；芯片尺寸未知时返回固定缺省半径 kDefaultRadius
    qreal symbolRadius() const;

    int m_row;
    int m_table;
    QString m_markType;
    bool m_mousePressed = false;   // 仅用户拖动期间置真，程序 setPos 不发信号
    bool m_markSelected = false;   // 表格行选中时描边高亮
    double m_chipShortSide = 0.0;  // 芯片短边（场景单位）；<=0 表示芯片尺寸未知
    double m_hitHalf = 7.0;        // 命中区半宽：不可见，另留下限，保证小芯片上仍能点中/拖动
    bool m_locked = false;         // 锁定：不可拖动/点击/选中（Chip 标定 Confirm 后的点）
    bool m_dimmed = false;         // 虚色：半透明绘制（候选点未被选中）
    bool m_clickOnly = false;      // 候选点模式：只响应左键点击，不可拖动/选中
};

#endif // LEVELINGMARKITEM_H
