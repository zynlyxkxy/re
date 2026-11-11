#include "WaferGraphicsView.h"

#include "ReData/S_RecipeCommon.h"   // S_Point2D：下方便利重载用（处方数据层的名义坐标）

#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsPolygonItem>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTransform>
#include <QPixmap>
#include <QBrush>
#include <QPen>
#include <QPainter>
#include <QtMath>
#include <cmath>

WaferGraphicsView::WaferGraphicsView(QWidget *parent)
    : DiagramBase(parent)   // 绘图引擎（移植自 gui 工程 DiagramComponent/Base）：底图/芯片层/符号层/HUD 轴带
{
    // 抗锯齿、无滚动条、左键平移光标、±1e6 场景矩形、场景与 die/符号图层等都在 DiagramBase 构造里完成。
    // 背景不复用 gui 的自绘白色网格底：视口底色用调色板 Base（白）、轴带用调色板 Window，与 gui 一致。
    m_pScene = scene();    // 引擎已建好场景，这里只取指针供本项目业务图元使用

    setAxesVisible(true);  // HUD 坐标轴带：固定在视口左/下边缘，不随平移移动，刻度随缩放自适应
    setGridVisible(true);  // 虚线网格，步距 = 轴刻度步距
}

void WaferGraphicsView::ClearScene()
{
    // 只清"本项目自己的"业务图元；引擎的 die 层/符号层/底图必须走基类接口，不能 scene()->clear()
    for (BondPointRectItem *item : m_lstBondItems)
    {
        m_pScene->removeItem(item);
        delete item;
    }
    m_lstBondItems.clear();

    for (LevelingMarkItem *item : m_lvlMarkItems)
    {
        m_pScene->removeItem(item);
        delete item;
    }
    m_lvlMarkItems.clear();

    for (LevelingMarkItem *item : m_lvlCandidateItems)
    {
        m_pScene->removeItem(item);
        delete item;
    }
    m_lvlCandidateItems.clear();

    hideMarkCoord();   // 坐标标注图元连同其行号记录一起清掉

    // 引擎图层内容：底图（晶圆轮廓/切边）、芯片层、符号层与各自的标注
    clearBackdrop();
    setDies(std::vector<QPointF>(), 1.0, 1.0);
    clearPoints();

    m_lstSelectedPoints.clear();
    m_nSelectedCount = 0;
    // 数据源 m_lvlMarks / m_lvlCandidates 保留：场景重建后据此恢复图元
}

void WaferGraphicsView::SetWaferRadius(double radius)
{
    // 半径由外部(晶圆尺寸/边缘长度)计算并设置；仅在 DisplayChipLayout 重建场景时生效
    m_waferRadius = radius;
}

void WaferGraphicsView::SetWaferNotchAngle(int angle)
{
    // 切边角度；仅在 DisplayChipLayout 重建场景时生效
    m_waferNotchAngle = angle;
}

