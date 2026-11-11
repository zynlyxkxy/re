#include "ChipCalibrationTab.h"

#include "ui_RecipeModule.h"
#include "ReData/S_Recipe.h"
#include "GraphicsView/ChipCalibrationController.h"   // 控制器已移入 GraphicsView 目录（与 WaferGraphicsView 同层）
#include "Point/PointCoordDelegate.h"

#include <QObject>
#include <QTableWidget>
#include <QAbstractItemView>
#include <utility>

ChipCalibrationTab::ChipCalibrationTab(Ui::RecipeModule *ui, QWidget *dialogParent,
                                       std::function<void()> onModified)
    : m_ui(ui)
    , m_dialogParent(dialogParent)
    , m_onModified(std::move(onModified))
{
    // Chip 标定点位表：x/y 列只允许输入数字，整行选中
    m_ui->tw_ChipCalibration->setItemDelegate(new PointCoordDelegate(m_ui->tw_ChipCalibration));
    m_ui->tw_ChipCalibration->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui->tw_ChipCalibration->setSelectionMode(QAbstractItemView::SingleSelection);
    // Chip 标定坐标只由“图上拾取”产生/回填，单元格始终只读（增删/移动仍由按钮完成）
    m_ui->tw_ChipCalibration->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Chip 标定交互控制器：Add→候选点选点→Confirm 状态机 + 表格坐标回填
    // （候选点由 setDownMarkPoints 在 chip 切换/排布重算后下发，构造时无需传入）
    m_controller = new ChipCalibrationController(
        m_ui->gv_preview_4, m_ui->tw_ChipCalibration,
        m_ui->btn_add_ChipCalibration, m_ui->btn_delete_ChipCalibration,
        m_ui->btn_upMove_ChipCalibration, m_ui->btn_dwMove_ChipCalibration,
        m_ui->btn_confirm_ChipCalibration, m_dialogParent);

    QObject::connect(m_controller, &ChipCalibrationController::dataModified,
                     [this]() { if (m_onModified) m_onModified(); });
}

void ChipCalibrationTab::setConnect()
{
    // 控制器在构造函数内自连（Add/Confirm/图上拾取），本页无额外连接
}

void ChipCalibrationTab::setEditable(bool editable)
{
    m_ui->tw_ChipCalibration->setEditTriggers(QAbstractItemView::NoEditTriggers);
    if (m_controller)
        m_controller->setUiEditable(editable);
}

void ChipCalibrationTab::load(const std::vector<S_NominalPoint> &points)
{
    if (m_controller)
        m_controller->loadPoints(points);
}

void ChipCalibrationTab::save(S_RecipeWaferChipCalibration &chipCal) const
{
    if (m_controller)
    {
        // 保存前丢弃仍在“放置态”的未确认点（空行/未确认标记不写入处方）
        m_controller->cancelPlacement();
        chipCal.lst_chipCalibrationPoints = m_controller->collectPoints();
        chipCal.lst_allChipMarkCalibrationPoints = m_controller->getAllMarkCandidates();
    }
}

void ChipCalibrationTab::resetOnLayoutChange()
{
    if (m_controller)
        m_controller->loadPoints(std::vector<S_NominalPoint>());
}

bool ChipCalibrationTab::isPlacing() const
{
    return m_controller ? m_controller->isPlacing() : false;
}

void ChipCalibrationTab::setDownMarkPoints(const std::vector<S_Point2D> &points)
{
    // 直接下发控制器：作为 Add 后图上显示的虚色候选点（本页不做缓存）
    if (m_controller)
        m_controller->setCandidatePoints(points);
}
