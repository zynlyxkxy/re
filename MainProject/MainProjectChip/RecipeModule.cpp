#include "RecipeModule.h"
#include "ui_RecipeModule.h"
#include "istar_ui_stylesheet.h"
#include "istar_language.h"

#include <QPushButton>
#include <QObject>
#include <QLabel>
#include <QRadioButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QBrush>
#include <QColor>

#include "dialog/messagedialog.h"

#include "rm_guiclient.h"
#include "rm_client.h"
#include "rm_define.h"
#include "istar_rpc_protocol_types.h"
#include "istar_serializer.hpp"

#include "Tabs/MaterialTab.h"
#include "Tabs/LevelTab.h"
#include "Tabs/AlignTab.h"
#include "Tabs/BondingTab.h"

using namespace ISTAR;
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
    for(auto &i : lst_gb)
    {
        i->setStyleSheet(StyleSheet::getInstance()->GroupBoxStyle + StyleSheet::getInstance()->DoubleSpinBox_Dark);
        i->SetheaderFontSize(16);
    }

    // 页签控制器：各页签自己的控件配置/连接/数据读写（Add 等按钮连接在各自 setConnect() 中）
    m_materialTab = new MaterialTab(ui, [this]() { markDataModified(); });
    m_levelTab    = new LevelTab(ui, [this]() { markDataModified(); });
    m_alignTab    = new AlignTab(ui, [this]() { markDataModified(); });
    m_bondingTab  = new BondingTab(ui, m_mainWidget, [this]() { markDataModified(); });

    m_materialTab->setConnect();
    m_levelTab->setConnect();
    m_alignTab->setConnect();
    m_bondingTab->setConnect();

    QList<QCheckBox*> lst_checkBox = m_mainWidget->findChildren<QCheckBox*>();
    for(auto &cb : lst_checkBox)
    {
        cb->setStyleSheet(iStarUIStyleSheet->checkbox);
    }

    QList<QDoubleSpinBox*> lst_DoubleSpinBox = m_mainWidget->findChildren<QDoubleSpinBox*>();
    for(auto &dsb : lst_DoubleSpinBox)
    {
        dsb->setStyleSheet(iStarUIStyleSheet->DoubleSpinBox_Dark);
    }

    setEditable(false);
    setConnections();
    refreshLanguage();
}