void WaferGraphicsView::DisplayChipLayout(const QList<QPointF> &chipPositions, double chipWidth, double chipHeight)
{
    // 记录芯片名义坐标：Chip 标定拾取时用它判断落点是否在芯片内/内切圆外
    m_chipCenters = chipPositions;

    // 单芯片编辑预览：清空后按名义坐标重新绘制（引擎图层 + 本项目业务图元）
    ClearScene();

    // ---- 底图：晶圆外轮廓圆 + 切边小三角（gui 风格：细灰描边 + 浅色填充，垫在芯片下面 z=-10）----
    if (m_waferRadius > 0)
    {
        drawCircle(0.0, 0.0, m_waferRadius * 2.0);

        // 切边方向（名义坐标，y 向上）：0°→名义 -y、90°→+x、180°→+y、270°→-x；
        // 小三角位于晶圆外侧、尖端在圆边缘（指向圆心）
        QPointF dirOut;
        switch (m_waferNotchAngle)
        {
            case 0:   dirOut = QPointF( 0, -1); break;
            case 90:  dirOut = QPointF( 1,  0); break;
            case 180: dirOut = QPointF( 0,  1); break;
            case 270: dirOut = QPointF(-1,  0); break;
            default:  dirOut = QPointF(); break;
        }
        if (!dirOut.isNull())
        {
            const double h = qMax(8.0, m_waferRadius * 0.06);   // 三角高度
            const double w = h * 0.6;                            // 半底宽
            const QPointF perp(-dirOut.y(), dirOut.x());
            const QPointF apex = dirOut * m_waferRadius;              // 尖端在圆边缘
            const QPointF baseMid = dirOut * (m_waferRadius + h);     // 底边在圆外侧
            drawTriangle(apex, baseMid + perp * w, baseMid - perp * w);
        }
    }

    // ---- 芯片：交给引擎的 die 层（黑 1px cosmetic 描边 + 黄色填充），样式与 gui 一致 ----
    std::vector<QPointF> chips;
    chips.reserve(static_cast<size_t>(chipPositions.size()));
    for (const QPointF &pos : chipPositions)
    {
        chips.push_back(pos);   // 名义坐标即场景坐标（视图变换里 y 取负呈现）
    }
    setDies(chips, chipWidth, chipHeight, QColor(Qt::yellow));

    // 芯片包围盒（连同底图圆一起，供取景判断用）
    QRectF bounds;
    for (const QPointF &pos : chipPositions)
    {
        bounds = bounds.united(QRectF(pos.x() - chipWidth / 2, pos.y() - chipHeight / 2, chipWidth, chipHeight));
    }
    if (m_waferRadius > 0)
    {
        bounds = bounds.united(QRectF(-m_waferRadius, -m_waferRadius, m_waferRadius * 2, m_waferRadius * 2));
    }
    m_chipBounds = bounds;
    m_hasChipBounds = true;

    // 记录芯片尺寸：调平标记按芯片短边缩放（S_DwChipMatRef 数据源，随布局刷新）
    m_lvlMarkChipWidth = chipWidth;
    m_lvlMarkChipHeight = chipHeight;

    fitToChips(bounds);
    // 场景重建后恢复调平标记与候选点（数据源不随 scene 清空而丢失）
    rebuildLevelingItems();
    rebuildCandidateItems();
}

void WaferGraphicsView::DisplayBondPoints(const QList<QPointF> &chipPositions, double chipWidth, double chipHeight, const QList<QPointF> &bondPoints)
{
    // 键合选点：绘制可点击选中的芯片矩形
    ClearScene();

    // 芯片同时铺一层引擎 die（样式与 gui 一致，并让引擎的取景/命中知道内容范围）；
    // 可点击的业务矩形 BondPointRectItem 按 gui 的二次开发约定叠在其上（z >= 100）
    std::vector<QPointF> chips;
    chips.reserve(static_cast<size_t>(chipPositions.size()));
    for (const QPointF &pos : chipPositions)
    {
        chips.push_back(pos);
    }
    setDies(chips, chipWidth, chipHeight, QColor(Qt::yellow));

    QRectF bounds;
    for (const QPointF &pos : chipPositions)
    {
        BondPointRectItem *item = new BondPointRectItem(pos.x() - chipWidth / 2, pos.y() - chipHeight / 2, chipWidth, chipHeight);
        item->SetNominalPos(pos.x(), pos.y());
        // 已存在于键合点列表中的芯片位初始化为选中
        if (bondPoints.contains(pos))
            item->SetSelected(true);
        connect(item, &BondPointRectItem::sig_selectedStateChanged, this,
                [this, item](bool selected) { updateSelectedState(item, selected); });
        item->setZValue(100);   // 业务图元层（引擎：底图 -10 / die 0 / 符号 10 / 标注 20）
        m_pScene->addItem(item);
        item->setEnabled(m_bondEditable);
        m_lstBondItems.append(item);
        bounds = bounds.united(item->sceneBoundingRect());
    }

    // 统计初始选中数量
    m_lstSelectedPoints.clear();
    m_nSelectedCount = 0;
    for (BondPointRectItem *item : m_lstBondItems)
    {
        if (item->IsSelected())
        {
            m_lstSelectedPoints.append(item->GetNominalPos());
            m_nSelectedCount++;
        }
    }

    // 单选模式：初始只保留第一个选中点
    if (m_singleSelect && m_nSelectedCount > 1)
    {
        QPointF first = m_lstSelectedPoints.first();
        for (BondPointRectItem *item : m_lstBondItems)
        {
            if (item->IsSelected() && item->GetNominalPos() != first)
                item->SetSelected(false);
        }
        m_lstSelectedPoints.clear();
        m_lstSelectedPoints.append(first);
        m_nSelectedCount = 1;
    }
    emit sig_selectedCountChanged(m_nSelectedCount);
    m_chipBounds = bounds;
    m_hasChipBounds = true;
    fitToChips(bounds);
}

