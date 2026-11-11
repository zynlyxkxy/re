#ifndef CHIPWAFERSYNC_H
#define CHIPWAFERSYNC_H

#include "ReData/S_Recipe.h"
#include <QObject>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <functional>

class QTimer;
class RM_Client_Inherit;
struct StringList;

// ══ Chip/Wafer 处方 5s 轮询同步 ══
//
// 职责：
//  - 缓存所有 chip 处方规格 {uid, released, chipWidth, chipHeight}（chipCache）
//  - 缓存所有已保存 wafer 处方的解析结构体（waferCache），chip 更新时直接改结构体
//  - 每 5s 轮询 chip 目录+header，比对 uid：有更新则拉 body 提取新规格，
//    对引用它的 wafer：重算点位、清键合点，自动保存；当前正在查看的 wafer 只刷 UI 不自动保存
//  - 下拉框数据源（released chip 名单）由 RecipeModule 经 chips() 查询
//
// 所有网络回调都切回主线程处理（QMetaObject::invokeMethod），
// 因此 onChipListChanged / onCurrentWaferUpdated 回调在主线程执行，可直接操作 UI。
class ChipWaferSync : public QObject
{
    Q_OBJECT
public:
    explicit ChipWaferSync(QObject *parent = nullptr);
    ~ChipWaferSync() override;

    // chip 处方缓存项（只存对账与取值所需的标量，不缓存 body）
    struct ChipEntry {
        std::string uid;
        bool released = false;
        double chipWidth = 0.0;
        double chipHeight = 0.0;
        std::vector<S_Point2D> downAlignPoints;   // chip 处方 align.lst_downAlignPoints
    };

    // wafer 处方缓存项（解析后的结构体：直接改字段 → 重算 → 打包保存）
    struct WaferEntry {
        std::string chipName;   // 引用的 chip 处方名（快照来源）
        std::string chipUid;    // 上次同步时 chip 的 uid，用于对账（不一致且 released 才更新）
        S_RecipeContentWafer recipe;
    };

    void start();   // 首次全量填充 + 启动 5s 轮询
    void stop();

    // ── 供 RecipeModule 查询 ──
    const std::map<std::string, ChipEntry>& chips() const { return m_chips; }
    bool chipDims(const std::string &chipName, double &w, double &h) const;
    // 取 chip 处方的下对位点坐标；chip 不在缓存 → false（⚠ 空列表 ≠ 取数失败）
    bool chipAlignPoints(const std::string &chipName, std::vector<S_Point2D> &out) const;

    // ── 当前正在查看/编辑的 wafer（自动更新时只刷 UI，不自动保存）──
    void setCurrentWafer(const std::string &name) { m_currentWafer = name; }

    // ── 回调（主线程）──
    std::function<void()> onChipListChanged;   // chip 列表/规格变化 → 重建下拉框
    std::function<void(const S_RecipeContentWafer &)> onCurrentWaferUpdated; // 当前 wafer 被自动更新 → 刷新 UI

private:
    // chip 处方在 "rm" 模块目录，wafer 处方在 "rm2" 模块目录（与 chip/wafer 进程一致）
    RM_Client_Inherit *chipClient();
    RM_Client_Inherit *waferClient();

    void initialFill();
    void tick();
    // tick 分步：chip 对账（rm 目录）→ wafer 目录同步（rm2 目录）→ 更新引用 wafer
    void chipDirectoryStep();
    void chipHeaderStep(const StringList &names);
    void chipBodyStep(const std::set<std::string> &changed, const std::vector<std::string> &needBody);
    void waferDirectoryStep(const std::set<std::string> &changed);
    void waferBodyStep(const std::set<std::string> &changed, const std::vector<std::string> &newWafers);
    void finishTick(const std::set<std::string> &changed);

    void updateWafersReferencing(const std::string &chipName);
    void recomputeLayout(S_RecipeContentWafer &recipe);
    bool saveWafer(const std::string &waferName, WaferEntry &entry);
    void notifyChipListChanged();

    QTimer *m_timer = nullptr;
    bool m_initialized = false;
    bool m_inFlight = false;
    std::map<std::string, ChipEntry> m_chips;
    std::map<std::string, WaferEntry> m_wafers;
    std::string m_currentWafer;
    std::vector<std::string> m_lastReleasedChips;   // 上次通知 UI 时的 released chip 名单（去重通知）
};

#endif // CHIPWAFERSYNC_H
