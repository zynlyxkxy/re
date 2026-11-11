#include "MaterialTab.h"

#include "ui_RecipeModule.h"
#include "ReData/S_Recipe.h"
#include "istar_ui_stylesheet.h"

#include <QStringList>

using namespace ISTAR::GUI;
using namespace ISTAR::UIStyle;

MaterialTab::MaterialTab(Ui::RecipeModule *ui, std::function<void()> onModified)
    : m_ui(ui)
    , m_onModified(std::move(onModified))
{
    // 晶圆尺寸/切边方向下拉框（首项空选项表示"未设置(0)"；尺寸下拉只出现 8 / 12 两个合法值）
    m_ui->cb_waferSize->setStyleSheet(iStarUIStyleSheet->ComboBox);
    m_ui->cb_waferSize->addItems(QStringList() << QStringLiteral("") << QStringLiteral("8") << QStringLiteral("12"));

    m_ui->cb_waferNotchOrient->setStyleSheet(iStarUIStyleSheet->ComboBox);
    m_ui->cb_waferNotchOrient->addItems(QStringList() << QStringLiteral("0") << QStringLiteral("90") << QStringLiteral("180") << QStringLiteral("270"));
}

void MaterialTab::setConnect()
{
    // 物料页无按钮/页内联动；控件值变化 → 标记已修改由 RecipeModule 的通用循环统一处理
}

void MaterialTab::load(const S_RecipeChipMaterial &material)
{
    m_ui->dsb_upChipWidth->setValue(material.upChipMaterialInfo.chipWidth);
    m_ui->dsb_upChipHeight->setValue(material.upChipMaterialInfo.chipHeight);
    m_ui->dsb_upChipThickness->setValue(material.upChipMaterialInfo.chipThickness);

    m_ui->dsb_dwChipWidth->setValue(material.dwChipMaterialInfo.chipWidth);
    m_ui->dsb_dwChipHeight->setValue(material.dwChipMaterialInfo.chipHeight);
    m_ui->dsb_bump->setValue(material.dwChipMaterialInfo.bump);

    const S_RecipeBlueWaferInfo &blueWafer = material.blueWaferInfo;
    // 处方值为 0(未设置) 时 setCurrentText 找不到项会残留上一个选中值，这里落到第0项(空选项)
    const int idx = m_ui->cb_waferSize->findText(QString::number(blueWafer.waferSize));
    m_ui->cb_waferSize->setCurrentIndex(idx < 0 ? 0 : idx);
    m_ui->cb_waferNotchOrient->setCurrentText(QString::number(blueWafer.waferNotchOrient));
    m_ui->dsb_expansionDepth->setValue(blueWafer.expansionDepth);
    m_ui->dsb_chipPitchX->setValue(blueWafer.xChipPitch);
    m_ui->dsb_chipPitchY->setValue(blueWafer.yChipPitch);
}

void MaterialTab::save(S_RecipeChipMaterial &material) const
{
    material.upChipMaterialInfo.chipWidth = m_ui->dsb_upChipWidth->value();
    material.upChipMaterialInfo.chipHeight = m_ui->dsb_upChipHeight->value();
    material.upChipMaterialInfo.chipThickness = m_ui->dsb_upChipThickness->value();

    material.dwChipMaterialInfo.chipWidth = m_ui->dsb_dwChipWidth->value();
    material.dwChipMaterialInfo.chipHeight = m_ui->dsb_dwChipHeight->value();
    material.dwChipMaterialInfo.bump = m_ui->dsb_bump->value();

    S_RecipeBlueWaferInfo &blueWafer = material.blueWaferInfo;
    blueWafer.waferSize = m_ui->cb_waferSize->currentText().toInt();
    blueWafer.waferNotchOrient = m_ui->cb_waferNotchOrient->currentText().toInt();
    blueWafer.expansionDepth = m_ui->dsb_expansionDepth->value();
    blueWafer.xChipPitch = m_ui->dsb_chipPitchX->value();
    blueWafer.yChipPitch = m_ui->dsb_chipPitchY->value();
}