void WaferGraphicsView::updateSelectedState(BondPointRectItem *item, bool selected) // 键合矩形点击后更新选中列表与数量
{
    if (m_singleSelect && selected)
    {
        // 单选模式：点击选中一个时，取消其它所有选中（SetSelected 不触发信号，无重入）
        for (BondPointRectItem *other : m_lstBondItems)
        {
            if (other != item && other->IsSelected())
                other->SetSelected(false);
        }
        m_lstSelectedPoints.clear();
        m_lstSelectedPoints.append(item->GetNominalPos());
        m_nSelectedCount = 1;
    }
    else
    {
        // 多选：点击后更新选中列表与数量
        QPointF pos = item->GetNominalPos();
        if (selected)
        {
            if (!m_lstSelectedPoints.contains(pos))
                m_lstSelectedPoints.append(pos);
            m_nSelectedCount++;
        }
        else
        {
            m_lstSelectedPoints.removeOne(pos);
            m_nSelectedCount--;
        }
    }
    emit sig_selectedCountChanged(m_nSelectedCount);
}

QList<QPointF> WaferGraphicsView::GetSelectedBondPoints() const // 获取当前选中的键合点（名义坐标列表）
{
    return m_lstSelectedPoints;
}

void WaferGraphicsView::CenterOnOrigin() // 视图居中到场景原点
{
    // 视图居中到场景原点
    centerOn(0, 0);
    update();
}

void WaferGraphicsView::SetBondEditable(bool editable)
{
    // 切换键合选点编辑状态：禁用后芯片矩形不再响应鼠标点击
    m_bondEditable = editable;
    for (BondPointRectItem *item : m_lstBondItems)
        item->setEnabled(editable);
}

void WaferGraphicsView::SetSingleSelectMode(bool single)
{
    // 切换单选/多选模式：单选时若已有多个选中，只保留第一个
    m_singleSelect = single;
    if (!m_singleSelect || m_lstSelectedPoints.size() <= 1)
        return;

    int kept = 0;
    for (BondPointRectItem *item : m_lstBondItems)
    {
        if (item->IsSelected())
        {
            if (kept == 0)
                kept++;
            else
                item->SetSelected(false);
        }
    }
    m_lstSelectedPoints.clear();
    m_nSelectedCount = 0;
    for (BondPointRectItem *item : m_lstBondItems)
    {
        if (item->IsSelected())
        {
            m_lstSelectedPoints.append(item->GetNominalPos());
            m_nSelectedCount = 1;
            break;
        }
    }
    emit sig_selectedCountChanged(m_nSelectedCount);
    update();
}

void WaferGraphicsView::SelectAllBondPoints()
{
    // 单选模式：“全选”退化为只选中第一个
    if (m_singleSelect)
    {
        return;
    }

    // 全选：所有键合点位设为选中并同步选中列表/数量
    m_lstSelectedPoints.clear();
    for (BondPointRectItem *item : m_lstBondItems)
    {
        item->SetSelected(true);
        m_lstSelectedPoints.append(item->GetNominalPos());
    }
    m_nSelectedCount = m_lstBondItems.size();
    emit sig_selectedCountChanged(m_nSelectedCount);
    update();
}

void WaferGraphicsView::ClearAllBondPoints()
{
    // 清空：所有键合点位取消选中并同步选中列表/数量
    for (BondPointRectItem *item : m_lstBondItems)
        item->SetSelected(false);
    m_lstSelectedPoints.clear();
    m_nSelectedCount = 0;
    emit sig_selectedCountChanged(m_nSelectedCount);
    update();
}

