#include "BondingParamDelegate.h"
#include "istar_ui_stylesheet.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QModelIndex>
#include <QAbstractItemModel>
#include <QStyleOptionViewItem>
#include <QMap>

using namespace ISTAR::UIStyle;

#include "istar_language.h"
#define TR_STR(msg)  QString::fromStdString(Tr(msg))
#define TR_STRQ(msg) QString::fromStdString(Tr((msg).toStdString()))

// ---------------------------------------------------------------------------
// 键合命令参数定义表（本模块自持，11 条命令）
//
// 表中 key / 编辑器类型 / 选项 key 全部是字面量：key 逐字对应
// config/gui_config/Languages/Language.csv 的词条 key，也是 module/re 内参数 key 的唯一来源。
// 表内不含任何译文——表头与下拉项显示一律在"建行/建编辑器"时用 Tr(key) 现取。
// 命令顺序 = 工序表格的显示（列）顺序；每个命令内的参数顺序 = 处方 S_BondStep::params 的位置序。
// Check 参数的选项恒为 [0] = 勾选 / [1] = 未勾选（bondingParamCheckedText/UncheckedText 依赖该顺序）。
// ---------------------------------------------------------------------------
static const QMap<QString, QList<BondingParamItem>> &bondingParamDefTable()
{
    static const QMap<QString, QList<BondingParamItem>> s_table = []()
    {
        auto spin = [](const char *key)
        {
            BondingParamItem it;
            it.key = QString::fromUtf8(key);
            it.type = QStringLiteral("spin");
            return it;
        };
        auto combo = [](const char *key, const QStringList &optionKeys)
        {
            BondingParamItem it;
            it.key = QString::fromUtf8(key);
            it.type = QStringLiteral("combo");
            it.options = optionKeys;
            return it;
        };
        auto check = [](const char *key, const QStringList &optionKeys)
        {
            BondingParamItem it;
            it.key = QString::fromUtf8(key);
            it.type = QStringLiteral("check");
            it.options = optionKeys;
            return it;
        };

        QMap<QString, QList<BondingParamItem>> t;

        // 力控制（5）
        t[QStringLiteral("ManualForceCtl")] = QList<BondingParamItem>()
                << spin("bond_TargetForceN")
                << spin("bond_SpeedMmS")
                << spin("bond_SteadyTimeS")
                << spin("bond_BondInitialPositionMm")
                << check("bond_BondInitialPosition",
                         QStringList() << "bond_BondInitialPosition_Checked" << "bond_BondInitialPosition_UnChecked");

        // 等待键合力到达（5）
        t[QStringLiteral("WaitForManualForce")] = QList<BondingParamItem>()
                << spin("bond_TargetForceN")
                << spin("bond_MinForceN")
                << spin("bond_MaxForceN")
                << spin("bond_TimeoutS")
                << check("bond_DisableForceControl",
                         QStringList() << "bond_DisableForceControl_Checked" << "bond_DisableForceControl_UnChecked");

        // 温度控制（4，无"下气冷"——无下冷硬件）
        t[QStringLiteral("TempCtl")] = QList<BondingParamItem>()
                << combo("bond_BondHeadHeatingSelection",
                         QStringList() << "bond_BondHeadHeatingSelection_Up"
                                       << "bond_BondHeadHeatingSelection_Down"
                                       << "bond_BondHeadHeatingSelection_UpAndDown")
                << spin("bond_TargetTemperatureC")
                << spin("bond_SteadyTimeS")
                << check("bond_UpAirCooling",
                         QStringList() << "bond_UpAirCooling_Checked" << "bond_UpAirCooling_UnChecked");

        // 等待温度到达（5）
        t[QStringLiteral("WaitForTemp")] = QList<BondingParamItem>()
                << combo("bond_BondHead",
                         QStringList() << "bond_BondHead_Up" << "bond_BondHead_Down" << "bond_BondHead_UpAndDown")
                << spin("bond_TargetTemperatureC")
                << spin("bond_MinTemperatureC")
                << spin("bond_MaxTemperatureC")
                << spin("bond_TimeoutS");

        // 保持时间（1）
        t[QStringLiteral("HoldTime")] = QList<BondingParamItem>()
                << spin("bond_TimeoutS");

        // 甲酸（1）
        t[QStringLiteral("FormicAcid")] = QList<BondingParamItem>()
                << combo("bond_FormicAcid", QStringList() << "bond_FormicAcid_On" << "bond_FormicAcid_Off");

        // 位置控制模式切换（5）
        t[QStringLiteral("PosCtlModeTrigger")] = QList<BondingParamItem>()
                << spin("bond_ZeroTemperatureC")
                << spin("bond_CollapsePositionMm")
                << spin("bond_TcbRateMmS")
                << spin("bond_TimeoutS")
                << check("bond_KeepTempAfterCollapse",
                         QStringList() << "bond_KeepTempAfterCollapse_Checked" << "bond_KeepTempAfterCollapse_UnChecked");

        // 恒温位置控制（7）
        t[QStringLiteral("ThermostaticPosCtl")] = QList<BondingParamItem>()
                << spin("bond_TargetPositionMm")
                << spin("bond_MinPositionDeviationMm")
                << spin("bond_MaxPositionDeviationMm")
                << spin("bond_TimeoutS")
                << spin("bond_C1UpSpeedMmS")
                << spin("bond_C1DownSpeedMmS")
                << check("bond_DisablePositionControl",
                         QStringList() << "bond_DisablePositionControl_Checked" << "bond_DisablePositionControl_UnChecked");

        // 冷却位置控制（3）
        t[QStringLiteral("CoolingPosCtl")] = QList<BondingParamItem>()
                << spin("bond_TargetPositionMm")
                << spin("bond_C2UpThreshold")
                << spin("bond_C2DownThreshold");

        // 上吸嘴破真空（1）
        t[QStringLiteral("UpSuckerBreakVac")] = QList<BondingParamItem>()
                << combo("bond_BreakVacuumWay",
                         QStringList() << "bond_BreakVacuumWay_OpenPositive" << "bond_BreakVacuumWay_OpenAir");

        // 键合臂相对移动（1）
        t[QStringLiteral("BondArmRelativeMove")] = QList<BondingParamItem>()
                << spin("bond_BondArmPositionMm");

        return t;
    }();
    return s_table;
}

