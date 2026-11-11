#include "RecipeModule.h"
#include "ui_RecipeModule.h"
#include "istar_ui_stylesheet.h"
#include "istar_language.h"

#include <QPushButton>
#include <QObject>
#include <QMessageBox>
#include <QLabel>
#include <QRadioButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QBrush>
#include <QColor>
#include "groupbox/groupbox.h"

#include "rm_guiclient.h"
#include "rm_client.h"
#include "rm_define.h"
#include "istar_rpc_protocol_types.h"
#include "istar_serializer.hpp"
#include "dialog/messagedialog.h"

#include "GraphicsView/WaferGraphicsView.h"
#include "Algorithm/EAGAlgorithm/EgaFit.h"

#include "Tabs/MaterialTab.h"
#include "Tabs/WaferLevelTab.h"
#include "Tabs/ChuckLevelTab.h"
#include "Tabs/ChipCalibrationTab.h"
#include "Tabs/BondPointTab.h"
#include "Tabs/OperationModeTab.h"
#include "Tabs/ContinuousTab.h"

using namespace ISTAR::GUI;
using namespace ISTAR::UIStyle;

#define TR_STR(msg)  QString::fromStdString(Tr(msg))
#define TR_STRQ(msg) QString::fromStdString(Tr((msg).toStdString()))

// ══════════════ 参照平台 View.cpp + OperateBoard.cpp 结构 ══════════════

RecipeModule::RecipeModule()
{
    m_mainWidget = new QWidget();
    ui = new Ui::RecipeModule;
    ui->setupUi(m_mainWidget);

    // 左边栏 GroupBox — 标题/装饰条/平台样式（参照 OperateBoard.cpp）
    ui->sideGroup->SetDecorateColor("transparent");
    ui->sideGroup->SetHeaderText("");
    ui->sideGroup->setStyleSheet(iStarUIStyleSheet->GroupBoxStyle);

    QList<QPushButton*> lst_btn = m_mainWidget->findChildren<QPushButton*>();
    for(auto &b : lst_btn)
    {
        b->setStyleSheet(iStarUIStyleSheet->Button_Border);
    }

    // 按钮样式 — 参照 OperateBoard.cpp
    QString btnStyle = QString(
        "QPushButton{background-color:#FFFFFF;color:#808080;border-radius:5px;}"
        "QPushButton:hover{border:2px solid #A3B6FF;color:#A3B6FF;}"
        "QPushButton:pressed{border:2px solid #4C70FA;color:#4C70FA;}"
        "QPushButton:checked{border:2px solid #4C70FA;color:#4C70FA;}"
        "QPushButton:disabled{color:#E0E0E0;}")
        + iStarUIStyleSheet->fontStyle_bold_12;
    ui->btnSave->setStyleSheet(btnStyle);
    ui->btnEdit->setStyleSheet(btnStyle);

    // 右侧 QTabWidget 样式（结构已在 .ui 中定义）
    ui->tabWidget->setStyleSheet(
        iStarUIStyleSheet->TabWidget + iStarUIStyleSheet->fontStyle_normal_12
                + QStringLiteral("QTabBar::tab{font-size:10pt;}"));

    // 分组框统一样式 + 标题
    QList<GroupBox*> lst_gb = m_mainWidget->findChildren<GroupBox*>();
    for (auto &i : lst_gb)
    {
        i->setStyleSheet(iStarUIStyleSheet->GroupBoxStyle + iStarUIStyleSheet->DoubleSpinBox_Dark);
        i->SetheaderFontSize(16);
    }

    // 输入控件样式（通用，跨页签）
    QList<QDoubleSpinBox*> lst_dsb = m_mainWidget->findChildren<QDoubleSpinBox*>();
    for (auto &dsb : lst_dsb)
        dsb->setStyleSheet(iStarUIStyleSheet->DoubleSpinBox_Dark);

    QList<QCheckBox*> lst_cb = m_mainWidget->findChildren<QCheckBox*>();
    for (auto &cb : lst_cb)
        cb->setStyleSheet(iStarUIStyleSheet->checkbox);

    QList<QComboBox*> lst_ComboBox = m_mainWidget->findChildren<QComboBox*>();
    for (auto &cb : lst_ComboBox)
        cb->setStyleSheet(iStarUIStyleSheet->ComboBox);

    QList<QLineEdit*> lineEdits = m_mainWidget->findChildren<QLineEdit*>();
    for (auto *w : lineEdits)
    {
        w->setStyleSheet(iStarUIStyleSheet->LineEdit_Dark);
    }


    // 页签控制器：各页签自己的控件配置/连接/数据读写；互斥勾选、点表配置等见各自 Tabs/*.cpp
    m_materialTab = new MaterialTab(ui, m_mainWidget,
        [this]() { applyLayoutParams(); },                        // “预览”按钮
        [this]() { if (!m_loading) { regenerateLayout(); markDataModified(); } });  // chip 处方切换
    m_levelTab   = new WaferLevelTab(ui, [this]() { markDataModified(); });
    m_chuckTab   = new ChuckLevelTab(ui, [this]() { markDataModified(); });
    m_chipCalTab = new ChipCalibrationTab(ui, m_mainWidget,
                                          [this]() { markDataModified(); });
    m_bondTab    = new BondPointTab(ui, [this]() { markDataModified(); });
    m_opModeTab  = new OperationModeTab(ui, [this]() { markDataModified(); });
    m_continuousTab = new ContinuousTab(ui, [this]() { markDataModified(); });

    m_materialTab->setConnect();
    m_levelTab->setConnect();
    m_chuckTab->setConnect();
    m_chipCalTab->setConnect();
    m_bondTab->setConnect();
    m_opModeTab->setConnect();
    m_continuousTab->setConnect();

    // Chip/Wafer 处方同步：缓存 chip 规格 + 已保存 wafer 结构体，5s 轮询 chip 更新并自动跟随
    m_sync = new ChipWaferSync();
    m_sync->onChipListChanged = [this]() { rebuildChipCombo(); };
    m_sync->onCurrentWaferUpdated = [this](const S_RecipeContentWafer &recipe) {
        // 当前查看的 wafer 被自动更新：刷新 UI，保存交给用户
        UpdateUI(recipe);
        markDataModified();
    };
    m_sync->start();

    setEditable(false);
    setConnections();
    refreshLanguage();
}