//第 row 行（表 table）的标记应该在名义坐标nominalPos处、画成什么样markType
void WaferGraphicsView::SetLevelingPoint(int row, const QPointF &nominalPos, const QString &markType, int table, bool locked)
{
    const std::pair<int, int> key = std::make_pair(row, table);
    auto it = m_lvlMarks.find(key);
    if (it != m_lvlMarks.end() && it->locked)
        return;   // 已锁定（Confirm 后）的标记不可再被移动/改类型

    // 数据源始终为真相：scene 清空后可据此重建
    LvlMarkData data;
    data.nominalPos = nominalPos;
    data.markType = markType;
    data.locked = locked || (it != m_lvlMarks.end() && it->locked);
    m_lvlMarks.insert(key, data);

    LevelingMarkItem *item = m_lvlMarkItems.value(key);
    if (item)
    {
        // 已有图元：原位更新（不重建，避免打断拖动/选中）
        item->setPos(nominalPos);   // 名义坐标即场景坐标（视图变换里 y 取负呈现）
        item->setMarkType(markType);
        item->setLocked(data.locked);
        item->setEnabled(m_lvlEditable);
    }
    else
    {
        item = new LevelingMarkItem(row, nominalPos, markType, table,
                                    m_lvlMarkChipWidth, m_lvlMarkChipHeight, data.locked);
        item->setZValue(1500);   // 与 gui 一致：标记点位于芯片之上、坐标轴刻度之下
        item->setEnabled(m_lvlEditable);
        connect(item, &LevelingMarkItem::sig_moved, this, &WaferGraphicsView::onLevelingMarkMoved);
        connect(item, &LevelingMarkItem::sig_clicked, this, &WaferGraphicsView::onLevelingMarkClicked);
        m_pScene->addItem(item);
        m_lvlMarkItems.insert(key, item);
    }
    update();
}

void WaferGraphicsView::SetLevelingPointLocked(int row, int table)
{
    const std::pair<int, int> key = std::make_pair(row, table);
    auto it = m_lvlMarks.find(key);
    if (it == m_lvlMarks.end())
        return;
    it->locked = true;
    if (LevelingMarkItem *item = m_lvlMarkItems.value(key))
        item->setLocked(true);
    update();
}