RecipeModule::~RecipeModule()
{
    delete m_materialTab;
    delete m_levelTab;
    delete m_alignTab;
    delete m_bondingTab;
    m_materialTab = nullptr;
    m_levelTab = nullptr;
    m_alignTab = nullptr;
    m_bondingTab = nullptr;
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
        const QStringList dynamicCombos = { QStringLiteral("comboBox_bondingProcess") };  // 命令下拉单独重建（绑定 raw 数据）
        const QStringList dynamicBtns = { QStringLiteral("btnEdit"), QStringLiteral("btnSave") };

        auto addSimple = [this](QWidget *w) {
            if (!w) return;
            const QString t = w->property("text").toString();
            if (!t.isEmpty()) m_langWidgets.append({w, t});
        };
        for (QLabel *w : m_mainWidget->findChildren<QLabel *>())
            if (!dynamicBtns.contains(w->objectName())) addSimple(w);
        for (QPushButton *w : m_mainWidget->findChildren<QPushButton *>())
            if (!dynamicBtns.contains(w->objectName())) addSimple(w);
        for (QCheckBox *w : m_mainWidget->findChildren<QCheckBox *>())
            addSimple(w);
        for (QRadioButton *w : m_mainWidget->findChildren<QRadioButton *>())
            addSimple(w);

        for (QTabWidget *tw : m_mainWidget->findChildren<QTabWidget *>())
            for (int i = 0; i < tw->count(); ++i)
                m_langTabs.append({tw, i, tw->tabText(i)});

        for (QComboBox *cb : m_mainWidget->findChildren<QComboBox *>())
        {
            if (dynamicCombos.contains(cb->objectName()))
                continue;
            for (int i = 0; i < cb->count(); ++i)
                m_langCombos.append({cb, i, cb->itemText(i)});
        }

        for (QTableWidget *tw : m_mainWidget->findChildren<QTableWidget *>())
            for (int c = 0; c < tw->columnCount(); ++c)
                if (QTableWidgetItem *h = tw->horizontalHeaderItem(c))
                    m_langHeaders.append({tw, c, h->text()});

        m_langReady = true;
    }

    for (const auto &e : m_langWidgets)
        if (e.first) e.first->setProperty("text", TR_STRQ(e.second));

    for (const auto &e : m_langTabs)
        if (e.widget && e.index >= 0 && e.index < static_cast<QTabWidget *>(e.widget)->count())
            static_cast<QTabWidget *>(e.widget)->setTabText(e.index, TR_STRQ(e.key));

    QList<QComboBox *> doneCombos;
    for (const auto &e : m_langCombos)
    {
        auto *cb = static_cast<QComboBox *>(e.widget);
        if (!cb || doneCombos.contains(cb)) continue;
        doneCombos.append(cb);
        QStringList keys;
        for (const auto &ee : m_langCombos)
            if (ee.widget == cb) keys.append(ee.key);
        const int cur = cb->currentIndex();
        cb->clear();
        for (const QString &k : keys) cb->addItem(TR_STRQ(k));
        if (cur >= 0 && cur < cb->count()) cb->setCurrentIndex(cur);
    }

    for (const auto &e : m_langHeaders)
    {
        auto *tw = static_cast<QTableWidget *>(e.widget);
        if (tw && e.index < tw->columnCount())
            tw->setHorizontalHeaderItem(e.index, new QTableWidgetItem(TR_STRQ(e.key)));
    }

    // 键合命令下拉 + 已建好的工序表：由 BondingTab 按 Tr(raw 命令名) 重放显示文本
    // （raw 数据不随语言变，表格就地重译，不重建、不丢数据）
    if (m_bondingTab)
    {
        m_bondingTab->retranslateCommandCombo();
        m_bondingTab->retranslateTable();
    }

    // 自定义分组标题（ISTAR GroupBox 标题不走 QWidget::text，单独重放）
    ui->gb_upChipInfo->SetHeaderText(TR_STR("Up Chip Info"));
    ui->gb_dwChipInfo->SetHeaderText(TR_STR("Down Chip Info"));
    ui->gb_blueWaferInfo->SetHeaderText(TR_STR("Blue Film Wafer Info"));
    ui->gb_coarseLevelPoints->SetHeaderText(TR_STR("Coarse Leveling Points"));
    ui->gb_upLevelPoints->SetHeaderText(TR_STR("Up Leveling Points"));
    ui->gb_dwLevelPoints->SetHeaderText(TR_STR("Down Leveling Points"));
    ui->gb_levelSet->SetHeaderText(TR_STR("Leveling Settings"));
    ui->gb_upLevelMarkSearch->SetHeaderText(TR_STR("Up Align Point Search"));
    ui->gb_dwLevelMarkSearch->SetHeaderText(TR_STR("Down Align Point Search"));
    ui->gb_levelFocalPlaneOffset->SetHeaderText(TR_STR("Leveling Focal Plane Offset"));
    ui->gb_fineLevelResultToleranceRange->SetHeaderText(TR_STR("Fine Leveling Result Tolerance Range"));
    ui->gb_alignPoints->SetHeaderText(TR_STR("Align Points"));
    ui->gb_fineAlignResultToleranceRange->SetHeaderText(TR_STR("Fine Align Result Tolerance Range"));
    ui->gb_alignSetting->SetHeaderText(TR_STR("Align Settings"));
    ui->gb_upAlignMarkSearch->SetHeaderText(TR_STR("Up Align Point Search"));
    ui->gb_dwAlignMarkSearch->SetHeaderText(TR_STR("Down Align Point Search"));
    ui->gb_bondingProcess->SetHeaderText(TR_STR("Bonding Process"));

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

    auto client = RM_GUIClient::GetInstance().GetRMClient("rm");
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

        // 新建处方 body 为空串，from_rawJson 对空串会抛异常；空 body 直接用默认结构（各表空/键合默认工序）
        S_ChipRecipe recipeStruct;
        if (!bodyJson.empty())
        {
            recipeStruct = Serializer::from_rawJson<S_ChipRecipe>(bodyJson);
        }

        UpdateUI(recipeStruct);
        setEditable(m_editable);
        // UpdateUI 重建表格会触发 itemChanged → mark，加载完成后清掉
        m_dataModified = false;
    }
}