// check 类参数的处方取值：选项 key 固定 [0]=勾选 / [1]=未勾选。
// 与旧实现不同——不再是全局固定的 "bond_Checked"/"bond_Unchecked"，
// 每个 Check 参数用自己的选项 key（如 bond_BondInitialPosition_Checked）。
QString bondingParamCheckedText(const QStringList &options)
{
    return options.value(0);
}

QString bondingParamUncheckedText(const QStringList &options)
{
    return options.value(1);
}

// check 值判定：命中勾选项 key 即为勾选（无 legacy 猜测）
bool bondingParamValueIsChecked(const QStringList &options, const QString &value)
{
    return !options.isEmpty() && value == options.value(0);
}

// combo/check 的处方值 → 规范选项 key
QString bondingParamOptionKey(const BondingParamItem &it, const QString &value)
{
    if (it.options.contains(value))
    {
        return value;
    }
    if (it.type == "check")
    {
        return bondingParamUncheckedText(it.options);
    }
    // combo 的空值/失配：回落首项，保证新工序有合法取值（不再把 "0"/"1"/"2" 当序号解释）
    return it.options.isEmpty() ? value : it.options.first();
}

// 处方值 → 单元格显示文本（显示 = Tr(key) 译文，值 = 规范 key）
QString bondingParamValueToText(const BondingParamItem &it, const QString &value)
{
    if (it.type != "combo")
    {
        return value;   // spin 的数字文本原样；check 由勾选框呈现
    }
    return TR_STRQ(bondingParamOptionKey(it, value));
}

// 根据命令名查本地参数定义表（未知命令返回空表）。
// 返回的是进程内静态表的引用（表内只有 key，无译文），语言切换无需重取缓存。
const QList<BondingParamItem> &bondingParamDefs(const QString &rawCmd)
{
    static const QList<BondingParamItem> s_empty;

    const QMap<QString, QList<BondingParamItem>> &table = bondingParamDefTable();
    const auto found = table.find(rawCmd);
    return found == table.end() ? s_empty : found.value();
}

