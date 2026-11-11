#include "BondingTab.h"

#include "ui_RecipeModule.h"
#include "ReData/S_Recipe.h"
#include "Bonding/TheoryBondData.h"
#include "Bonding/SimulationCurveDialog.h"
#include "dialog/messagedialog.h"
#include "istar_language.h"
#include "istar_ui_stylesheet.h"

#include <QPushButton>
#include <QObject>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QComboBox>
#include <QHeaderView>
#include <QColor>
#include <QtGlobal>
#include <utility>

using namespace ISTAR;
using namespace ISTAR::GUI;
using namespace ISTAR::UIStyle;

#define TR_STR(msg)  QString::fromStdString(Tr(msg))
#define TR_STRQ(msg) QString::fromStdString(Tr((msg).toStdString()))

// 键合参数列固定为 7 列：11 条命令中参数最多的是 ThermostaticPosCtl（7 个参数），
// 7 即命令定义表的上限。参数列数只增不减，恒 >= 7（含 Command 列共 >= 8 列）。
constexpr int kBondingDefaultParamCount = 7;

// 新工序 / 默认工序的参数初值：按参数类型给出结构合法的默认值
// （留空会被控制层的严格校验当成非法值，如数字参数报 ParamNumeric）。
//   spin  → "0"（数字文本）
//   combo → 首个规范选项 key（单元格显示为 Tr(key) 译文）
//   check → 未勾选的选项 key（options[1]，与单元格 Unchecked 状态一致）
// 定义表内的 options 意外为空时回落空串（QStringList::value 越界返回空，不会崩）。
static QString bondingDefaultParamValue(const BondingParamItem &it)
{
    if (it.type == "spin")
    {
        return QStringLiteral("0");
    }
    if (it.type == "combo")
    {
        return it.options.value(0);
    }
    if (it.type == "check")
    {
        return it.options.value(1);
    }
    return QString();
}

// 键合命令规范名（raw）清单：顺序即下拉/显示顺序，与本地参数定义表
// （BondingParamDelegate.cpp 的 bondingParamDefTable）逐字一致。
// 编辑器只用这一个标识：显示文本一律 Tr(raw)（译文配在 Language.csv 里 key = raw 的行），
// 落盘/查定义表也用它，不再维护"英文显示名 → raw"的映射表。
static const QStringList &bondingCmdKeys()
{
    static const QStringList s_keys = {
        QStringLiteral("ManualForceCtl"),
        QStringLiteral("WaitForManualForce"),
        QStringLiteral("TempCtl"),
        QStringLiteral("WaitForTemp"),
        QStringLiteral("HoldTime"),
        QStringLiteral("FormicAcid"),
        QStringLiteral("PosCtlModeTrigger"),
        QStringLiteral("ThermostaticPosCtl"),
        QStringLiteral("CoolingPosCtl"),
        QStringLiteral("UpSuckerBreakVac"),
        QStringLiteral("BondArmRelativeMove"),
    };
    return s_keys;
}

