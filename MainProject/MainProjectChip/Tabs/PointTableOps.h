#ifndef POINTTABLEOPS_H
#define POINTTABLEOPS_H

#include <functional>

class QTableWidget;
class QPushButton;

// 点位表行操作（增/删/上移/下移）及按钮绑定；由多个页签共用（Level/Align）。
// 上移/下移会完整搬移单元格 item 与 cellWidget；每次操作后回调 onModified 标记数据已修改。
namespace PointTableOps {

void addRow(QTableWidget *tw);
void deleteRow(QTableWidget *tw);
void moveRowUp(QTableWidget *tw);
void moveRowDown(QTableWidget *tw);

// 把 add/delete/up/down 四个按钮绑定到 tw 的行操作（onModified 可为空）
void bindRowButtons(QTableWidget *tw, QPushButton *add, QPushButton *del,
                    QPushButton *up, QPushButton *dw,
                    const std::function<void()> &onModified = std::function<void()>());

} // namespace PointTableOps

#endif // POINTTABLEOPS_H
