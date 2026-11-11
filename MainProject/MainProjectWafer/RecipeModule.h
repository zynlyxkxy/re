#ifndef RECIPEMODULE_H
#define RECIPEMODULE_H

#include "IREModule.h"
#include "ReData/S_Recipe.h"
#include "ChipWaferSync.h"
#include <QPair>
#include <QStringList>
#include <QVector>
#include <QWidget>

class MaterialTab;
class WaferLevelTab;
class ChuckLevelTab;
class ChipCalibrationTab;
class BondPointTab;
class OperationModeTab;
class ContinuousTab;

namespace Ui { class RecipeModule; }

// RecipeModule 只保留通用职责：
//   - IREModule 接口 / Edit·Save / 全局编辑态切换 / 数据在 处方结构体 ↔ 页签 间的调度
//   - Chip/Wafer 处方同步、晶圆排布计算与预览分发（跨页签共享）
//   - 通用样式、语言重放（含 GroupBox 标题清单）、控件通用 mark 循环
// 各页签的具体控件配置、连接、数据 load/save 收拢在 Tabs/*Tab.{h,cpp}。
class RecipeModule : public IREModule
{
public:
    RecipeModule();
    ~RecipeModule() override;

    QWidget* getWidget() override;
    void setCurrentRecipe(const QString& name) override;
    void refreshLanguage() override;

private:
    void applyLanguage();
    void onEditClicked();
    void onSaveClicked();
    void setEditable(bool editable);
    void UpdateUI(const S_RecipeContentWafer& recipe);
    S_RecipeContentWafer GetUIInfo();

    void setConnections();
    void markDataModified();
    void rebuildChipCombo();
    void regenerateLayout();
    void applyLayoutParams();
    void refreshPreview(double radius, int notchAngle);

    bool queryChipMarkPositions(std::vector<S_Point2D> &out) const;   // 纯查询：当前 chip 的下对位点
    void refreshChipMarkPositions();  // 把查询结果落到 m_chipMarkPosition(更新芯片标记名义坐标)
    std::vector<S_Point2D> allChipMarkNominalPoints() const;

    QWidget *m_mainWidget = nullptr;
    Ui::RecipeModule *ui = nullptr;

    QString m_currentRecipe;
    bool m_editable = false;
    bool m_dataModified = false;

    // Chip/Wafer 处方同步（5s 轮询缓存 chip 规格 + 已保存 wafer 结构体，chip 更新自动跟随）
    ChipWaferSync *m_sync = nullptr;
    bool m_loading = false;          // UpdateUI 填充控件期间抑制 combo 切换重算
    std::string m_loadedChipName;    // 当前加载处方引用的 chip 名（combo 恢复选中用）

    // 页签控制器（各页签控件配置/连接/load/save 见 Tabs/*.cpp）
    MaterialTab *m_materialTab = nullptr;
    WaferLevelTab *m_levelTab = nullptr;
    ChuckLevelTab *m_chuckTab = nullptr;
    ChipCalibrationTab *m_chipCalTab = nullptr;
    BondPointTab *m_bondTab = nullptr;
    OperationModeTab *m_opModeTab = nullptr;
    ContinuousTab *m_continuousTab = nullptr;

    // 芯片排布数据（跨页签共享：由模块计算并分发给各页签视图）
    QList<QPointF> m_chipPositions;
    double m_chipWidth = 0.0;
    double m_chipHeight = 0.0;
    std::vector<S_Point2D> m_chipMarkPositions;

    // ── 语言切换重放：静态控件/页签/下拉/表头文本以英文 key 捕获，刷新时按 TR_STR 重放 ──
    struct LangEntry { QWidget *widget = nullptr; int index = -1; QString key; };
    bool m_langReady = false;
    QVector<QPair<QWidget *, QString>> m_langWidgets;
    QVector<LangEntry> m_langTabs;
    QVector<LangEntry> m_langCombos;
    QVector<LangEntry> m_langHeaders;
};

#endif