BondingTab::BondingTab(Ui::RecipeModule *ui, QWidget *dialogParent, std::function<void()> onModified)
    : m_ui(ui)
    , m_dialogParent(dialogParent)
    , m_onModified(std::move(onModified))
{
    m_ui->comboBox_bondingProcess->setStyleSheet(iStarUIStyleSheet->ComboBox);
    // 命令下拉框：11 条命令（顺序即显示顺序），命令名（raw）与本地参数定义表
    // （BondingParamDelegate.cpp 的 bondingParamDefTable）逐字一致。
    // 编辑器只用 raw 命令名这一个标识（与 GUI 一致）：下拉文本 = Tr(raw)，译文统一配在
    // Language.csv 里 key = raw 的行的 English 列；itemData 存 raw，供落盘/查定义表使用。
    const QStringList &cmdRaw = bondingCmdKeys();
    for (int i = 0; i < cmdRaw.size(); ++i)
    {
        m_ui->comboBox_bondingProcess->addItem(TR_STRQ(cmdRaw[i]));
        m_ui->comboBox_bondingProcess->setItemData(i, cmdRaw[i]);
    }
    selectCmd = m_ui->comboBox_bondingProcess->itemData(0).toString();

    // 键合工序表格（模仿平台 Bonding_tableWidget：每个工序 = 表头行 + 数据行）
    m_ui->tw_bondingProcess->setStyleSheet(iStarUIStyleSheet->tabStyle);
    m_ui->tw_bondingProcess->horizontalHeader()->setMinimumSectionSize(60);
    // 列宽自适应（QHeaderView::ResizeToContents）统一在 bonding_setColumnCount() 内设置，
    // 这样每次列数变化后仍然生效，无需在此重复设置
    m_ui->tw_bondingProcess->setSelectionBehavior(QTableWidget::SelectRows);
    m_ui->tw_bondingProcess->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ui->tw_bondingProcess->setItemDelegate(new BondingParamDelegate(m_ui->tw_bondingProcess, m_ui->tw_bondingProcess));
    m_ui->tw_bondingProcess->setEditTriggers(QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    // 空表初态就是固定 7 个参数列（bonding_setColumnCount 收的是参数列数，不含 Command 列）
    bonding_setColumnCount(kBondingDefaultParamCount);
    m_ui->tw_bondingProcess->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
}

void BondingTab::setConnect()
{
    // 键合工序：增删/上下移/命令下拉框/选中联动
    QObject::connect(m_ui->btn_add_bondingProcess, &QPushButton::clicked, [this]() { bondingTabWidget_add(m_ui->tw_bondingProcess); });
    QObject::connect(m_ui->btn_delete_bondingProcess, &QPushButton::clicked, [this]() { bondingTabWidget_delete(m_ui->tw_bondingProcess); });
    QObject::connect(m_ui->btn_up_bondingProcess, &QPushButton::clicked, [this]() { bondingTabWidget_up(m_ui->tw_bondingProcess); });
    QObject::connect(m_ui->btn_down_bondingProcess, &QPushButton::clicked, [this]() { bondingTabWidget_down(m_ui->tw_bondingProcess); });

    // 校验处方
    QObject::connect(m_ui->btn_verifyRecipe, &QPushButton::clicked, [this]() { onVerifyRecipeClicked(); });

    // 模拟曲线
    QObject::connect(m_ui->btn_simulationCurve, &QPushButton::clicked, [this]() { onSimulationCurveClicked(); });

    QObject::connect(m_ui->comboBox_bondingProcess, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [this](int idx) {
        // 下拉项的唯一标识就是 raw 命令名（itemData）；下拉文本是译文，绝不可当命令用
        selectCmd = idx >= 0 ? m_ui->comboBox_bondingProcess->itemData(idx).toString() : QString();
    });

    QObject::connect(static_cast<QTableWidget *>(m_ui->tw_bondingProcess), &QTableWidget::currentItemChanged,
                     [this](QTableWidgetItem *current, QTableWidgetItem *previous) {
        Q_UNUSED(previous);
        if (!current) return;
        int row = current->row();
        QTableWidgetItem *row0Item = m_ui->tw_bondingProcess->item(row, 0);
        if (!row0Item) return;
        if (row0Item->data(Qt::UserRole).toString() == "header")
        {
            if (row + 1 < m_ui->tw_bondingProcess->rowCount())
                row0Item = m_ui->tw_bondingProcess->item(row + 1, 0);
            if (!row0Item) return;
        }
        QString rawCmd = row0Item->data(Qt::UserRole).toString();
        if (!rawCmd.isEmpty())
        {
            // 下拉项的 itemData 恒为 raw 命令名，按 raw 精确选中即可
            const int idx = m_ui->comboBox_bondingProcess->findData(rawCmd);
            if (idx >= 0)
                m_ui->comboBox_bondingProcess->setCurrentIndex(idx);
        }
    });
}

void BondingTab::setEditable(bool editable)
{
    m_bondingEditable = editable;
    applyBondingCheckFlags();
}

void BondingTab::retranslateCommandCombo()
{
    // 键合命令下拉：文本按 Tr(raw) 重放，raw 数据不随语言变化
    auto *cb = m_ui->comboBox_bondingProcess;
    const QString curRaw = cb->itemData(cb->currentIndex()).toString();
    cb->clear();
    const QStringList &cmdRaw = bondingCmdKeys();
    for (int i = 0; i < cmdRaw.size(); ++i)
    {
        cb->addItem(TR_STRQ(cmdRaw[i]));
        cb->setItemData(i, cmdRaw[i]);
    }
    const int restore = cb->findData(curRaw);
    cb->setCurrentIndex(restore >= 0 ? restore : 0);
    selectCmd = cb->itemData(cb->currentIndex()).toString();
}

// 语言切换：就地重译已建好的工序表（表头行的命令名/参数名、combo 单元格、列头、行号标签），
// 不重建表格、不改动任何数据角色、也不改 key。
// 行对 = 表头行 baseRow + 数据行 baseRow + 1；命令 raw 取自数据行第 0 列的 Qt::UserRole
// （建行时写入，与落盘值同一份），空/缺失时保持原样不猜。
void BondingTab::retranslateTable()
{
    auto *tw = m_ui->tw_bondingProcess;

    // 列头（Command / P1..Pn）与行号标签按当前语言重放
    bonding_updateHorizontalHeaderLabels();
    bonding_updateRowHeaderLabels();

    for (int baseRow = 0; baseRow + 1 < tw->rowCount(); ++baseRow)
    {
        QTableWidgetItem *headerCmdItem = tw->item(baseRow, 0);
        if (!headerCmdItem || headerCmdItem->data(Qt::UserRole).toString() != "header")
        {
            continue;   // 只处理"表头行 + 数据行"成对的工序
        }

        QTableWidgetItem *dataCmdItem = tw->item(baseRow + 1, 0);
        const QString rawCmd = dataCmdItem ? dataCmdItem->data(Qt::UserRole).toString() : QString();
        if (rawCmd.isEmpty())
        {
            continue;   // 命令缺失/为空：不猜显示文本，单元格原样保留
        }

        // 表头行第 0 列：命令显示文本 = Tr(raw)
        headerCmdItem->setText(TR_STRQ(rawCmd));

        // 参数名按命令定义的参数顺序与列序一一对应取用
        const QList<BondingParamItem> items = getBondingParamItems(rawCmd);
        for (int c = 1; c < tw->columnCount(); ++c)
        {
            const int paramIdx = c - 1;

            // 表头行参数名 = Tr(参数 key)；超出命令定义的列不猜，单元格保持不变
            QTableWidgetItem *headerParamItem = tw->item(baseRow, c);
            if (headerParamItem && paramIdx < items.size())
            {
                headerParamItem->setText(TR_STRQ(items[paramIdx].key));
            }

            QTableWidgetItem *dataParamItem = tw->item(baseRow + 1, c);
            if (!dataParamItem)
            {
                continue;
            }
            // check 单元格：取值靠勾选状态（文本恒为空），语言切换不动它
            if (dataParamItem->data(Qt::UserRole + 1).toString() == "check")
            {
                continue;
            }
            // combo 单元格：显示文本的唯一来源是 UserRole + 3 里的规范选项 key；
            // spin 单元格没有该数据角色（文本即数字），原样保留
            const QString optKey = dataParamItem->data(Qt::UserRole + 3).toString();
            if (!optKey.isEmpty())
            {
                dataParamItem->setText(TR_STRQ(optKey));
            }
        }
    }
}

QList<BondingParamItem> BondingTab::getBondingParamItems(const QString &cmd) const
{
    // cmd 即命令规范名（raw），直接查定义表（未知命令返回空表）
    return bondingParamDefs(cmd);
}

std::vector<S_BondParam> BondingTab::bonding_rowToParams(int dataRow, const QString &rawCmd) const
{
    std::vector<S_BondParam> plist;
    QList<BondingParamItem> items = getBondingParamItems(rawCmd);
    for (int j = 1; j < m_ui->tw_bondingProcess->columnCount(); ++j)
    {
        QTableWidgetItem *paramItem = m_ui->tw_bondingProcess->item(dataRow, j);
        if (!paramItem)
        {
            continue;
        }

        S_BondParam bp;
        // 参数名取用顺序：单元格记住的 key（load 时随处方带下来）→ 命令定义的 key
        const QString cellKey = paramItem->data(Qt::UserRole + 2).toString();
        if (!cellKey.isEmpty())
        {
            bp.key = cellKey.toStdString();
        }
        else if (j - 1 < items.size())
        {
            bp.key = items[j - 1].key.toStdString();
        }
        else
        {
            // 没有命令定义的多余列：参数名无从得知，留空（不再拿显示文本冒充 key）
            bp.key.clear();
        }

        if (j - 1 < items.size())
        {
            const BondingParamItem &it = items[j - 1];
            if (it.type == "check")
            {
                // 勾选状态 → 参数定义里的两个选项 key（[0]=勾选 / [1]=未勾选）
                bp.value = (paramItem->checkState() == Qt::Checked
                            ? bondingParamCheckedText(it.options)
                            : bondingParamUncheckedText(it.options)).toStdString();
            }
            else if (it.type == "combo")
            {
                // 处方值 = 规范选项 key，存于数据角色 UserRole+3（单元格显示的是译文，不可回写）
                bp.value = bondingParamOptionKey(it, paramItem->data(Qt::UserRole + 3).toString()).toStdString();
            }
            else
            {
                bp.value = paramItem->text().toStdString();   // spin：数字文本
            }
        }
        else
        {
            // 超出命令定义的列：单元格文本原样存入
            bp.value = paramItem->text().toStdString();
        }
        plist.push_back(std::move(bp));
    }
    return plist;
}

void BondingTab::bonding_updateTableRow(int row, const QString &rawCmd, const QStringList &params,
                                        const QStringList &paramKeys)
{
    // 表头行：灰底，命令名（Tr(raw)）+ 参数名，不可编辑
    int headerRow = row;
    QTableWidgetItem *headerCmdItem = new QTableWidgetItem(TR_STRQ(rawCmd));
    headerCmdItem->setData(Qt::UserRole, QString("header"));
    headerCmdItem->setBackground(QColor(230, 230, 230));
    headerCmdItem->setFlags(headerCmdItem->flags() & ~Qt::ItemIsEditable);
    m_ui->tw_bondingProcess->setItem(headerRow, 0, headerCmdItem);

    QList<BondingParamItem> items = getBondingParamItems(rawCmd);
    for (int p = 0; p < items.count(); ++p)
    {
        // 表头文本 = Tr(参数 key)，建行时现取（不能在参数定义里缓存译文）
        QTableWidgetItem *headerParamItem = new QTableWidgetItem(TR_STRQ(items[p].key));
        headerParamItem->setData(Qt::UserRole, QString("header"));
        headerParamItem->setBackground(QColor(230, 230, 230));
        headerParamItem->setFlags(headerParamItem->flags() & ~Qt::ItemIsEditable);
        m_ui->tw_bondingProcess->setItem(headerRow, p + 1, headerParamItem);
    }

    // 数据行：命令列留空且不可编辑，参数列可编辑；命令列(第0列)灰底
    int dataRow = row + 1;
    QTableWidgetItem *dataCmdItem = new QTableWidgetItem(QString());
    dataCmdItem->setData(Qt::UserRole, rawCmd);
    dataCmdItem->setBackground(QColor(230, 230, 230));
    dataCmdItem->setFlags(dataCmdItem->flags() & ~Qt::ItemIsEditable);
    m_ui->tw_bondingProcess->setItem(dataRow, 0, dataCmdItem);
    for (int p = 0; p < params.count(); ++p)
    {
        const bool hasDef = p < items.count();
        QTableWidgetItem *dataParamItem;
        if (hasDef && items[p].type == "check")
        {
            // check：显示勾选框，处方值由 checkState 决定 → UserRole+3 记下对应的选项 key
            dataParamItem = new QTableWidgetItem();
            dataParamItem->setData(Qt::UserRole + 1, QString("check"));
            dataParamItem->setFlags(dataParamItem->flags() | Qt::ItemIsUserCheckable);
            dataParamItem->setCheckState(
                bondingParamValueIsChecked(items[p].options, params[p]) ? Qt::Checked : Qt::Unchecked);
            dataParamItem->setData(Qt::UserRole + 3, bondingParamOptionKey(items[p], params[p]));
        }
        else
        {
            // combo：显示 Tr(选项 key)，同时把规范选项 key 存进 UserRole+3（保存端取值用）
            // spin：数字文本原样显示；超出定义的列原样显示
            const QString valText = hasDef ? bondingParamValueToText(items[p], params[p]) : params[p];
            dataParamItem = new QTableWidgetItem(valText);
            if (hasDef && items[p].type == "combo")
            {
                dataParamItem->setData(Qt::UserRole + 3, bondingParamOptionKey(items[p], params[p]));
            }
        }
        // 单元格记住自己的参数名：命令定义优先，其次是处方带来的 key（save 时据此还原 S_BondParam.key）
        // 用 UserRole + 2：UserRole + 1 已被上面的 check 标记占用；UserRole + 3 存 combo/check 的选项 key
        dataParamItem->setData(Qt::UserRole + 2,
                               hasDef ? items[p].key
                                      : (p < paramKeys.count() ? paramKeys[p] : QString()));
        m_ui->tw_bondingProcess->setItem(dataRow, p + 1, dataParamItem);
    }
}

void BondingTab::bonding_updateRowHeaderLabels()
{
    int group = 0;
    for (int i = 0; i < m_ui->tw_bondingProcess->rowCount(); ++i)
    {
        QTableWidgetItem *item = m_ui->tw_bondingProcess->item(i, 0);
        bool isHeader = item && item->data(Qt::UserRole).toString() == "header";
        QString text = isHeader ? QString::number(++group) : QString();
        m_ui->tw_bondingProcess->setVerticalHeaderItem(i, new QTableWidgetItem(text));
    }
    m_ui->tw_bondingProcess->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
}

// 统计键合工序表中所有 Target Force / Target Temperature 的最大值（找不到按 0）
void BondingTab::calcBondingMaxTargets(double &maxForce, double &maxTemp) const
{
    // 参数 key 是规范字面量（不随语言变化）
    const QString keyTargetForce = QStringLiteral("bond_TargetForceN");
    const QString keyTargetTemp = QStringLiteral("bond_TargetTemperatureC");

    maxForce = 0.0;
    maxTemp = 0.0;

    for (int r = 0; r < m_ui->tw_bondingProcess->rowCount(); ++r)
    {
        QTableWidgetItem *cmdItem = m_ui->tw_bondingProcess->item(r, 0);
        if (!cmdItem || cmdItem->data(Qt::UserRole).toString() == "header")
        {
            continue;   // 表头行不带参数数据
        }

        const QString rawCmd = cmdItem->data(Qt::UserRole).toString();
        const QList<BondingParamItem> items =
                rawCmd.isEmpty() ? QList<BondingParamItem>() : getBondingParamItems(rawCmd);
        for (int c = 1; c < m_ui->tw_bondingProcess->columnCount(); ++c)
        {
            QTableWidgetItem *paramItem = m_ui->tw_bondingProcess->item(r, c);
            if (!paramItem)
            {
                continue;
            }

            // 参数 key 取用顺序与 bonding_rowToParams 一致：单元格记住的 key → 命令定义的 key
            QString key = paramItem->data(Qt::UserRole + 2).toString();
            if (key.isEmpty() && c - 1 < items.size())
            {
                key = items[c - 1].key;
            }
            if (key != keyTargetForce && key != keyTargetTemp)
            {
                continue;
            }

            bool ok = false;
            const double val = paramItem->text().toDouble(&ok);
            if (!ok)
            {
                continue;   // 空单元格 / 非数字的旧值不参与统计
            }

            if (key == keyTargetForce)
            {
                maxForce = qMax(maxForce, val);
            }
            else
            {
                maxTemp = qMax(maxTemp, val);
            }
        }
    }
}

// 列表头（Command / P1..Pn）按当前语言重放，并重新应用列宽自适应模式：
// 改表头/改列数都会重建表头项，故每次设完表头后重新应用 ResizeToContents，保证依然生效
void BondingTab::bonding_updateHorizontalHeaderLabels()
{
    QStringList headers;
    headers << TR_STR("Command");
    for (int c = 1; c < m_ui->tw_bondingProcess->columnCount(); ++c)
    {
        headers << QString("P%1").arg(c);
    }
    m_ui->tw_bondingProcess->setHorizontalHeaderLabels(headers);
    m_ui->tw_bondingProcess->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

void BondingTab::bonding_setColumnCount(int paramCount)
{
    m_ui->tw_bondingProcess->setColumnCount(qMax(1, paramCount + 1));
    bonding_updateHorizontalHeaderLabels();
}

void BondingTab::bonding_updateColumnCount()
{
    int maxParamCount = 0;
    int curCols = m_ui->tw_bondingProcess->columnCount();
    for (int i = 0; i < m_ui->tw_bondingProcess->rowCount(); ++i)
    {
        QTableWidgetItem *item = m_ui->tw_bondingProcess->item(i, 0);
        if (!item || item->data(Qt::UserRole).toString() == "header")
        {
            continue;
        }

        QString rawCmd = item->data(Qt::UserRole).toString();
        int need = rawCmd.isEmpty() ? 0 : getBondingParamItems(rawCmd).count();
        // 数据行实际已存在的参数项可能超过命令定义，保留这些数据所需的列
        for (int c = curCols - 1; c >= 1; --c)
        {
            if (m_ui->tw_bondingProcess->item(i, c))
            {
                need = qMax(need, c);
                break;
            }
        }
        if (need > maxParamCount)
        {
            maxParamCount = need;
        }
    }
    // 参数列数下限 = kBondingDefaultParamCount（7）：列数只增不减，
    // 即使所有工序的参数都少于 7 个，表格也保持固定的 7 个参数列（含 Command 共 8 列）
    bonding_setColumnCount(qMax(maxParamCount, kBondingDefaultParamCount));
}

// 在工序参数中按规范参数 key 取原始文本。只认同一个 key：失配返回空，
// 已删除"按定义序号取值"的兜底——那会让"显示文案被当成 key"之类的失配静默通过。
static QString bondParamText(const S_BondStep &s, const char *key)
{
    for (const S_BondParam &bp : s.params)
    {
        if (bp.key == key)
        {
            return QString::fromStdString(bp.value);
        }
    }
    return QString();
}

// 数值型参数：文本转 double（空/非数字按 0），用于模拟曲线/处方校验
static double bondParamValue(const S_BondStep &s, const char *key)
{
    return bondParamText(s, key).toDouble();
}

// 勾选型参数：处方值命中勾选项 key 即为勾选（无 legacy 猜测）
static bool bondParamFlag(const S_BondStep &s, const char *key, const char *checkedOptKey)
{
    return bondParamText(s, key) == QString::fromUtf8(checkedOptKey);
}

// 默认键合工序（空序列时补全；参数个数与规范 key 取自本模块的参数定义表）
static std::vector<S_BondStep> defaultBondSteps()
{
    static const char *const cmds[] = {
        "ManualForceCtl",
        "WaitForManualForce",
        "TempCtl",
        "WaitForTemp",
        "HoldTime",
    };
    std::vector<S_BondStep> steps;
    for (const char *cmd : cmds)
    {
        S_BondStep s;
        s.command = cmd;
        const QList<BondingParamItem> defs = bondingParamDefs(QString::fromUtf8(cmd));
        for (const BondingParamItem &it : defs)
        {
            // 参数初值按类型取默认值（spin "0" / combo 首个选项 key / check 未勾选 key），不留空
            s.params.push_back(S_BondParam{ it.key.toStdString(), bondingDefaultParamValue(it).toStdString() });
        }
        steps.push_back(std::move(s));
    }
    return steps;
}

void BondingTab::load(const S_RecipeChipBond &bond)
{
    m_ui->tw_bondingProcess->setRowCount(0);
    // 空表也保持固定 7 个参数列（bonding_setColumnCount 收的是参数列数，不含 Command 列）
    bonding_setColumnCount(kBondingDefaultParamCount);

    // 空序列（新配方 / 旧版本配方）时补默认 5 条工序，保证表格不空
    std::vector<S_BondStep> defaults;
    const std::vector<S_BondStep> *pSteps = &bond.lst_BondingSequence;
    if (pSteps->empty())
    {
        defaults = defaultBondSteps();
        pSteps = &defaults;
    }

    // 列表元素 ↔ 表格一行工序（表头行 + 数据行），顺序保持一致
    for (const S_BondStep &step : *pSteps)
    {
        QString rawCmd = QString::fromStdString(step.command);
        // 只保留这一条读取路径：参数按"列序 = 参数序"原样对应
        // （combo/check 的 value 就是规范选项 key，spin 是数字文本），
        // 不再按显示文案匹配、也没有"按原序号取值"的兼容分支——编辑器只需读懂自己写出的文件
        QStringList params;
        QStringList keys;   // 与 params 等长：各参数自带的规范 key，保存时原样还原
        params.reserve(int(step.params.size()));
        keys.reserve(int(step.params.size()));
        for (const S_BondParam &bp : step.params)
        {
            params.append(QString::fromStdString(bp.value));
            keys.append(QString::fromStdString(bp.key));
        }

        // 先扩展列数再填行，否则参数单元格会被丢弃
        int need = qMax(getBondingParamItems(rawCmd).count(), params.count());
        if (need + 1 > m_ui->tw_bondingProcess->columnCount())
        {
            bonding_setColumnCount(need);
        }
        int baseRow = m_ui->tw_bondingProcess->rowCount();
        m_ui->tw_bondingProcess->insertRow(baseRow);
        m_ui->tw_bondingProcess->insertRow(baseRow + 1);
        bonding_updateTableRow(baseRow, rawCmd, params, keys);
    }

    bonding_updateColumnCount();
    bonding_updateRowHeaderLabels();
    applyBondingCheckFlags();
}

void BondingTab::save(S_RecipeChipBond &bond) const
{
    bond.lst_BondingSequence.clear();

    for (int i = 0; i < m_ui->tw_bondingProcess->rowCount(); ++i)
    {
        QTableWidgetItem *item = m_ui->tw_bondingProcess->item(i, 0);
        if (!item)
        {
            continue;
        }
        if (item->data(Qt::UserRole).toString() == "header")
        {
            continue;
        }

        QString rawCmd = item->data(Qt::UserRole).toString();
        if (rawCmd.isEmpty())
        {
            continue;
        }
        std::vector<S_BondParam> plist = bonding_rowToParams(i, rawCmd);

        S_BondStep step;
        step.command = rawCmd.toStdString();
        step.params = std::move(plist);
        bond.lst_BondingSequence.push_back(std::move(step));
    }
    // 汇总本页所有工序的 Target Force / Target Temperature 最大值
    calcBondingMaxTargets(bond.maxTargetForce, bond.maxTargetTemp);
}

void BondingTab::onSimulationCurveClicked()
{
    S_RecipeChipBond bond;
    save(bond);

    TheoryBondData data;
    getTheoryBondData(data, bond.lst_BondingSequence);

    SimulationCurveDialog dialog(m_dialogParent);
    dialog.SetTheoryBondData(data);
    dialog.exec();
}

// 理论键合曲线模拟：按工序顺序推进时间/温度/力状态（模仿平台 getTheoryBondData）
void BondingTab::getTheoryBondData(TheoryBondData &dt, const std::vector<S_BondStep> &steps)
{
    double dTime = 0;
    double dTempUp = 20;
    double dTempDw = 20;
    double dForce = 0;
    double dForceSteadyTime = 0;

    dt.vec_TempUp.append(QPointF(dTime, dTempUp));
    dt.vec_TempDw.append(QPointF(dTime, dTempDw));
    dt.vec_Force.append(QPointF(dTime, dForce));

    for (const S_BondStep &e : steps)
    {
        const std::string &cmd = e.command;

        if (cmd == "ManualForceCtl")
        {
            // 勾选 bond_BondInitialPosition 则先移动到位（平台按 10s 估算）
            if (e.params.size() >= 5
                && bondParamFlag(e, "bond_BondInitialPosition", "bond_BondInitialPosition_Checked"))
            {
                dTime += 10;
            }
            if (e.params.size() >= 3)
            {
                dForceSteadyTime = bondParamValue(e, "bond_SteadyTimeS");
            }
        }
        else if (cmd == "WaitForTemp")
        {
            if (e.params.size() >= 2)
            {
                // 加热选择按 option key 判定（不再用下标，消除选项顺序依赖）+ 目标温度；升温 10℃/s，降温 5℃/s
                double dDeltaTempUp = 0;
                double dDeltaTempDw = 0;
                const QString header = bondParamText(e, "bond_BondHead");
                const bool upAndDown = header == QString::fromUtf8("bond_BondHead_UpAndDown");
                const bool upOnly = header == QString::fromUtf8("bond_BondHead_Up");
                const bool downOnly = header == QString::fromUtf8("bond_BondHead_Down");
                const double targetTemp = bondParamValue(e, "bond_TargetTemperatureC");
                if (upAndDown)
                {
                    dDeltaTempUp = targetTemp - dTempUp;
                    dDeltaTempDw = targetTemp - dTempDw;
                    dTempUp = targetTemp;
                    dTempDw = targetTemp;
                }
                else if (upOnly)
                {
                    dDeltaTempUp = targetTemp - dTempUp;
                    dTempUp = targetTemp;
                }
                else if (downOnly)
                {
                    dDeltaTempDw = targetTemp - dTempDw;
                    dTempDw = targetTemp;
                }
                const double dTimeUp = dDeltaTempUp > 0 ? (dDeltaTempUp / 10) : (-dDeltaTempUp / 5);
                const double dTimeDw = dDeltaTempDw > 0 ? (dDeltaTempDw / 10) : (-dDeltaTempDw / 5);
                dt.vec_TempUp.append(QPointF(dTime + dTimeUp, dTempUp));
                dt.vec_TempDw.append(QPointF(dTime + dTimeDw, dTempDw));
                dTime += dTimeUp < dTimeDw ? dTimeDw : dTimeUp;
            }
        }
        else if (cmd == "WaitForManualForce")
        {
            // 力经过 SteadyTime 到达目标力
            dTime += dForceSteadyTime;
            if (e.params.size() >= 1)
            {
                dForce = bondParamValue(e, "bond_TargetForceN");
            }
        }
        else if (cmd == "HoldTime")
        {
            if (e.params.size() >= 1)
            {
                dTime += bondParamValue(e, "bond_TimeoutS");
            }
        }
        // TempCtl：模拟中无动作（与平台一致）

        dt.vec_TempUp.append(QPointF(dTime, dTempUp));
        dt.vec_TempDw.append(QPointF(dTime, dTempDw));
        dt.vec_Force.append(QPointF(dTime, dForce));
    }

    // 结尾：真空关闭稳定时间（RE 无该控件，暂按 0）+ 力释放
    dTime += 1;
    dt.vec_Force.append(QPointF(dTime, dForce));
    dt.vec_Force.append(QPointF(dTime, 0));
    dTime += 3;
    dt.vec_Force.append(QPointF(dTime, 0));
    dt.vec_TempUp.append(QPointF(dTime, dTempUp));
    dt.vec_TempDw.append(QPointF(dTime, dTempDw));
}

// 校验处方：按顺序配对 TempCtl/WaitForTemp、ManualForceCtl/WaitForManualForce 并比对参数（模仿平台 validateRecipe）
BondingTab::ValidationResult BondingTab::validateRecipe()
{
    S_RecipeChipBond bond;
    save(bond);
    const std::vector<S_BondStep> &steps = bond.lst_BondingSequence;

    struct Item { const S_BondStep *bs; int row; };
    std::vector<Item> tempItemCtls, waitTemps, forceItemCtls, waitForces;
    for (int j = 0; j < int(steps.size()); ++j)
    {
        Item it = { &steps[j], j + 1 };
        const std::string &cmd = steps[j].command;
        if (cmd == "TempCtl")
            tempItemCtls.push_back(it);
        else if (cmd == "WaitForTemp")
            waitTemps.push_back(it);
        else if (cmd == "ManualForceCtl")
            forceItemCtls.push_back(it);
        else if (cmd == "WaitForManualForce")
            waitForces.push_back(it);
    }

    int minS = qMin(int(tempItemCtls.size()), int(waitTemps.size()));
    for (int k = 0; k < minS; ++k)
    {
        const S_BondStep *tctl = tempItemCtls[k].bs;
        const S_BondStep *wtmp = waitTemps[k].bs;
        if (tctl->params.size() >= 2 && wtmp->params.size() >= 2)
        {
            double setTemp = bondParamValue(*tctl, "bond_TargetTemperatureC");   // TempCtl TargetTemp
            double waitTemp = bondParamValue(*wtmp, "bond_TargetTemperatureC");  // WaitForTemp TargetTemp
            if (setTemp != waitTemp)
                return { TempNotEqual, waitTemps[k].row, tempItemCtls[k].row, waitTemp, setTemp };
        }
    }
    for (int k = 0; k < minS; ++k)
    {
        const S_BondStep *tctl = tempItemCtls[k].bs;
        const S_BondStep *wtmp = waitTemps[k].bs;
        if (tctl->params.size() >= 3 && wtmp->params.size() >= 5)
        {
            double steady = bondParamValue(*tctl, "bond_SteadyTimeS");   // TempCtl SteadyTime
            double timeout = bondParamValue(*wtmp, "bond_TimeoutS");     // WaitForTemp TimeOut
            if (timeout <= steady)
                return { WaitTempTimeSetError, waitTemps[k].row, tempItemCtls[k].row, timeout, steady };
        }
    }
    minS = qMin(int(forceItemCtls.size()), int(waitForces.size()));
    for (int k = 0; k < minS; ++k)
    {
        const S_BondStep *fctl = forceItemCtls[k].bs;
        const S_BondStep *wfrc = waitForces[k].bs;
        if (fctl->params.size() >= 3 && wfrc->params.size() >= 4)
        {
            double steady = bondParamValue(*fctl, "bond_SteadyTimeS");   // ManualForceCtl SteadyTime
            double timeout = bondParamValue(*wfrc, "bond_TimeoutS");     // WaitForManualForce TimeOut
            if (timeout <= steady)
                return { ForceTimeSetError, waitForces[k].row, forceItemCtls[k].row, timeout, steady };
        }
    }
    return {};
}

void BondingTab::onVerifyRecipeClicked()
{
    ValidationResult r = validateRecipe();

    if (r.type == TempNotEqual)
    {
        MessageDialog dlg(MessageDialog::Error, m_dialogParent,
            QString("%1\n%2\n%3")
            .arg(TR_STR("Recipe verification failed"))
            .arg(TR_STR("Temperature mismatch: row %1 [Wait for Temperature] target temperature (%2) != row %3 [Temperature Control] target temperature (%4)")
                 .arg(r.rowB).arg(r.valB, 0, 'f', 1).arg(r.rowA).arg(r.valA, 0, 'f', 1))
            .arg(TR_STR("Please set both to the same value")));
        dlg.exec();
    }
    else if (r.type == WaitTempTimeSetError)
    {
        MessageDialog dlg(MessageDialog::Error, m_dialogParent,
            QString("%1\n%2\n%3")
            .arg(TR_STR("Recipe verification failed"))
            .arg(TR_STR("Wait for Temperature timeout error: row %1 [Wait for Temperature] timeout (%2 s) <= row %3 [Temperature Control] steady time (%4 s)")
                 .arg(r.rowB).arg(r.valB, 0, 'f', 1).arg(r.rowA).arg(r.valA, 0, 'f', 1))
            .arg(TR_STR("Please set the timeout greater than %1 s or the steady time less than %2 s")
                 .arg(r.valA, 0, 'f', 1).arg(r.valB, 0, 'f', 1)));
        dlg.exec();
    }
    else if (r.type == ForceTimeSetError)
    {
        MessageDialog dlg(MessageDialog::Error, m_dialogParent,
            QString("%1\n%2\n%3")
            .arg(TR_STR("Recipe verification failed"))
            .arg(TR_STR("Wait for Bond Force timeout error: row %1 [Wait for Bond Force] timeout (%2 s) <= row %3 [Force Control] steady time (%4 s)")
                 .arg(r.rowB).arg(r.valB, 0, 'f', 1).arg(r.rowA).arg(r.valA, 0, 'f', 1))
            .arg(TR_STR("Please set the timeout greater than %1 s or the steady time less than %2 s")
                 .arg(r.valA, 0, 'f', 1).arg(r.valB, 0, 'f', 1)));
        dlg.exec();
    }
    else
    {
        MessageDialog dlg(MessageDialog::Success, m_dialogParent, TR_STR("Recipe verification passed"));
        dlg.exec();
    }
}

void BondingTab::applyBondingCheckFlags()
{
    for (int r = 0; r < m_ui->tw_bondingProcess->rowCount(); ++r)
    {
        for (int c = 1; c < m_ui->tw_bondingProcess->columnCount(); ++c)
        {
            QTableWidgetItem *it = m_ui->tw_bondingProcess->item(r, c);
            if (it && it->data(Qt::UserRole + 1).toString() == "check")
            {
                if (m_bondingEditable)
                {
                    it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
                }
                else
                {
                    it->setFlags(it->flags() & ~Qt::ItemIsUserCheckable);
                }
            }
        }
    }
}

void BondingTab::bondingTabWidget_add(QTableWidget *widget)
{
    if (selectCmd.isEmpty())
    {
        return;
    }
    // 下拉当前项就是 raw 命令名，直接用（落盘/查定义表同一份标识）
    QString rawCmd = selectCmd;
    QList<BondingParamItem> items = bondingParamDefs(rawCmd);
    QStringList dlist;
    for (int i = 0; i < items.count(); ++i)
    {
        // 新工序按参数类型预填合法默认值（spin "0" / combo 首个选项 key / check 未勾选 key）：
        // 整条工序开箱即结构合法，保存时不会再被控制层的严格校验（如 ParamNumeric）拒掉
        dlist.append(bondingDefaultParamValue(items[i]));
    }

    // 新增命令的参数可能超过当前表格列数，先扩展列数，避免单元格被丢弃
    if (items.count() + 1 > widget->columnCount())
    {
        bonding_setColumnCount(items.count());
    }

    int baseRow = widget->rowCount();
    widget->insertRow(baseRow);
    widget->insertRow(baseRow + 1);
    bonding_updateTableRow(baseRow, rawCmd, dlist);
    bonding_updateRowHeaderLabels();
    m_onModified();
}

void BondingTab::bondingTabWidget_delete(QTableWidget *widget)
{
    int row = widget->currentRow();
    if (row < 0 || row >= widget->rowCount())
    {
        return;
    }

    QTableWidgetItem *item = widget->item(row, 0);
    if (!item)
    {
        return;
    }

    bool isHeader = (item->data(Qt::UserRole).toString() == "header");
    if (isHeader)
    {
        widget->removeRow(row);
        if (row < widget->rowCount())
        {
            widget->removeRow(row);
        }
    }
    else
    {
        widget->removeRow(row);
        if (row - 1 >= 0 && row - 1 < widget->rowCount())
        {
            QTableWidgetItem *prevItem = widget->item(row - 1, 0);
            if (prevItem && prevItem->data(Qt::UserRole).toString() == "header")
            {
                widget->removeRow(row - 1);
            }
        }
    }

    bonding_updateRowHeaderLabels();
    bonding_updateColumnCount();
    m_onModified();
}

void BondingTab::bondingTabWidget_up(QTableWidget *widget)
{
    int row = widget->currentRow();
    if (row < 0 || row >= widget->rowCount())
    {
        return;
    }

    QTableWidgetItem *item = widget->item(row, 0);
    if (!item)
    {
        return;
    }

    bool isHeader = (item->data(Qt::UserRole).toString() == "header");
    int groupStart = isHeader ? row : (row - 1);
    int groupEnd = isHeader ? (row + 1) : row;

    if (groupStart < 2)
    {
        return;
    }

    for (int c = 0; c < widget->columnCount(); ++c)
    {
        QTableWidgetItem *a = widget->takeItem(groupStart - 2, c);
        QTableWidgetItem *b = widget->takeItem(groupStart, c);
        widget->setItem(groupStart - 2, c, b);
        widget->setItem(groupStart, c, a);

        a = widget->takeItem(groupStart - 1, c);
        b = widget->takeItem(groupEnd, c);
        widget->setItem(groupStart - 1, c, b);
        widget->setItem(groupEnd, c, a);
    }

    widget->selectRow(groupStart - 2);
    bonding_updateRowHeaderLabels();
    m_onModified();
}

void BondingTab::bondingTabWidget_down(QTableWidget *widget)
{
    int row = widget->currentRow();
    if (row < 0 || row >= widget->rowCount())
    {
        return;
    }

    QTableWidgetItem *item = widget->item(row, 0);
    if (!item)
    {
        return;
    }

    bool isHeader = (item->data(Qt::UserRole).toString() == "header");
    int groupStart = isHeader ? row : (row - 1);
    int groupEnd = isHeader ? (row + 1) : row;

    if (groupEnd + 2 >= widget->rowCount())
    {
        return;
    }

    for (int c = 0; c < widget->columnCount(); ++c)
    {
        QTableWidgetItem *a = widget->takeItem(groupStart, c);
        QTableWidgetItem *b = widget->takeItem(groupEnd + 1, c);
        widget->setItem(groupStart, c, b);
        widget->setItem(groupEnd + 1, c, a);

        a = widget->takeItem(groupEnd, c);
        b = widget->takeItem(groupEnd + 2, c);
        widget->setItem(groupEnd, c, b);
        widget->setItem(groupEnd + 2, c, a);
    }

    widget->selectRow(groupEnd + 1);
    bonding_updateRowHeaderLabels();
    m_onModified();
}