RecipeModule::~RecipeModule()
{
    if (m_sync)
    {
        m_sync->stop();
        delete m_sync;
        m_sync = nullptr;
    }
    delete m_materialTab;
    delete m_levelTab;
    delete m_chuckTab;
    delete m_chipCalTab;
    delete m_bondTab;
    delete m_opModeTab;
    delete m_continuousTab;
    m_materialTab = nullptr;
    m_levelTab = nullptr;
    m_chuckTab = nullptr;
    m_chipCalTab = nullptr;
    m_bondTab = nullptr;
    m_opModeTab = nullptr;
    m_continuousTab = nullptr;
    delete ui;
    delete m_mainWidget;
}

void RecipeModule::refreshLanguage()
{
    applyLanguage();
}

void RecipeModule::applyLanguage()
{
    // .ui 静态文本在 setupUi 后即为英文 key；首次调用捕获 key，
    // 之后（含语言切换）统一按 TR_STR(key) 重放，实现中/英即时切换。
    if (!m_langReady)
    {
        const QStringList dynamicCombos = { QStringLiteral("cb_chipRecipe") };  // 处方名列表动态生成，不参与翻译
        const QStringList dynamicBtns = { QStringLiteral("btnEdit"), QStringLiteral("btnSave") };

        auto addSimple = [this](QWidget *w) {
            if (!w)
            {
                return;
            }
            const QString t = w->property("text").toString();
            if (!t.isEmpty())
            {
                m_langWidgets.append({w, t});
            }
        };
        for (QLabel *w : m_mainWidget->findChildren<QLabel *>())
        {
            if (!dynamicBtns.contains(w->objectName()))
            {
                addSimple(w);
            }
        }

        for (QPushButton *w : m_mainWidget->findChildren<QPushButton *>())
        {
            if (!dynamicBtns.contains(w->objectName()))
            {
                addSimple(w);
            }
        }

        for (QCheckBox *w : m_mainWidget->findChildren<QCheckBox *>())
        {
            addSimple(w);
        }

        for (QRadioButton *w : m_mainWidget->findChildren<QRadioButton *>())
        {
            addSimple(w);
        }

        for (QTabWidget *tw : m_mainWidget->findChildren<QTabWidget *>())
        {
            for (int i = 0; i < tw->count(); ++i)
            {
                m_langTabs.append({tw, i, tw->tabText(i)});
            }
        }



        for (QComboBox *cb : m_mainWidget->findChildren<QComboBox *>())
        {
            if (dynamicCombos.contains(cb->objectName()))
            {
                continue;
            }
            for (int i = 0; i < cb->count(); ++i)
            {
                m_langCombos.append({cb, i, cb->itemText(i)});
            }
        }

        for (QTableWidget *tw : m_mainWidget->findChildren<QTableWidget *>())
        {
            for (int c = 0; c < tw->columnCount(); ++c)
            {
                if (QTableWidgetItem *h = tw->horizontalHeaderItem(c))
                {
                    m_langHeaders.append({tw, c, h->text()});
                }
            }
        }

        m_langReady = true;
    }

    for (const auto &e : m_langWidgets)
    {
        if (e.first)
        {
            e.first->setProperty("text", TR_STRQ(e.second));
        }
    }

    for (const auto &e : m_langTabs)
    {
        if (e.widget && e.index >= 0 && e.index < static_cast<QTabWidget *>(e.widget)->count())
        {
            static_cast<QTabWidget *>(e.widget)->setTabText(e.index, TR_STRQ(e.key));
        }
    }

    QList<QComboBox *> doneCombos;
    for (const auto &e : m_langCombos)
    {
        auto *cb = static_cast<QComboBox *>(e.widget);
        if (!cb || doneCombos.contains(cb))
        {
            continue;
        }
        doneCombos.append(cb);
        QStringList keys;
        for (const auto &ee : m_langCombos)
        {
            if (ee.widget == cb)
            {
                keys.append(ee.key);
            }
        }
        const int cur = cb->currentIndex();
        cb->clear();
        for (const QString &k : keys)
        {
            cb->addItem(TR_STRQ(k));
        }
        if (cur >= 0 && cur < cb->count())
        {
            cb->setCurrentIndex(cur);
        }
    }

    for (const auto &e : m_langHeaders)
    {
        auto *tw = static_cast<QTableWidget *>(e.widget);
        if (tw && e.index < tw->columnCount())
        {
            tw->setHorizontalHeaderItem(e.index, new QTableWidgetItem(TR_STRQ(e.key)));
        }
    }

    // 自定义分组标题（ISTAR GroupBox 标题不走 QWidget::text，单独重放）
    ui->gb_dwWaferInfo->SetHeaderText(TR_STR("Single Chip Edit"));
    ui->gb_bondPointSetting->SetHeaderText(TR_STR("Bond Point Settings"));
    ui->gb_waferLevelingPoints->SetHeaderText(TR_STR("Wafer Leveling Points"));
    ui->gb_upChuckAlign->SetHeaderText(TR_STR("Up Chuck Leveling Points"));
    ui->gb_downChuckAlign->SetHeaderText(TR_STR("Down Chuck Leveling Points"));
    ui->gb_chuckevelResultToleranceRange->SetHeaderText(TR_STR("Chuck Leveling Result Tolerance Range"));
    ui->gb_ChipCalibration->SetHeaderText(TR_STR("Chip Calibration Points"));
    ui->gb_bondRecognitionTemplate->SetHeaderText(TR_STR("Bond Before/After Recognition Template"));
    ui->gb_precisionDetectSetting->SetHeaderText(TR_STR("Precision Detection Settings"));
    ui->gb_markOffset->SetHeaderText(TR_STR("Sub Mark and Main Mark Offset"));

    ui->btnEdit->setText(m_editable ? TR_STR("Cancel") : TR_STR("Edit"));
    ui->btnSave->setText(TR_STR("Save"));
}

