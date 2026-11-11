#include "MaterialTab.h"

#include "ui_RecipeModule.h"
#include "ReData/S_Recipe.h"
#include "istar_language.h"
#include "dialog/messagedialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QObject>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>

using namespace ISTAR::GUI;

#define TR_STR(msg)  QString::fromStdString(Tr(msg))

MaterialTab::MaterialTab(Ui::RecipeModule *ui, QWidget *dialogParent,
                         std::function<void()> onPreviewClicked,
                         std::function<void()> onChipRecipeChanged)
    : m_ui(ui)
    , m_dialogParent(dialogParent)
    , m_onPreviewClicked(std::move(onPreviewClicked))
    , m_onChipRecipeChanged(std::move(onChipRecipeChanged))
{
    // 一种芯片 / 自定义 互斥选择
    m_ui->ckb_oneChip->setAutoExclusive(true);
    m_ui->ckb_custom->setAutoExclusive(true);

    m_ui->cb_waferNotchOrient->addItems(QStringList()
                                        << QStringLiteral("0") << QStringLiteral("90")
                                        << QStringLiteral("180") << QStringLiteral("270"));
}

void MaterialTab::setConnect()
{
    // “预览”按钮：按当前 UI 参数重算排布并刷新预览（模块统一分发到各页签视图）
    QObject::connect(m_ui->btn_display, &QPushButton::clicked, [this]() {
        if (m_onPreviewClicked) m_onPreviewClicked();
    });

    // chip 处方下拉框手动切换：选中即重算排布（模块回调带 m_loading 抑制与 mark）
    QObject::connect(m_ui->cb_chipRecipe, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) {
        if (m_onChipRecipeChanged) m_onChipRecipeChanged();
    });
}

void MaterialTab::setChipRecipeNames(const QStringList &releasedNames, const QString &prefer)
{
    QSignalBlocker blocker(m_ui->cb_chipRecipe);
    const QString prevText = m_ui->cb_chipRecipe->currentText();
    m_ui->cb_chipRecipe->clear();
    for (const QString &name : releasedNames)
        m_ui->cb_chipRecipe->addItem(name);
    if (!prevText.isEmpty())
        m_ui->cb_chipRecipe->setCurrentText(prevText);
    else if (!prefer.isEmpty())
        m_ui->cb_chipRecipe->setCurrentText(prefer);
}

QString MaterialTab::currentChipRecipeName() const
{
    return m_ui->cb_chipRecipe->currentText();
}

void MaterialTab::load(const S_RecipeWaferMaterial &material)
{
    const S_RecipeWaferMaterialOneChipTypeInfo &oneChip = material.oneChipTypeInfo;

    m_lastMaterialEditMode = material.materialEditMode;
    m_ui->ckb_oneChip->setChecked(material.materialEditMode == 0);
    m_ui->ckb_custom->setChecked(material.materialEditMode == 1);
    m_ui->dsb_waferSize->setValue(oneChip.waferSize);
    m_ui->dsb_waferEdgeLength->setValue(oneChip.waferEdgeLength);
    m_ui->dsb_waferThickness->setValue(oneChip.waferThickness);
    m_ui->cb_waferNotchOrient->setCurrentText(QString::number(oneChip.waferNotchOrient));
    m_ui->dsb_chipPitchX->setValue(oneChip.xChipPitch);
    m_ui->dsb_chipPitchY->setValue(oneChip.yChipPitch);
    m_ui->cb_waferLayoutMode->setCurrentIndex(oneChip.waferLayoutMode);
    m_ui->cb_chipRecipe->setCurrentText(QString::fromStdString(oneChip.chipRecipe.chipRecipeName));

    // chip 处方不存在（未发布/已删除）提示
    QStringList texts;
    const int n = m_ui->cb_chipRecipe->count();
    for (int i = 0; i < n; ++i)
        texts << m_ui->cb_chipRecipe->itemText(i);
    if (!texts.contains(QString::fromStdString(oneChip.chipRecipe.chipRecipeName))
        && !m_ui->cb_chipRecipe->currentText().isEmpty())
    {
        MessageDialog dlg(MessageDialog::Attention, m_dialogParent,
                          TR_STR("The selected chip recipe is unreleased or has been deleted"));
        dlg.exec();
    }
}

void MaterialTab::save(S_RecipeWaferMaterial &material) const
{
    // 未改动（两个都不选）时沿用加载的原值
    material.materialEditMode = m_ui->ckb_oneChip->isChecked() ? 0
                              : m_ui->ckb_custom->isChecked()  ? 1
                                                                : m_lastMaterialEditMode;

    S_RecipeWaferMaterialOneChipTypeInfo &oneChip = material.oneChipTypeInfo;
    oneChip.waferSize = m_ui->dsb_waferSize->value();
    oneChip.waferEdgeLength = m_ui->dsb_waferEdgeLength->value();
    oneChip.waferThickness = m_ui->dsb_waferThickness->value();
    oneChip.waferNotchOrient = m_ui->cb_waferNotchOrient->currentText().toInt();
    oneChip.xChipPitch = m_ui->dsb_chipPitchX->value();
    oneChip.yChipPitch = m_ui->dsb_chipPitchY->value();
    oneChip.waferLayoutMode = m_ui->cb_waferLayoutMode->currentIndex();

    // chipRecipe 引用：名称来自下拉框；尺寸/名义坐标由模块在 save 后按布局状态与 chip 缓存回填
    oneChip.chipRecipe.chipRecipeName = m_ui->cb_chipRecipe->currentText().toStdString();
}
