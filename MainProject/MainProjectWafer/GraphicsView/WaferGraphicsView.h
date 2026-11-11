#ifndef WAFERGRAPHICSVIEW_H
#define WAFERGRAPHICSVIEW_H

#include "BondPointRectItem.h"
#include "DiagramBase.h"
#include "LevelingMarkItem.h"

#include <QFont>
#include <QGraphicsView>
#include <QList>
#include <QMap>
#include <QPoint>
#include <QPointF>
#include <vector>

class QMouseEvent;
class QGraphicsTextItem;   // 仅前向声明：点击标记后显示的坐标标注图元在 .cpp 中创建

// 处方数据层的名义坐标类型（S_Point2D）：仅用于下方便利重载，这里只做前置声明（它是 class 不是 struct）
namespace ISTAR { namespace BasicPlatform { namespace BasicValue { class S_Point2D; } } }

// 晶圆处方图形视图模块：
// 绘图引擎沿用 gui 工程 ProcessModule/Components/DiagramComponent/Base/DiagramBase
// （底图 + die 芯片层 + 符号层 + HUD 轴带/虚线网格 + 平移缩放 + 点击命中 + 坐标标注），
// 本类在其上实现本项目的全部对外功能（芯片预览、键合选点、调平标记、Chip 标定候选点）。
// 坐标约定与 gui 一致：场景坐标 == 名义坐标（y 向上），视图变换里 y 取负呈现。
// 用于在 .ui 中把 gv_preview / gv_bond 提升(提升为)为本类。
class WaferGraphicsView : public DiagramBase
{
    Q_OBJECT
public:
    explicit WaferGraphicsView(QWidget *parent = nullptr);

    // 单芯片编辑预览：按芯片中心点绘制芯片矩形（黄色），chipPositions 为名义坐标
    void DisplayChipLayout(const QList<QPointF> &chipPositions, double chipWidth, double chipHeight);

    // 键合选点：绘制可点击选中的芯片矩形，并按已选点列表初始化选中状态
    void DisplayBondPoints(const QList<QPointF> &chipPositions, double chipWidth, double chipHeight, const QList<QPointF> &bondPoints);

    // 设置晶圆半径：DisplayChipLayout 重建场景时以原点为圆心绘制晶圆外轮廓（无填充）
    void SetWaferRadius(double radius);
    // 设置切边角度(0/90/180/270)：在晶圆外侧画指示切边位置的小三角，尖端指向圆心
    void SetWaferNotchAngle(int angle);
    // 获取当前选中的键合点（名义坐标列表）
    QList<QPointF> GetSelectedBondPoints() const;
    // 设置键合选点是否可编辑（非编辑模式下芯片矩形不可点击选中）
    void SetBondEditable(bool editable);
    // 设置单选模式：开启后一次只能选中一个点位，点击新点位会自动取消其它选中
    void SetSingleSelectMode(bool single);
    // 全选：所有键合点位都设为选中
    void SelectAllBondPoints();
    // 清空：所有键合点位都取消选中
    void ClearAllBondPoints();

    // ---- 晶圆调平点位标记 ----
    // 设置/新增第 row 行的调平标记：在名义坐标 (nominalPos.x, nominalPos.y) 处显示 markType 符号；
    // row 与表格行一一对应，已存在则原位更新；locked=true 创建即锁定（不可拖/点/选）；
    // 已锁定标记不接受任何原位更新（需先解锁/删除重建）
    void SetLevelingPoint(int row, const QPointF &nominalPos, const QString &markType, int table = 0, bool locked = false);
    // 锁定（确认）第 row 行标记：此后不可拖动/点击/选中，也不可再被 SetLevelingPoint 移动
    void SetLevelingPointLocked(int row, int table = 0);
    // 删除第 row 行的调平标记（连同该行标记点一并删除），并使其后所有行号 -1，
    // 保证剩余标记点的行号与表格行重新一一对应（表格删除行后调用）
    void RemoveLevelingPoint(int row, int table = 0);
    // 仅删除第 row 行（表格行仍存在、仅坐标未填/被清空）对应的标记点，不做行号平移
    void RemoveLevelingPointExact(int row, int table = 0);
    // 交换 rowA/rowB 两行的标记，二者行号互换（表格上移/下移后调用）
    void MoveLevelingPoint(int rowA, int rowB, int table = 0);
    // 清空全部调平标记
    void ClearLevelingPoints();
    // 切换调平标记是否可拖动（非编辑模式下禁用拖动）
    void SetLevelingEditable(bool editable);
    // 高亮第 row 行对应标记点（描边），row<0 时清除全部高亮；由表格行选中驱动
    void SetLevelingSelectedRow(int row, int table = 0);
    // 打开/关闭“选点态”：开启后单击候选点发出 sig_levelingCandidateClicked，
    // 点其它位置（空白/坐标轴）一律吞掉，不再允许自由放置点位
    void SetLevelingPickMode(bool on);