QWidget* RecipeModule::getWidget()
{
    return m_mainWidget;
}

void RecipeModule::setCurrentRecipe(const QString& name)
{
    m_currentRecipe = name;
    m_dataModified = false;
    if (m_sync)
        m_sync->setCurrentWafer(name.toStdString());

    auto client = RM_GUIClient::GetInstance().GetRMClient("rm2");
    if (!client)
    {
        return;
    }

    StringList rcpNameList;
    rcpNameList.items.push_back(name.toStdString());

    RcpList rcpList;
    if (client->RMGetRcp(rcpNameList, rcpList) == ISTAR_RESULT_SUCCESS && !rcpList.items.empty())
    {
        std::string bodyJson = rcpList.items[0].body;
        ui->btnEdit->setEnabled(!rcpList.items[0].header.released);

        // body 解析失败（空/旧结构/非 wafer 处方）时回退到空处方，避免 UpdateUI 被跳过、
        // 界面残留上一个处方的状态（如多片勾选）
        S_RecipeContentWafer recipeStruct;
        try
        {
            recipeStruct = Serializer::from_rawJson<S_RecipeContentWafer>(bodyJson);
        }
        catch (const std::exception &)
        {
            recipeStruct = S_RecipeContentWafer{};
        }

        UpdateUI(recipeStruct);
        setEditable(m_editable);
    }
}

