#ifndef CONTINUOUSTAB_H
#define CONTINUOUSTAB_H

#include <array>
#include <functional>

class QDoubleSpinBox;

namespace Ui { class RecipeModule; }
struct S_RecipeWaferContinuous;

// 连续键合（Continuous，页签 tab_2）控制器：
// 调平台下压高度/键合次数、键合前后识别模板表(tw_bondRecognitionTemplate)、
// Point 1~4 的 X/Y 补偿偏移 与 S_RecipeWaferContinuous 的数据读写收拢在此。
// 本页无图形视图与页内按钮，输入控件的“已修改”标记由 RecipeModule 的通用循环统一处理。
class ContinuousTab
{
public:
    ContinuousTab(Ui::RecipeModule *ui, std::function<void()> onModified);

    void setConnect();   // 本页无额外连接，保留统一接口
    // 处方加载（UpdateUI 连续键合段）
    void load(const S_RecipeWaferContinuous &continuous);
    // 保存读取（GetUIInfo 连续键合段）
    void save(S_RecipeWaferContinuous &continuous) const;

private:
    // 取第 index(0~3) 个点的 4 个输入框：xOffsetX, xOffsetY, yOffsetX, yOffsetY
    // （点号越界时返回全空）
    std::array<QDoubleSpinBox *, 4> offsetSpinBoxes(int index) const;

    Ui::RecipeModule *m_ui = nullptr;
    std::function<void()> m_onModified;
};

#endif // CONTINUOUSTAB_H