void WaferGraphicsView::SetLevelingPickMode(bool on) // 选点态：只允许点击候选点（虚色圆点）
{
    m_lvlPickActive = on;
    for (LevelingMarkItem *item : m_lvlCandidateItems)
        item->setEnabled(on);   // 候选点仅在选点态下可点
    setCursor(on ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

// ---- Chip 标定候选点层 ----

void WaferGraphicsView::SetLevelingCandidates(const QList<QPointF> &nominalPoints, int table,
                                              const QList<int> &usedIndices)
{
    // 数据源始终为真相：scene 清空后可据此重建
    m_lvlCandidateTable = table;
    m_lvlCandidates = nominalPoints;
    m_lvlCandidateUsed = usedIndices;
    m_lvlCandidateSelected = -1;
    rebuildCandidateItems();
}

// ---- 便利重载：处方数据层 S_Point2D → Qt 类型 ----

void WaferGraphicsView::SetLevelingPoint(int row, const S_Point2D &nominalPos, const QString &markType,
                                         int table, bool locked)
{
    SetLevelingPoint(row, QPointF(nominalPos.x_, nominalPos.y_), markType, table, locked);
}

void WaferGraphicsView::SetLevelingCandidates(const std::vector<S_Point2D> &nominalPoints, int table,
                                              const QList<int> &usedIndices)
{
    QList<QPointF> points;
    points.reserve(static_cast<int>(nominalPoints.size()));
    for (const S_Point2D &p : nominalPoints)
        points.append(QPointF(p.x_, p.y_));
    SetLevelingCandidates(points, table, usedIndices);
}

void WaferGraphicsView::ClearLevelingCandidates(int table)
{
    if (table != m_lvlCandidateTable)
        return;
    // 立即从场景移除并释放图元，同时清空数据源与选中态
    for (LevelingMarkItem *item : m_lvlCandidateItems)
    {
        if (item->scene() == m_pScene)
            m_pScene->removeItem(item);
        delete item;
    }
    m_lvlCandidateItems.clear();
    m_lvlCandidates.clear();
    m_lvlCandidateUsed.clear();
    m_lvlCandidateSelected = -1;
    update();
}

void WaferGraphicsView::SetLevelingCandidateSelected(int index, int table)
{
    if (table != m_lvlCandidateTable)
        return;
    m_lvlCandidateSelected = index;
    for (LevelingMarkItem *item : m_lvlCandidateItems)
        item->setDimmed(item->row() != index);   // 选中的转实色，其余保持虚色
    update();
}

void WaferGraphicsView::rebuildCandidateItems()
{
    // 释放旧图元：仍在场景中的先移除再删除
    for (LevelingMarkItem *item : m_lvlCandidateItems)
    {
        if (item->scene() == m_pScene)
            m_pScene->removeItem(item);
        delete item;
    }
    m_lvlCandidateItems.clear();

    for (int i = 0; i < m_lvlCandidates.size(); ++i)
    {
        if (m_lvlCandidateUsed.contains(i))
            continue;   // 已占用：该位置已有（或将有）实色标记，不再画候选点

        const QPointF &p = m_lvlCandidates[i];
        // 名义坐标即场景坐标（视图变换里 y 取负呈现）：与调平标记/芯片排布同一坐标系（晶圆中心为原点）
        LevelingMarkItem *item = new LevelingMarkItem(
            i, p, QStringLiteral("Calibration"), m_lvlCandidateTable,
            m_lvlMarkChipWidth, m_lvlMarkChipHeight, /*locked=*/false);
        item->setClickOnly(true);                      // 候选点：只点不拖
        item->setDimmed(i != m_lvlCandidateSelected);  // 未选中 → 虚色（半透明深蓝）
        item->setEnabled(m_lvlPickActive);             // 仅选点态下可点
        item->setZValue(1500);                         // 与调平标记同层（gui 同值）
        connect(item, &LevelingMarkItem::sig_candidateClicked,
                this, &WaferGraphicsView::sig_levelingCandidateClicked);
        m_pScene->addItem(item);
        m_lvlCandidateItems.append(item);
    }
    update();
}

void WaferGraphicsView::RemoveLevelingPoint(int row ,int table) // 保证剩余标记点的行号与表格行重新一一对应（表格删除行后调用）
{
    // 删除该行标记图元（连同标记点）
    if (LevelingMarkItem *item = m_lvlMarkItems.take(std::make_pair(row,table)))
    {
        m_pScene->removeItem(item);
        delete item;
    }
    m_lvlMarks.remove(std::make_pair(row,table));

    // 行号大于 row 的同表标记行号 -1，保证与表格行重新一一对应（其它表的行号不变）
    QMap<std::pair<int,int>, LvlMarkData> shifted;
    for (auto it = m_lvlMarks.constBegin(); it != m_lvlMarks.constEnd(); ++it)
    {
        const std::pair<int,int> key = it.key();
        if (key.second == table && key.first > row)
            shifted.insert(std::make_pair(key.first - 1, table), it.value());
        else
            shifted.insert(key, it.value());
    }
    m_lvlMarks = shifted;

    // 行号变更，重建全部图元刷新 row 绑定
    rebuildLevelingItems();
    hideMarkCoord();   // 标记已删除：撤掉图上留下的过期坐标标注
    update();
}

void WaferGraphicsView::RemoveLevelingPointExact(int row, int table) //删最后一行
{
    // 只删除该行（表格行仍存在）的标记，不影响其它行号
    if (LevelingMarkItem *item = m_lvlMarkItems.take(std::make_pair(row, table)))
    {
        m_pScene->removeItem(item);
        delete item;
    }
    m_lvlMarks.remove(std::make_pair(row, table));
    hideMarkCoord();   // 标记已删除：撤掉图上留下的过期坐标标注
    update();
}

void WaferGraphicsView::MoveLevelingPoint(int rowA, int rowB ,int table)  // 标记--行号
{
    if (rowA == rowB)
        return;
    if (!m_lvlMarks.contains(std::make_pair(rowA,table)) || !m_lvlMarks.contains(std::make_pair(rowB,table)))
        return;

    // 交换两行数据，行号互换
    LvlMarkData dataA = m_lvlMarks.take(std::make_pair(rowA,table));
    LvlMarkData dataB = m_lvlMarks.take(std::make_pair(rowB,table));
    m_lvlMarks.insert(std::make_pair(rowA,table), dataB);
    m_lvlMarks.insert(std::make_pair(rowB,table), dataA);

    rebuildLevelingItems();
    update();
}

void WaferGraphicsView::ClearLevelingPoints()
{
    // 立即从场景移除并释放图元，清空数据源
    for (LevelingMarkItem *item : m_lvlMarkItems)
    {
        m_pScene->removeItem(item);
        delete item;
    }
    m_lvlMarkItems.clear();
    m_lvlMarks.clear();
    hideMarkCoord();   // 标记已清空：撤掉图上留下的过期坐标标注
    update();
}

void WaferGraphicsView::SetLevelingEditable(bool editable)
{
    m_lvlEditable = editable;
    for (LevelingMarkItem *item : m_lvlMarkItems)
        item->setEnabled(editable);
}

QPointF WaferGraphicsView::GetLevelingPoint(int row, int table) const // 获取第 row 行的标记名义坐标
{
    auto it = m_lvlMarks.constFind(std::make_pair(row,table));
    return it == m_lvlMarks.constEnd() ? QPointF() : it.value().nominalPos;
}

void WaferGraphicsView::SetLevelingSelectedRow(int row, int table) // 高亮第 row 行对应标记点（描边）
{
    for (auto it = m_lvlMarkItems.constBegin(); it != m_lvlMarkItems.constEnd(); ++it)
        it.value()->setMarkSelected(it.key().first == row && it.key().second == table);
    update();
}

void WaferGraphicsView::rebuildLevelingItems()
{
    // 释放旧图元：仍在场景中的先移除再删除（scene->clear() 已删除的图元不在此路径）
    for (LevelingMarkItem *item : m_lvlMarkItems)
    {
        if (item->scene() == m_pScene)
            m_pScene->removeItem(item);
        delete item;
    }
    m_lvlMarkItems.clear();

    for (auto it = m_lvlMarks.constBegin(); it != m_lvlMarks.constEnd(); ++it)
    {
        const LvlMarkData &data = it.value();
        LevelingMarkItem *item = new LevelingMarkItem(
            it.key().first, data.nominalPos, data.markType, it.key().second,
            m_lvlMarkChipWidth, m_lvlMarkChipHeight, data.locked);
        item->setZValue(1500);   // 与 gui 一致：标记点位于芯片之上、坐标轴刻度之下
        item->setEnabled(m_lvlEditable);
        connect(item, &LevelingMarkItem::sig_moved, this, &WaferGraphicsView::onLevelingMarkMoved);
        connect(item, &LevelingMarkItem::sig_clicked, this, &WaferGraphicsView::onLevelingMarkClicked);
        m_pScene->addItem(item);
        m_lvlMarkItems.insert(it.key(), item);
    }
    update();
}

void WaferGraphicsView::onLevelingMarkMoved(int row, int table, const QPointF &scenePos) // 调平标记拖动信号：回写数据源并通知外部
{
    // 场景坐标 == 名义坐标（视图变换里 y 取负呈现），无需再做符号翻转
    const QPointF nominal = scenePos;
    auto it = m_lvlMarks.find(std::make_pair(row,table));
    if (it != m_lvlMarks.end())
        it.value().nominalPos = nominal;
    // 坐标文字已显示时跟随标记，避免文字停在旧位置
    if (m_pCoordLabel)
        showMarkCoord(scenePos);
    emit sig_levelingPointMoved(row, table, nominal);
}

void WaferGraphicsView::onLevelingMarkClicked(int row, int table) // 调平标记点击信号：唯一高亮该标记
{
    // 与表格行选中共用同一套唯一高亮逻辑：清除其它标记，仅高亮被点击的这一个
    SetLevelingSelectedRow(row, table);
    emit sig_levelingPointClicked(row, table);

    // 坐标标注照抄 gui 的交互：点已标注的同一个标记 → 取消标注；点别的标记 → 只显示当前这一个
    const bool sameAsAnnotated = (m_pCoordLabel != nullptr && m_coordLabelRow == row && m_coordLabelTable == table);
    if (sameAsAnnotated)
    {
        hideMarkCoord();
        return;
    }

    if (LevelingMarkItem *item = m_lvlMarkItems.value(std::make_pair(row, table)))
    {
        m_coordLabelRow = row;
        m_coordLabelTable = table;
        showMarkCoord(item->pos());
    }
}

void WaferGraphicsView::showMarkCoord(const QPointF &scenePos) // 显示/更新该点的名义坐标标注（gui 样式）
{
    // 场景坐标 == 名义坐标，保留 3 位小数
    const QString text = QStringLiteral("(%1, %2)")
        .arg(QString::number(scenePos.x(), 'f', 3),
             QString::number(scenePos.y(), 'f', 3));

    if (!m_pCoordLabel)
    {
        m_pCoordLabel = new QGraphicsTextItem();
        // 视图变换里 y 取负（名义 y 向上），文字类图元必须忽略变换，否则会上下镜像
        m_pCoordLabel->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        m_pCoordLabel->setZValue(1600);   // 本项目 Z 分割：业务图元 100 起，标注在其上
        m_pScene->addItem(m_pCoordLabel);
    }
    // gui 的标注样式：浅黄底光晕衬底 + 深色字（换行按两行显示）
    m_pCoordLabel->setHtml(QString("<span style='background-color:rgba(255,255,240,215);'>%1</span>")
                               .arg(text.toHtmlEscaped().replace('\n', "<br>")));
    m_pCoordLabel->setDefaultTextColor(QColor(20, 20, 20));
    updateCoordLabelFont();
    // 位置照抄 gui 的首选方向（kLabelDirs[0] = NE）：
    // 忽略变换后局部坐标 = 设备像素，故以点位为锚、用局部平移把文本左上角放到右上 7px/-6px 处
    m_pCoordLabel->setPos(scenePos);
    m_pCoordLabel->setTransform(QTransform::fromTranslate(7.0, -6.0));
    update();
}

void WaferGraphicsView::hideMarkCoord() // 取消坐标标注
{
    m_coordLabelRow = -1;
    m_coordLabelTable = -1;
    if (m_pCoordLabel)
    {
        m_pScene->removeItem(m_pCoordLabel);
        delete m_pCoordLabel;
        m_pCoordLabel = nullptr;
    }
    update();
}

void WaferGraphicsView::updateCoordLabelFont() // 标注字号随缩放（gui 的 applyMarkFont 规则）
{
    if (!m_pCoordLabel)
        return;

    if (m_markFontBasePx <= 0)
    {
        // 基准字号从默认字体解析（点值按 96dpi 折像素；一般 9pt → 12px）
        m_markFontBase = m_pCoordLabel->font();
        const int ps = m_markFontBase.pixelSize();
        m_markFontBasePx = ps > 0 ? ps : qMax(1, qRound(m_markFontBase.pointSizeF() * 96.0 / 72.0));
    }

    // 相对"自适应视野"的放大倍率：缩小不更小（下限 = 基准字号），放大按对数阻尼
    const double scale = transform().m11();
    const double zoom = (m_fitScale > 0.0 && scale > 0.0) ? qMax(scale / m_fitScale, 1.0) : 1.0;
    const int px = qRound(m_markFontBasePx * (1.0 + 0.3 * std::log2(zoom)));
    if (m_pCoordLabel->font().pixelSize() == px)
        return;
    QFont f = m_markFontBase;
    f.setPixelSize(px);
    m_pCoordLabel->setFont(f);
}

void WaferGraphicsView::fitToChips(const QRectF &bounds) // 自适应缩放（fit-to-view）
{
    // 空数据时退回原点居中；否则交给引擎的 fitWafer：按内容（底图 + 芯片 + 符号）留 3% 边距，
    // 并让出 HUD 轴带占用的视口（左 52px / 下 34px），与 gui 的取景规则一致
    if (bounds.isNull())
    {
        CenterOnOrigin(); // 视图居中到场景原点
        return;
    }

    fitWafer();
    // 记下本次自适应的比例：标注字号随缩放的对数阻尼需要一个"1 倍"基准（gui 用相对 fitWafer 的倍率）
    if (transform().m11() > 0.0)
        m_fitScale = transform().m11();
    update();
}

void WaferGraphicsView::wheelEvent(QWheelEvent *event)
{
    // 滚轮缩放照抄 gui（引擎实现）：每步 1.15 倍、以光标为中心、限幅相对 fitWafer 视野 [0.2, 20]
    m_bIsInitialFit = false;          // 用户已手动缩放：之后的 resize 不再强制自适应
    DiagramBase::wheelEvent(event);
    updateCoordLabelFont();           // 字号随缩放（下限 = 基准字号，放大对数阻尼）
}

void WaferGraphicsView::keyPressEvent(QKeyEvent *event)
{
    // 方向键平移视图
    int step = 20;
    switch (event->key())
    {
        case Qt::Key_Up:
            verticalScrollBar()->setValue(verticalScrollBar()->value() - step);
            break;
        case Qt::Key_Down:
            verticalScrollBar()->setValue(verticalScrollBar()->value() + step);
            break;
        case Qt::Key_Left:
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - step);
            break;
        case Qt::Key_Right:
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() + step);
            break;
        default:
            QGraphicsView::keyPressEvent(event);
    }
}