// 用同步类的 chip 缓存重建下拉框（chip 列表/规格变化时调用）
void RecipeModule::rebuildChipCombo()
{
    if (!m_sync || !m_materialTab)
        return;
    QStringList names;
    for (const auto &kv : m_sync->chips())
    {
        if (!kv.second.released)
            continue;   // 只列已发布的 chip 处方
        names << QString::fromStdString(kv.first);
    }
    const QString prefer = m_loadedChipName.empty() ? QString() : QString::fromStdString(m_loadedChipName);
    m_materialTab->setChipRecipeNames(names, prefer);
    refreshChipMarkPositions();
}

// combo 手动切换 chip：选中即写入编辑态快照（dims）+ 重算点位 + 清键合点/标定点，
// 与 chip 自动更新“替用户重新选择”的行为一致；保存由用户决定
void RecipeModule::regenerateLayout()
{
    applyLayoutParams();
}

// 按当前 UI 参数重算排布点位并刷新预览；键合点与 Chip 标定点失效清空。
// dims 以 combo 当前选中 chip 的缓存规格为准（加载快照可能为 0/过时，chip 缓存实时跟随），
// 快照仅作 combo 选不中时的兜底
void RecipeModule::applyLayoutParams()
{
    const QString chipName = m_materialTab ? m_materialTab->currentChipRecipeName() : QString();
    refreshChipMarkPositions();
    double w = 0.0, h = 0.0;
    if (!chipName.isEmpty() && m_sync && m_sync->chipDims(chipName.toStdString(), w, h))
    {
        m_chipWidth = w;
        m_chipHeight = h;
    }
    if (m_chipWidth <= 0 || m_chipHeight <= 0)
        return;
    double inch_to_mm = 25.4;
    double waferRadius = ui->dsb_waferSize->value() * inch_to_mm / 2 - ui->dsb_waferEdgeLength->value();
    EGA::MatrixXd chipsPos = EGA::create_chip_position(waferRadius,
                                                             m_chipWidth, m_chipHeight,
                                                             ui->dsb_chipPitchX->value(),
                                                             ui->dsb_chipPitchY->value(),
                                                             ui->cb_waferLayoutMode->currentIndex() == WaferLayoutMode::ChipCornerAndWaferCenterCoincide,
                                                             ui->cb_waferNotchOrient->currentText().toInt());

    m_chipPositions.clear();
    for (int i = 0; i < chipsPos.rows(); ++i)
    {
        S_Point2D p{chipsPos(i, 0), chipsPos(i, 1)};
        m_chipPositions.append(QPointF(p.x_, p.y_));
    }
    refreshPreview(waferRadius, ui->cb_waferNotchOrient->currentText().toInt());

    // 排布点位重算后，旧键合点（绝对坐标）失效：清空重选（BondPointTab）
    if (m_bondTab)
        m_bondTab->resetOnLayoutChange(m_chipPositions, m_chipWidth, m_chipHeight);

    if (m_chipCalTab)
    {
        m_chipCalTab->resetOnLayoutChange(); // 旧 Chip 标定点（绝对坐标）同样失效：清空表格与圆点（ChipCalibrationTab）
        m_chipCalTab->setDownMarkPoints(allChipMarkNominalPoints());
    }

}

