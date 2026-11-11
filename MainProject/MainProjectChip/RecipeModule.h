#ifndef RECIPEMODULE_H
#define RECIPEMODULE_H

#include "IREModule.h"
#include "ReData/S_Recipe.h"
#include <QPair>
#include <QStringList>
#include <QVector>
#include <QWidget>

class MaterialTab;
class LevelTab;
class AlignTab;
class BondingTab;

namespace Ui { class RecipeModule; }

// RecipeModule 只保留通用职责：
//   - IREModule 接口 / Edit·Save / 编辑态全局切换 / 数据在 处方结构体 ↔ 页签 间的调度
//   - 通用样式、语言重放（含 GroupBox 标题清单、控件通用 mark 循环）
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
    void UpdateUI(const S_ChipRecipe& recipe);
    S_ChipRecipe GetUIInfo();

    void setConnections();
    void markDataModified();

    QWidget *m_mainWidget = nullptr;
    Ui::RecipeModule *ui = nullptr;

    QString m_currentRecipe;
    bool m_editable = false;
    bool m_dataModified = false;

    // 页签控制器（各页签控件配置/连接/load/save 见 Tabs/*.cpp）
    MaterialTab *m_materialTab = nullptr;
    LevelTab *m_levelTab = nullptr;
    AlignTab *m_alignTab = nullptr;
    BondingTab *m_bondingTab = nullptr;

    // ── 语言切换重放：静态控件/页签/下拉/表头文本以英文 key 捕获，刷新时按 TR_STR 重放 ──
    struct LangEntry { QWidget *widget = nullptr; int index = -1; QString key; };
    bool m_langReady = false;
    QVector<QPair<QWidget *, QString>> m_langWidgets;
    QVector<LangEntry> m_langTabs;
    QVector<LangEntry> m_langCombos;
    QVector<LangEntry> m_langHeaders;
};

#endif
