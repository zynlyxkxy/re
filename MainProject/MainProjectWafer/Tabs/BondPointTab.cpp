#include "BondPointTab.h"

#include "ui_RecipeModule.h"
#include "ReData/S_Recipe.h"
#include "GraphicsView/WaferGraphicsView.h"

#include <QPushButton>
#include <QObject>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <utility>

BondPointTab::BondPointTab(Ui::RecipeModule *ui, std::function<void()> onModified)
    : m_ui(ui)
    , m_onModified(std::move(onModified))
{
    // 单片 / 多片 互斥选择
    m_ui->ckb_singleBond->setAutoExclusive(true);
    m_ui->ckb_multiBond->setAutoExclusive(true);
}

void BondPointTab::setConnect()
{
    // 键合选点：Edit 后点击一次“编辑点位”即可编辑 gv_bond，直到退出编辑模式
    QObject::connect(m_ui->btn_editBondPoint, &QPushButton::clicked, [this]() {
        m_editActive = true;
        updateEditUiState();
    });
    // 点击“保存选点”后退出键合选点编辑状态
    QObject::connect(m_ui->btn_saveBondPoint, &QPushButton::clicked, [this]() {
        m_editActive = false;
        updateEditUiState();
    });

    // 全选/清空键合点位
    QObject::connect(m_ui->btn_selectAllBondPoint, &QPushButton::clicked, [this]() {
        m_ui->gv_bond->SelectAllBondPoints();
        m_onModified();
    });
    QObject::connect(m_ui->btn_clearAllBondPoint, &QPushButton::clicked, [this]() {
        m_ui->gv_bond->ClearAllBondPoints();
        m_onModified();
    });

    // 键合选中数量变化 → 更新“已选点数量”输入框
    QObject::connect(m_ui->gv_bond, &WaferGraphicsView::sig_selectedCountChanged,
                     [this](int count) { m_ui->dsb_selectPointNum->setValue(count); });

    // 单片/多片切换 → 同步 gv_bond 的单选/多选模式
    QObject::connect(m_ui->ckb_singleBond, &QCheckBox::toggled, [this]() { switchBondSelectMode(); });
    QObject::connect(m_ui->ckb_multiBond, &QCheckBox::toggled, [this]() { switchBondSelectMode(); });
}

void BondPointTab::setEditable(bool editable)
{
    m_uiEditable = editable;
    if (!m_uiEditable)
        m_editActive = false;   // 退出编辑模式时复位“编辑点位”状态
    updateEditUiState();
}

void BondPointTab::updateEditUiState()
{
    // gv_bond 及 全选/清空 仅在 Edit 且点击过“编辑点位”后才可编辑
    const bool active = m_uiEditable && m_editActive;
    m_ui->gv_bond->SetBondEditable(active);
    m_ui->btn_selectAllBondPoint->setEnabled(active);
    m_ui->btn_clearAllBondPoint->setEnabled(active);
}

void BondPointTab::switchBondSelectMode()
{
    m_ui->gv_bond->SetSingleSelectMode(m_ui->ckb_singleBond->isChecked());
}

void BondPointTab::load(const S_RecipeWaferBond &bond, const QList<QPointF> &chipPositions,
                        double chipWidth, double chipHeight)
{
    m_lastBondMode = bond.bondMode;
    m_ui->ckb_singleBond->setChecked(bond.bondMode == 0);
    m_ui->ckb_multiBond->setChecked(bond.bondMode == 1);

    // 键合选点：绘制可点击选中的芯片矩形，并按已保存的键合点初始化选中状态
    QList<QPointF> bondPoints;
    for (const S_Point2D &p : bond.lst_bondNominalPoint)
    {
        bondPoints.append(QPointF(p.x_, p.y_));
    }
    m_ui->gv_bond->DisplayBondPoints(chipPositions, chipWidth, chipHeight, bondPoints);
    switchBondSelectMode();
}

void BondPointTab::save(S_RecipeWaferBond &bond) const
{
    // 未改动（两个都不选）时沿用加载的原值
    bond.bondMode = m_ui->ckb_singleBond->isChecked()  ? 0
                  : m_ui->ckb_multiBond->isChecked() ? 1
                                                       : m_lastBondMode;

    // 读回当前选中的键合点（名义坐标）
    bond.lst_bondNominalPoint.clear();
    const QList<QPointF> selectedPoints = m_ui->gv_bond->GetSelectedBondPoints();
    for (const QPointF &p : selectedPoints)
    {
        bond.lst_bondNominalPoint.push_back({p.x(), p.y()});
    }
}

void BondPointTab::resetOnLayoutChange(const QList<QPointF> &chipPositions,
                                       double chipWidth, double chipHeight)
{
    // 排布点位重算后，旧键合点（绝对坐标）失效：清空重选
    m_ui->gv_bond->ClearAllBondPoints();
    QList<QPointF> bondPoints;
    m_ui->gv_bond->DisplayBondPoints(chipPositions, chipWidth, chipHeight, bondPoints);
}