    // ---- Chip 标定候选点（选点态显示的虚色可点圆点）----
    // 设置候选点（名义坐标：晶圆绝对名义坐标，晶圆中心即原点，内部按 scene(x, -y) 绘制）：
    // 每个候选点画成半透明深蓝实心圆点（虚色），被点中后由外部置为实色（SetLevelingCandidateSelected）；
    // usedIndices 中的候选点不绘制（该位置已被表格行占用，不可重复选）；
    // 候选点仅在选点态下可点（SetLevelingPickMode(true) 时统一启用）；
    // table 与调平标记的表号同义（Chip 标定为 0），用于区分不同视图的候选点层。
    void SetLevelingCandidates(const QList<QPointF> &nominalPoints, int table = 0,
                               const QList<int> &usedIndices = QList<int>());
    // 清空候选点层（退出选点态时调用）
    void ClearLevelingCandidates(int table = 0);
    // 选中态：第 index 个候选点转为实色，其余保持虚色；index < 0 表示全部取消选中
    void SetLevelingCandidateSelected(int index, int table = 0);
    // 当前候选点集合（名义坐标，与 SetLevelingCandidates 传入的一致，含已占用项）
    QList<QPointF> LevelingCandidates() const { return m_lvlCandidates; }

    // ---- 便利重载：处方数据层 S_Point2D → Qt 类型（内部按 x_/y_ 转成 QPointF）----
    // 便于上层（ChipCalibrationController）直接传 chip 处方的 S_Point2D / std::vector<S_Point2D>
    void SetLevelingPoint(int row, const ISTAR::BasicPlatform::BasicValue::S_Point2D &nominalPos,
                          const QString &markType, int table = 0, bool locked = false);
    void SetLevelingCandidates(const std::vector<ISTAR::BasicPlatform::BasicValue::S_Point2D> &nominalPoints,
                               int table = 0, const QList<int> &usedIndices = QList<int>());

signals:
    // 选中键合点数量变化
    void sig_selectedCountChanged(int count);
    // 调平标记被拖动/释放时发出（row/table 为标记所属表格与行号，nominalPos 为名义坐标）
    void sig_levelingPointMoved(int row, int table, const QPointF &nominalPos);
    // 调平标记被点击时发出（供外部同步表格行选中）
    void sig_levelingPointClicked(int row, int table);
    // 候选点被点击时发出（选点态；index 为候选点序号，table 为标记表号）
    void sig_levelingCandidateClicked(int index, int table);

protected:
    // 滚轮缩放（以鼠标位置为缩放中心）
    void wheelEvent(QWheelEvent *event) override;
    // 方向键平移视图
    void keyPressEvent(QKeyEvent *event) override;
    // 首次显示时按场景范围自适应缩放
    void resizeEvent(QResizeEvent *event) override;
    // 鼠标左键路由：选点态只放行候选点；命中本项目图元（标记/候选点/键合矩形）交给图元处理；
    // 其余位置交给引擎做左键平移（gui 手感）
    void mousePressEvent(QMouseEvent *event) override;

private:
    // 键合矩形点击后更新选中列表与数量
    void updateSelectedState(BondPointRectItem *item, bool selected);
    // 按芯片包围盒自适应放大视图（内部走引擎的 fitWafer：3% 边距 + 让出 HUD 轴带宽度）
    void fitToChips(const QRectF &bounds);
    // 按 m_lvlMarks 数据源重建全部调平标记图元（scene 清空后用于恢复标记）
    void rebuildLevelingItems();
    // 按 m_lvlCandidates 数据源重建全部候选点图元（候选点更新/scene 重建后调用）
    void rebuildCandidateItems();
    // 调平标记拖动信号：场景坐标转名义坐标并回写数据源
    void onLevelingMarkMoved(int row, int table, const QPointF &scenePos);
    // 调平标记点击信号：唯一高亮该标记并通知外部
    void onLevelingMarkClicked(int row, int table);
    // 在标记点旁显示/更新其名义坐标标注（gui 样式：浅黄底光晕、深色字、忽略视图变换；
    // 位置取 gui 的首选方向 NE，即点位右上 7px/-6px 处）
    void showMarkCoord(const QPointF &scenePos);
    // 取消当前坐标标注（再点同一个标记、或标记被删/清空时调用）
    void hideMarkCoord();
    // 标注字号随缩放：下限 = 基准字号，放大按对数阻尼（gui 的 applyMarkFont 规则）
    void updateCoordLabelFont();