void WaferGraphicsView::mousePressEvent(QMouseEvent *event)
{
    // 左键路由规则（本项目功能优先）：
    //   1) 选点态：只放行候选点（虚色圆点）的点击，其余位置一律吞掉，不允许自由放置点位；
    //   2) 命中本项目的可交互图元（调平标记/候选点/键合矩形，且已启用）→ 交给 QGraphicsView，
    //      由图元自己处理拖动/选中；
    //   3) 其余（空白、坐标轴带、非编辑态图元）→ 交给引擎 DiagramBase：左键按住平移视图。
    if (event->button() == Qt::LeftButton)
    {
        const QList<QGraphicsItem *> hits =
            m_pScene->items(mapToScene(event->pos()), Qt::IntersectsItemShape, Qt::DescendingOrder, QTransform());

        if (m_lvlPickActive)
        {
            for (QGraphicsItem *g : hits)
            {
                auto *mark = dynamic_cast<LevelingMarkItem *>(g);
                if (mark && mark->clickOnly() && mark->isEnabled())   // 命中候选点：由候选点自行处理点击
                {
                    QGraphicsView::mousePressEvent(event);
                    return;
                }
            }
            event->accept();   // 其它位置：吞掉，不发出任何选点信号
            return;
        }

        for (QGraphicsItem *g : hits)
        {
            // QGraphicsItem 非 QObject，qobject_cast 不可用；这两类为多态类型，用 dynamic_cast
            const bool interactive = (dynamic_cast<LevelingMarkItem *>(g) != nullptr) ||
                                     (dynamic_cast<BondPointRectItem *>(g) != nullptr);
            if (interactive && g->isEnabled())
            {
                QGraphicsView::mousePressEvent(event);   // 图元优先：拖动/点击选中交给图元
                return;
            }
        }
    }

    DiagramBase::mousePressEvent(event);   // 空白处：引擎的左键平移（光标 OpenHand/ClosedHand）
    m_bIsInitialFit = false;               // 用户已手动平移：之后的 resize 不再强制自适应
}

void WaferGraphicsView::resizeEvent(QResizeEvent *event)
{
    // 首次显示时自适应缩放：优先按芯片包围盒放大，无数据时由引擎按默认内容取景
    //（都走 fitWafer，保证 y 向上的视图变换约定一致）
    QGraphicsView::resizeEvent(event);
    if (m_bIsInitialFit)
    {
        if (m_hasChipBounds)
            fitToChips(m_chipBounds);
        else
            fitWafer();
    }
}