void RecipeModule::onEditClicked()
{
    // 当前在编辑模式(文本为Cancel)时点击 = 取消编辑；有未保存修改则弹窗提醒
    if (m_editable && m_dataModified)
    {
        MessageDialog(MessageDialog::Attention, m_mainWidget, TR_STR("Data has been modified and not saved yet")).exec();
        //return;
    }

    m_editable = !m_editable;
    setEditable(m_editable);
    ui->btnEdit->setText(m_editable ? QStringLiteral("Cancel") : QStringLiteral("Edit"));
}

void RecipeModule::onSaveClicked()
{
    if (!m_editable) return;

    S_ChipRecipe recipeStruct = GetUIInfo();
    // std::string json = struct_pack::to_json(recipeStruct);
    std::string json = Serializer::to_rawJson(recipeStruct);

    RcpBodySaveInfo info;
    info.rcpName = m_currentRecipe.toStdString();
    info.body = json;

    auto client = RM_GUIClient::GetInstance().GetRMClient("rm");
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

    QList<QPushButton*> buttons = m_mainWidget->findChildren<QPushButton*>();
    for (auto *b : buttons)
    {
        if (b != ui->btnEdit)
            b->setEnabled(editable);
    }

    // 所有表格随编辑模式切换（含空单元格）
    QList<QTableWidget*> tables = m_mainWidget->findChildren<QTableWidget*>();
    for (auto *tw : tables)
    {
        tw->setEditTriggers(editable
            ? QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
            : QAbstractItemView::NoEditTriggers);

        // 普通点表：非编辑模式置灰（键合表由 BondingTab 独立处理勾选状态）
        if (tw != ui->tw_bondingProcess)
        {
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
    }

    // 键合工序表格：勾选列随编辑模式切换可勾选状态（BondingTab 负责）
    if (m_bondingTab)
        m_bondingTab->setEditable(editable);
}

// 将 recipe 数据填充到控件：逐页签分发
void RecipeModule::UpdateUI(const S_ChipRecipe& recipe)
{
    m_materialTab->load(recipe.material);
    m_levelTab->load(recipe.level);
    m_alignTab->load(recipe.align);
    m_bondingTab->load(recipe.bond);
}

// 读取控件数据到 recipe：逐页签收集
S_ChipRecipe RecipeModule::GetUIInfo()
{
    S_ChipRecipe recipeStruct;
    m_materialTab->save(recipeStruct.material);
    m_levelTab->save(recipeStruct.level);
    m_alignTab->save(recipeStruct.align);
    m_bondingTab->save(recipeStruct.bond);
    return recipeStruct;
}

void RecipeModule::setConnections()
{
    // 编辑/保存（页签内按钮/表格连接由各 Tab::setConnect() 完成）
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

    QList<QTableWidget*> tables = m_mainWidget->findChildren<QTableWidget*>();
    for (auto *tw : tables)
        QObject::connect(tw, &QTableWidget::itemChanged, mark);
}

void RecipeModule::markDataModified()
{
    m_dataModified = true;
}

extern "C" {

__declspec(dllexport) IREModule* CreateREModule()
{
    return new RecipeModule();
}

}
