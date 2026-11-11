#ifndef WAFERMATERIALTAB_H
#define WAFERMATERIALTAB_H

#include <functional>
#include <QStringList>

class QWidget;

namespace Ui { class RecipeModule; }
struct S_RecipeWaferMaterial;

// 物料信息（materialInfo）页签控制器：
// 本页控件（单芯片参数/蓝膜晶圆参数/排布方式/chip 处方下拉/预览按钮/gv_preview）的
// 配置、连接与 S_RecipeWaferMaterial 数据读写收拢在此。
// chip 处方列表来自模块的 ChipWaferSync（setChipRecipeNames 推送），切换 chip/点预览时
// 由模块回调（onChipRecipeChanged/onPreviewClicked）触发跨页签的排布重算。
class MaterialTab
{
public:
    MaterialTab(Ui::RecipeModule *ui, QWidget *dialogParent,
                std::function<void()> onPreviewClicked,
                std::function<void()> onChipRecipeChanged);

    void setConnect();

    // 处方加载：物料数据 → 本页控件（原 UpdateUI 物料段；chipRecipe 引用与名义坐标由模块另行处理）
    void load(const S_RecipeWaferMaterial &material);
    // 保存读取：本页控件 → 物料数据（chip 尺寸/名义坐标由模块在 save 后按布局状态回填）
    void save(S_RecipeWaferMaterial &material) const;

    // 重建 chip 处方下拉（模块 ChipWaferSync chip 列表变化时推送；prefer 用于恢复上次选中）
    void setChipRecipeNames(const QStringList &releasedNames, const QString &prefer);
    // 当前选中的 chip 处方名（模块保存时解析 chip 尺寸用）
    QString currentChipRecipeName() const;

private:
    Ui::RecipeModule *m_ui = nullptr;
    QWidget *m_dialogParent = nullptr;
    std::function<void()> m_onPreviewClicked;
    std::function<void()> m_onChipRecipeChanged;

    int m_lastMaterialEditMode = 0;   // 加载时的物料编辑模式（均未勾选时保存沿用原值）
};

#endif // WAFERMATERIALTAB_H
