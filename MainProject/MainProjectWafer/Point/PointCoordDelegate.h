#ifndef POINTCOORDDELEGATE_H
#define POINTCOORDDELEGATE_H

#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QRegularExpressionValidator>

// 点位表坐标列(第0/1列 x/y)编辑器：QLineEdit + 数字校验，字母无法输入；
// 其余列保持默认编辑器。header-only，无 Q_OBJECT，不需要 moc。
class PointCoordDelegate : public QStyledItemDelegate
{
public:
    explicit PointCoordDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        if (index.column() > 1)
        {
            return QStyledItemDelegate::createEditor(parent, option, index);
        }
        QLineEdit *ed = new QLineEdit(parent);
        ed->setValidator(new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral("[-+]?[0-9]*\\.?[0-9]*([eE][-+]?[0-9]+)?")), ed));
        return ed;
    }
};

#endif // POINTCOORDDELEGATE_H