void RecipeModule::onEditClicked()
{
    // 当前在编辑模式(文本为Cancel)时点击 = 取消编辑；有未保存修改则弹窗提醒
    if (m_editable && m_dataModified)
    {
        QMessageBox::warning(m_mainWidget, TR_STR("Notice"), TR_STR("Data has been modified and not saved yet"));
    }

    m_editable = !m_editable;
    setEditable(m_editable);
    ui->btnEdit->setText(m_editable ? TR_STR("Cancel") : TR_STR("Edit"));
}

void RecipeModule::onSaveClicked()
{
    if (!m_editable) return;

    S_RecipeContentWafer recipeStruct = GetUIInfo();
    std::string json = Serializer::to_rawJson<S_RecipeContentWafer>(recipeStruct);

    RcpBodySaveInfo info;
    info.rcpName = m_currentRecipe.toStdString();
    info.body = json;

    auto client = RM_GUIClient::GetInstance().GetRMClient("rm2");
    if (client)
    {
        if (client->RMSaveRcpBody(info) == ISTAR_RESULT_SUCCESS)
        {
            m_dataModified = false;
        }
    }

    setEditable(false);
    ui->btnEdit->setText(QStringLiteral("Edit"));
}

void RecipeModule::setEditable(bool editable)
{
    m_editable = editable;
    ui->btnSave->setEnabled(editable);

    // 所有tab里的输入控件随编辑模式切换（通用，跨页签）
    QList<QDoubleSpinBox*> spinBoxes = m_mainWidget->findChildren<QDoubleSpinBox*>();
    for (auto *w : spinBoxes)
        w->setEnabled(editable);

    QList<QComboBox*> comboBoxes = m_mainWidget->findChildren<QComboBox*>();
    for (auto *w : comboBoxes)
        w->setEnabled(editable);

    QList<QCheckBox*> checkBoxes = m_mainWidget->findChildren<QCheckBox*>();
    for (auto *w : checkBoxes)
        w->setEnabled(editable);

    QList<QLineEdit*> lineEdits = m_mainWidget->findChildren<QLineEdit*>();
    for (auto *w : lineEdits)
    {
        w->setEnabled(editable);
    }


    QList<QPushButton*> buttons = m_mainWidget->findChildren<QPushButton*>();
    for (auto *b : buttons)
    {
        if (b != ui->btnEdit)
            b->setEnabled(editable);
    }

    // 调平点位表：随编辑模式切换可编辑状态与置灰
    QList<QTableWidget*> tables = m_mainWidget->findChildren<QTableWidget*>();
    for (auto *tw : tables)
    {
        tw->setEditTriggers(editable
            ? QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
            : QAbstractItemView::NoEditTriggers);

        for (int r = 0; r < tw->rowCount(); ++r)
        {
            for (int c = 0; c < tw->columnCount(); ++c)
            {
                QTableWidgetItem *item = tw->item(r, c);
                if (item)
                {
                    item->setBackground(editable ? QBrush() : QColor("#E9E9EF"));
                }
            }
        }
    }

    // 页签级编辑态联动（各自视图可拖/按钮状态机等）
    if (m_levelTab)   m_levelTab->setEditable(editable);
    if (m_chuckTab)   m_chuckTab->setEditable(editable);
    if (m_chipCalTab) m_chipCalTab->setEditable(editable);
    if (m_bondTab)    m_bondTab->setEditable(editable);
}

// 刷新各预览视图（物料信息 / 调平 / 吸盘 / Chip 标定 共用同一芯片排布数据）
void RecipeModule::refreshPreview(double radius, int notchAngle)
{
    ui->gv_preview->SetWaferRadius(radius);
    ui->gv_preview->SetWaferNotchAngle(notchAngle);
    ui->gv_preview->DisplayChipLayout(m_chipPositions, m_chipWidth, m_chipHeight);

    ui->gv_preview_2->SetWaferRadius(radius);
    ui->gv_preview_2->SetWaferNotchAngle(notchAngle);
    ui->gv_preview_2->DisplayChipLayout(m_chipPositions, m_chipWidth, m_chipHeight);

    ui->gv_preview_3->SetWaferRadius(radius);
    ui->gv_preview_3->SetWaferNotchAngle(notchAngle);
    ui->gv_preview_3->DisplayChipLayout(m_chipPositions, m_chipWidth, m_chipHeight);

    ui->gv_preview_4->SetWaferRadius(radius);
    ui->gv_preview_4->SetWaferNotchAngle(notchAngle);
    ui->gv_preview_4->DisplayChipLayout(m_chipPositions, m_chipWidth, m_chipHeight);

}

