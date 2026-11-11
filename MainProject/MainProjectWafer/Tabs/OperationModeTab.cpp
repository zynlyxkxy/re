#include "OperationModeTab.h"

#include "ui_RecipeModule.h"
#include "ReData/S_Recipe.h"

#include <QCheckBox>
#include <utility>

OperationModeTab::OperationModeTab(Ui::RecipeModule *ui, std::function<void()> onModified)
    : m_ui(ui)
    , m_onModified(std::move(onModified))
{
    // 精度/产率 互斥选择
    m_ui->ckb_precision->setAutoExclusive(true);
    m_ui->ckb_throughput->setAutoExclusive(true);
}

void OperationModeTab::setConnect()
{
    // 无页内按钮/联动；勾选变化 → 标记已修改由 RecipeModule 的通用循环统一处理
}

void OperationModeTab::load(const S_RecipeWaferOperationMode &opMode)
{
    m_lastOperationMode = opMode.operationMode;
    m_ui->ckb_precision->setChecked(opMode.operationMode == 0);
    m_ui->ckb_throughput->setChecked(opMode.operationMode == 1);
}

void OperationModeTab::save(S_RecipeWaferOperationMode &opMode) const
{
    // 未改动（两个都不选）时沿用加载的原值，避免把没有 operationMode 的处方默认存成 Precision
    opMode.operationMode = m_ui->ckb_precision->isChecked()  ? 0
                         : m_ui->ckb_throughput->isChecked() ? 1
                                                              : m_lastOperationMode;
}
