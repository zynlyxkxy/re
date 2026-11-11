#ifndef CHIPCALIBRATIONTAB_H
#define CHIPCALIBRATIONTAB_H

#include <functional>
#include <vector>
#include "ReData/S_Recipe.h"


class QWidget;

namespace Ui { class RecipeModule; }
struct S_NominalPoint;
struct S_RecipeWaferChipCalibration;
class ChipCalibrationController;

// Chip 标定（tab）页签控制器：Add→图上拾取→Confirm 交互统一由 ChipCalibrationController 实现，
// 本类负责：控制器创建/数据回调、处方数据进出（load/save）、编辑态与布局重算联动。
class ChipCalibrationTab
{
public:
    ChipCalibrationTab(Ui::RecipeModule *ui, QWidget *dialogParent,
                       std::function<void()> onModified);

    void setConnect();   // 本页无额外连接（控制器内部自连），保留统一接口
    // 编辑态：坐标表格只读 + 控制器按钮状态机
    void setEditable(bool editable);
    // 处方加载（原 UpdateUI 标定段：清空后按数据重建行与锁定圆点）
    void load(const std::vector<S_NominalPoint> &points);
    // 保存读取（原 GetUIInfo 标定段：先丢弃未确认放置再收集）
    void save(S_RecipeWaferChipCalibration &chipCal) const;
    // 排布重算后旧标定点失效：清空（模块 applyLayoutParams 调用）
    void resetOnLayoutChange();
    // 是否为“进行中放置态”（供模块判断是否需要先中断）
    bool isPlacing() const;
    // 下发当前 chip 的全部标记名义坐标：作为 Chip 标定的候选点（Add 后图上显示的虚色点）。
    // 不再本地缓存，直接转交控制器；chip 切换/排布重算后由 RecipeModule 重新下发。
    void setDownMarkPoints(const std::vector<S_Point2D> &points);

private:
    Ui::RecipeModule *m_ui = nullptr;
    QWidget *m_dialogParent = nullptr;
    std::function<void()> m_onModified;
    ChipCalibrationController *m_controller = nullptr;
};

#endif // CHIPCALIBRATIONTAB_H