// 将 recipe 数据填充到控件：先回填布局状态与物料页，再把各数据段分发给页签
void RecipeModule::UpdateUI(const S_RecipeContentWafer& recipe)
{
    m_loading = true;   // 填充控件期间抑制 combo 切换重算
    const S_RecipeContentWafer &wafer = recipe;
    const S_RecipeWaferMaterial &waferMat = wafer.material;
    const S_RecipeWaferMaterialOneChipTypeInfo &oneChip = wafer.material.oneChipTypeInfo;

    // 芯片名义坐标 → 排布状态（gv_preview 由“预览”按钮触发显示）
    QList<QPointF> chipPositions;
    for (const S_Point2D &p : oneChip.lst_nominalPoints)
    {
        chipPositions.append(QPointF(p.x_, p.y_));
    }
    m_chipPositions = chipPositions;
    m_chipWidth = oneChip.chipRecipe.chipWidth;
    m_chipHeight = oneChip.chipRecipe.chipHeight;
    m_loadedChipName = oneChip.chipRecipe.chipRecipeName;
    refreshChipMarkPositions();

    // 物料页（含 chip 处方下拉选中 + 未发布提示）
    m_materialTab->load(waferMat);

    const double inch_to_mm = 25.4;
    refreshPreview(oneChip.waferSize * inch_to_mm / 2 - oneChip.waferEdgeLength, oneChip.waferNotchOrient);

    // 各页签数据段
    m_levelTab->load(wafer.waferLevel);
    m_chuckTab->load(wafer.chuckLevel);

    m_chipCalTab->load(wafer.chipCalibration.lst_chipCalibrationPoints);

    m_chipCalTab->setDownMarkPoints(wafer.chipCalibration.lst_allChipMarkCalibrationPoints.empty()?
                                    allChipMarkNominalPoints():wafer.chipCalibration.lst_allChipMarkCalibrationPoints);

    m_bondTab->load(wafer.bondPointSelection, m_chipPositions, m_chipWidth, m_chipHeight);
    m_opModeTab->load(wafer.operationMode);
    m_continuousTab->load(wafer.continuous);

    m_loading = false;
}

// 读取控件数据到 recipe：逐页签收集 + 布局/chip 引用回填
S_RecipeContentWafer RecipeModule::GetUIInfo()
{
    S_RecipeContentWafer recipeStruct;

    m_materialTab->save(recipeStruct.material);

    // chipRecipe 引用：下拉框选中的 chip 处方名，die 尺寸以 chip 缓存为准（实时跟随 chip 更新），
    // 选不中（chip 未发布/已删除）时用布局快照兜底
    auto &oneChip = recipeStruct.material.oneChipTypeInfo;
    double chipW = 0.0, chipH = 0.0;
    if (!oneChip.chipRecipe.chipRecipeName.empty()
        && m_sync && m_sync->chipDims(oneChip.chipRecipe.chipRecipeName, chipW, chipH))
    {
        oneChip.chipRecipe.chipWidth = chipW;
        oneChip.chipRecipe.chipHeight = chipH;
    }
    else
    {
        oneChip.chipRecipe.chipWidth = m_chipWidth;
        oneChip.chipRecipe.chipHeight = m_chipHeight;
    }

    // 芯片名义坐标（当前排布状态）
    oneChip.lst_nominalPoints.clear();
    for (const QPointF &p : m_chipPositions)
    {
        oneChip.lst_nominalPoints.push_back({p.x(), p.y()});
    }

    m_levelTab->save(recipeStruct.waferLevel);
    m_chuckTab->save(recipeStruct.chuckLevel);
    m_chipCalTab->save(recipeStruct.chipCalibration);
    m_bondTab->save(recipeStruct.bondPointSelection);
    m_opModeTab->save(recipeStruct.operationMode);
    m_continuousTab->save(recipeStruct.continuous);

    return recipeStruct;
}