    // ---- 仅内部使用（外部无需调用，故不对外暴露） ----
    // 清空"本项目自己的"业务图元（键合矩形/调平标记/候选点/坐标文字）以及引擎图层内容；
    // 注意：不能整场景 clear —— 引擎的 die 层/符号层图元会被一起删掉
    void ClearScene();
    // 视图居中到场景原点
    void CenterOnOrigin();
    // 获取第 row 行的标记名义坐标（无则返回 null QPointF）
    QPointF GetLevelingPoint(int row, int table = 0) const;

private:
    // 调平标记数据源（行号 -> 名义坐标/类型/锁定态），scene 清空后据此重建图元
    struct LvlMarkData {
        QPointF nominalPos;
        QString markType;
        bool locked = false;       // 已确认（锁定）标记：不可移动/点击/选中
    };

    QGraphicsScene *m_pScene = nullptr;        // 图形场景
    QList<BondPointRectItem *> m_lstBondItems; // 键合矩形项
    QList<QPointF> m_lstSelectedPoints;        // 当前选中的键合点
    int m_nSelectedCount = 0;                  // 选中数量
    bool m_bIsInitialFit = true;               // 是否处于首次布局
    QRectF m_chipBounds;                       // 最近一次绘制的芯片包围盒
    bool m_hasChipBounds = false;              // 是否已有芯片包围盒数据
    bool m_bondEditable = false;               // 键合选点是否可编辑
    bool m_singleSelect = false;               // 单选模式：一次只能选中一个
    double m_waferRadius = 0.0;                // 晶圆半径(>0 时绘制外轮廓圆)
    int m_waferNotchAngle = 0;                 // 切边角度(0/90/180/270)
    QMap<std::pair<int,int>, LvlMarkData> m_lvlMarks;            // 调平标记数据源（行号,表号 -> 数据）
    QMap<std::pair<int,int>, LevelingMarkItem *> m_lvlMarkItems; // 调平标记图元（行号,表号 -> item）
    bool m_lvlEditable = false;                // 调平标记是否可拖动
    bool m_lvlPickActive = false;              // 选点态：只允许点击候选点
    int m_lvlCandidateTable = 0;               // 候选点所属表号（同调平标记 table，Chip 标定为 0）
    QList<QPointF> m_lvlCandidates;            // 候选点数据源（名义坐标：晶圆绝对名义坐标）
    QList<int> m_lvlCandidateUsed;             // 已占用候选序号（不绘制、不可点）
    int m_lvlCandidateSelected = -1;           // 当前选中的候选序号（-1 表示无）
    QList<LevelingMarkItem *> m_lvlCandidateItems; // 候选点图元
    double m_lvlMarkChipWidth = 0.0;           // 芯片尺寸：调平标记按芯片短边缩放（DisplayChipLayout 记录）
    double m_lvlMarkChipHeight = 0.0;
    QGraphicsTextItem *m_pCoordLabel = nullptr;  // 坐标标注图元（gui 样式：浅黄底光晕 + 忽略视图变换）
    int m_coordLabelRow = -1;                    // 当前标注对应的标记行号（-1 = 无标注）：再点同一个即取消
    int m_coordLabelTable = -1;                  // 当前标注对应的表号
    double m_fitScale = 1.0;                     // 最近一次自适应时的像素/场景单位（算字号随缩放倍率用）
    int m_markFontBasePx = 0;                    // 标注基准字号（像素；<=0 = 尚未解析）
    QFont m_markFontBase;                        // 标注基准字体

    QList<QPointF> m_chipCenters;   // 各芯片中心（名义坐标）：Chip 标定拾取合法性判定用
};

#endif // WAFERGRAPHICSVIEW_H