BondingParamDelegate::BondingParamDelegate(QTableWidget *table, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_table(table)
{
}

bool BondingParamDelegate::paramAt(const QModelIndex &index, QTableWidget *table, BondingParamItem *out)
{
    if (!table || index.column() <= 0)
    {
        return false;
    }
    QTableWidgetItem *row0 = table->item(index.row(), 0);
    if (!row0 || row0->data(Qt::UserRole).toString() == "header")
    {
        return false;
    }
    QString rawCmd = row0->data(Qt::UserRole).toString();
    if (rawCmd.isEmpty())
    {
        return false;
    }
    int paramIdx = index.column() - 1;
    const QList<BondingParamItem> &items = bondingParamDefs(rawCmd);
    if (paramIdx >= items.size())
    {
        return false;
    }
    if (out)
    {
        *out = items[paramIdx];
    }
    return true;
}

QWidget *BondingParamDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    BondingParamItem it;
    if (!paramAt(index, m_table, &it) || it.type == "check")
    {
        return nullptr;   // check 直接在单元格上勾选，无编辑器
    }
    if (it.type == "combo")
    {
        QComboBox *cb = new QComboBox(parent);
        // 显示 Tr(选项 key)，userData 存选项 key：落盘取 userData，绝不取显示译文
        for (const QString &optKey : it.options)
        {
            cb->addItem(TR_STRQ(optKey), optKey);
        }
        cb->setStyleSheet(iStarUIStyleSheet->ComboBox);
        return cb;
    }
    QDoubleSpinBox *sb = new QDoubleSpinBox(parent);
    sb->setDecimals(6);
    sb->setRange(-999999.0, 999999.0);
    // 去掉右侧上/下箭头
    sb->setStyleSheet(iStarUIStyleSheet->DoubleSpinBox_Dark +
                      "QDoubleSpinBox::up-button{width:0px;border:none;}"
                      "QDoubleSpinBox::down-button{width:0px;border:none;}");
    return sb;
}

void BondingParamDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    BondingParamItem it;
    if (!paramAt(index, m_table, &it))
    {
        return;
    }
    if (it.type == "combo")
    {
        if (QComboBox *cb = qobject_cast<QComboBox *>(editor))
        {
            // 回填依据是单元格数据角色里的选项 key（显示文本是译文，不能用来反查）
            const QTableWidgetItem *cell = m_table ? m_table->item(index.row(), index.column()) : nullptr;
            const QString valueKey = cell ? cell->data(Qt::UserRole + 3).toString() : QString();
            const int i = cb->findData(valueKey);
            cb->setCurrentIndex(i >= 0 ? i : 0);
        }
    }
    else if (QDoubleSpinBox *sb = qobject_cast<QDoubleSpinBox *>(editor))
    {
        sb->setValue(index.data().toString().toDouble());
    }
}

void BondingParamDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    BondingParamItem it;
    if (!paramAt(index, m_table, &it))
    {
        return;
    }
    QTableWidgetItem *cell = m_table ? m_table->item(index.row(), index.column()) : nullptr;
    if (it.type == "combo")
    {
        if (QComboBox *cb = qobject_cast<QComboBox *>(editor))
        {
            // 落盘值是选项 key（userData）；同时刷新 UserRole+3 与显示译文，
            // 保证"改语言后保存"仍然写回规范 key（往返闭环）
            const QString key = cb->currentData().toString();
            if (cell)
            {
                cell->setData(Qt::UserRole + 3, key);
                cell->setText(TR_STRQ(key));
            }
            else
            {
                model->setData(index, key, Qt::UserRole + 3);
                model->setData(index, TR_STRQ(key));
            }
        }
    }
    else if (QDoubleSpinBox *sb = qobject_cast<QDoubleSpinBox *>(editor))
    {
        const QString text = QString::number(sb->value(), 'g', 10);
        if (cell)
        {
            cell->setText(text);
        }
        else
        {
            model->setData(index, text);
        }
    }
}