void RecipeModule::setConnections()
{
    // 编辑/保存（页签内按钮/表格/视图连接由各 Tab::setConnect() 完成）
    QObject::connect(ui->btnEdit, &QPushButton::clicked, [this]() { onEditClicked(); });
    QObject::connect(ui->btnSave, &QPushButton::clicked, [this]() { onSaveClicked(); });

    // 控件值变化 → 标记数据已修改（通用，跨页签）
    auto mark = [this]() { markDataModified(); };

    QList<QDoubleSpinBox*> spinBoxes = m_mainWidget->findChildren<QDoubleSpinBox*>();
    for (auto *w : spinBoxes)
        QObject::connect(w, QOverload<double>::of(&QDoubleSpinBox::valueChanged), mark);

    QList<QComboBox*> comboBoxes = m_mainWidget->findChildren<QComboBox*>();
    for (auto *w : comboBoxes)
        QObject::connect(w, QOverload<int>::of(&QComboBox::currentIndexChanged), mark);

    QList<QCheckBox*> checkBoxes = m_mainWidget->findChildren<QCheckBox*>();
    for (auto *w : checkBoxes)
        QObject::connect(w, &QCheckBox::toggled, mark);

    QList<QLineEdit*> lineEdits = m_mainWidget->findChildren<QLineEdit*>();
    for (auto *w : lineEdits)
        QObject::connect(w, &QLineEdit::textChanged, mark);

    QList<QTableWidget*> tables = m_mainWidget->findChildren<QTableWidget*>();
    for (auto *tw : tables)
        QObject::connect(tw, &QTableWidget::itemChanged, mark);
}

void RecipeModule::markDataModified()
{
    m_dataModified = true;
}

bool RecipeModule::queryChipMarkPositions(std::vector<S_Point2D> &out) const
{
    const QString chip = m_materialTab ? m_materialTab->currentChipRecipeName() : QString();
    return m_sync && !chip.isEmpty() && m_sync->chipAlignPoints(chip.toStdString(), out);
}

void RecipeModule::refreshChipMarkPositions()
{
    std::vector<S_Point2D> pts;
    if (queryChipMarkPositions(pts))
    {
        m_chipMarkPositions = std::move(pts);   // 含"chip 存在但没有点" → 赋成空，正确
    }
    else
    {
        m_chipMarkPositions.clear();            // 取不到：清掉上一颗 chip 的残留，绝不张冠李戴
    }
}

namespace {
std::vector<S_Point2D> toPoint2DVector(const QList<QPointF> &src)
{
    std::vector<S_Point2D> out;
    out.reserve(static_cast<size_t>(src.size()));
    for (const QPointF &p : src)
    {
        out.push_back(S_Point2D(p.x(), p.y()));
    }
    return out;
}
} // namespace

std::vector<S_Point2D> RecipeModule::allChipMarkNominalPoints() const
{
    if (m_chipPositions.isEmpty() || m_chipMarkPositions.empty())
        {
            return {};                       // 没布局 / 没标记点：没有"所有芯片的标记点"可言
        }

    const std::vector<S_Point2D> chipPos = toPoint2DVector(m_chipPositions);   // ← 这里转
    const S_Point2D &base = m_chipMarkPositions.front();
    const S_Point2D *nearest = &chipPos.front();
    double best = std::numeric_limits<double>::max();
    for (const S_Point2D &p : chipPos)
    {
        const double d = std::pow(p.x_ - base.x_, 2) + std::pow(p.y_ - base.y_, 2);
        if (d < best)
        {
            best = d;
            nearest = &p;
        }
    }

    std::vector<S_Point2D> offsets;
    offsets.reserve(m_chipMarkPositions.size());
    for (const auto &m : m_chipMarkPositions)
    {
        offsets.push_back(S_Point2D(m.x_ - nearest->x_, m.y_ - nearest->y_));
    }

    std::vector<S_Point2D> res;
    res.reserve(m_chipPositions.size() * offsets.size());
    for (const S_Point2D &p : chipPos)
    {
        for (const S_Point2D &o : offsets)
        {
            res.push_back(S_Point2D(p.x_ + o.x_, p.y_ + o.y_));
        }
    }
    return res;


}

extern "C" {

__declspec(dllexport) IREModule* CreateREModule()
{
    return new RecipeModule();
}

}
