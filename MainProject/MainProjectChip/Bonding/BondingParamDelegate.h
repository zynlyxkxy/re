#ifndef BONDINGPARAMDELEGATE_H
#define BONDINGPARAMDELEGATE_H

#include <QStyledItemDelegate>
#include <QStringList>
#include <QList>

class QTableWidget;

// 键合参数项定义：参数 key / 编辑器类型 / 选项 key 由本模块自持的参数定义表给出
// （见 BondingParamDelegate.cpp 的 bondingParamDefTable），不依赖任何跨模块 key 契约头。
// 注意：这里刻意不保存表头/选项的译文——显示文本一律在"建行"时用 Tr(key) 现取，
//      否则语言切换后静态缓存不会刷新。
struct BondingParamItem
{
    QString key;         // 参数规范 key（定义表里的字面量，原样存入处方 S_BondParam.key）
    QString type;        // 编辑器类型 "combo" / "spin" / "check"
    QStringList options; // combo/check 的规范选项 key 列表（Numeric 为空）
};

// check 类参数的处方取值 = 定义表里的两个选项 key（options[0] = 勾选，options[1] = 未勾选）。
// 名字沿用历史命名，返回值是 key 而非显示文案。
QString bondingParamCheckedText(const QStringList &options);
QString bondingParamUncheckedText(const QStringList &options);

// 处方值（S_BondParam::value）→ 单元格显示文本：
//   spin ：值即数字文本，原样显示
//   combo：值是选项 key，显示为 Tr(key)（译文只用于显示，绝不落盘）
//   check：原样返回（勾选状态由单元格 checkState 呈现）
QString bondingParamValueToText(const BondingParamItem &it, const QString &value);

// combo/check 的处方值归一化为定义表里的选项 key：
//   combo：命中 options 原样返回；空值或失配回落到首项（保证新工序有合法值）
//   check：命中 options[0] 视为勾选，否则取 options[1]
QString bondingParamOptionKey(const BondingParamItem &it, const QString &value);

// check 值判定：处方值命中勾选项 key（options[0]）即为勾选。
// 不再做 yes/no/true/false/1 之类的猜测——协议值是 key，失配就是失配。
bool bondingParamValueIsChecked(const QStringList &options, const QString &value);

// 根据命令名返回其参数定义（查本模块自持定义表，表内只有 key，无译文）
const QList<BondingParamItem> &bondingParamDefs(const QString &rawCmd);

// 键合参数单元格编辑器：combo → QComboBox（显示 Tr(option key)，userData 存 option key），
// spin → QDoubleSpinBox，check → 无编辑器（直接在单元格上勾选）
class BondingParamDelegate : public QStyledItemDelegate
{
public:
    explicit BondingParamDelegate(QTableWidget *table, QObject *parent = nullptr);

    static bool paramAt(const QModelIndex &index, QTableWidget *table, BondingParamItem *out = nullptr);

protected:
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

private:
    QTableWidget *m_table = nullptr;
};

#endif // BONDINGPARAMDELEGATE_H
