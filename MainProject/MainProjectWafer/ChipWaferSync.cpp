#include "ChipWaferSync.h"

#include "rm_guiclient.h"
#include "rm_client.h"
#include "rm_define.h"
#include "istar_rpc_protocol_types.h"
#include "istar_serializer.hpp"

#include "Algorithm/EAGAlgorithm/EgaFit.h"

#include <QTimer>
#include <QCoreApplication>
#include <QDebug>
#include <set>

// ══ 本地镜像 Chip 处方的最小结构：只读 material.dwChipMaterialInfo ══
// （字段名与 Chip 侧 JSON key 严格一致，否则 iguana 匹配不上）
namespace {
struct S_DwChipMatRef {
    double chipWidth = 0.0;
    double chipHeight = 0.0;
    YLT_REFL(S_DwChipMatRef, chipWidth, chipHeight);
};
struct S_MaterialRef {
    S_DwChipMatRef dwChipMaterialInfo;
    YLT_REFL(S_MaterialRef, dwChipMaterialInfo);
};
struct S_DownAlignPointRef {
    S_Point2D point;
    YLT_REFL(S_DownAlignPointRef, point);
};
struct S_ChipAlignRef {
    std::vector<S_DownAlignPointRef> lst_downAlignPoints;
    YLT_REFL(S_ChipAlignRef, lst_downAlignPoints);
};

struct S_ChipRecipeRef {
    S_MaterialRef material;
    S_ChipAlignRef align;
    YLT_REFL(S_ChipRecipeRef, material, align);
};

std::vector<S_Point2D> toPoints(const std::vector<S_DownAlignPointRef> &src)
{
    std::vector<S_Point2D> out;
    out.reserve(src.size());
    for (const auto &p : src)
        out.push_back(p.point);
    return out;
}
} // namespace

ChipWaferSync::ChipWaferSync(QObject *parent)
    : QObject(parent)
{
}

ChipWaferSync::~ChipWaferSync()
{
    stop();
}

// chip 处方在 "rm" 模块目录（与 chip 进程一致）；wafer 处方在 "rm2" 模块目录。
// 客户端由 GetRMClient 返回的 shared_ptr 持有于 RM_GUIClient 内部，返回裸指针调用安全。
RM_Client_Inherit *ChipWaferSync::chipClient()
{
    auto client = RM_GUIClient::GetInstance().GetRMClient("rm");
    if (!client)
    {
        RM_GUIClient::GetInstance().Communication_Init("rm");
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, this, []() {
            RM_GUIClient::GetInstance().Communication_Exit("rm");
        });
        client = RM_GUIClient::GetInstance().GetRMClient("rm");
    }
    return client.get();
}

RM_Client_Inherit *ChipWaferSync::waferClient()
{
    auto client = RM_GUIClient::GetInstance().GetRMClient("rm2");
    if (!client)
    {
        RM_GUIClient::GetInstance().Communication_Init("rm2");
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, this, []() {
            RM_GUIClient::GetInstance().Communication_Exit("rm2");
        });
        client = RM_GUIClient::GetInstance().GetRMClient("rm2");
    }
    return client.get();
}

void ChipWaferSync::start()
{
    if (m_timer)
        return;
    initialFill();
    m_timer = new QTimer(this);
    m_timer->setInterval(3000);
    connect(m_timer, &QTimer::timeout, this, [this]() { tick(); });
    m_timer->start();
}

void ChipWaferSync::stop()
{
    if (m_timer)
    {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }
}

bool ChipWaferSync::chipDims(const std::string &chipName, double &w, double &h) const
{
    auto it = m_chips.find(chipName);
    if (it == m_chips.end())
        return false;
    w = it->second.chipWidth;
    h = it->second.chipHeight;
    return true;
}

bool ChipWaferSync::chipAlignPoints(const std::string &chipName, std::vector<S_Point2D> &out) const
{
    auto it = m_chips.find(chipName);
    if (it == m_chips.end())
        return false;
    out = it->second.downAlignPoints;
    return true;
}

void ChipWaferSync::notifyChipListChanged()
{
    // released chip 名单变化才通知 UI（避免每轮重建下拉框打断用户操作）
    std::vector<std::string> cur;
    for (const auto &kv : m_chips)
        if (kv.second.released)
            cur.push_back(kv.first);
    if (cur == m_lastReleasedChips)
        return;
    m_lastReleasedChips = std::move(cur);
    if (onChipListChanged)
        onChipListChanged();
}

// ══ 首次填充：chip（"rm" 目录）+ wafer（"rm2" 目录）全量缓存 ══
void ChipWaferSync::initialFill()
{
    // ── chip 处方全量缓存 ──
    if (RM_Client_Inherit *c = chipClient())
    {
        c->RMGetRcpDirectoryAsync([this](ResponseMessageStruct<StringList> &dirResponse) {
            StringList names = dirResponse.data;
            auto client = RM_GUIClient::GetInstance().GetRMClient("rm");
            if (!client)
                return ISTAR_RESULT_SUCCESS;
            if (names.items.empty())
            {
                QMetaObject::invokeMethod(this, [this]() {
                    m_initialized = true;
                    notifyChipListChanged();
                }, Qt::QueuedConnection);
                return ISTAR_RESULT_SUCCESS;
            }
            client->RMGetRcpAsync(names, [this](ResponseMessageStruct<RcpList> &rcpResponse) {
                RcpList rcps = rcpResponse.data;
                QMetaObject::invokeMethod(this, [this, rcps]() {
                    m_chips.clear();
                    for (const auto &r : rcps.items)
                    {
                        try
                        {
                            S_ChipRecipeRef ref = Serializer::from_rawJson<S_ChipRecipeRef>(r.body);
                            ChipEntry e;
                            e.uid = r.header.uid;
                            e.released = r.header.released;
                            e.chipWidth = ref.material.dwChipMaterialInfo.chipWidth;
                            e.chipHeight = ref.material.dwChipMaterialInfo.chipHeight;
                            e.downAlignPoints = toPoints(ref.align.lst_downAlignPoints);
                            m_chips[r.header.name] = std::move(e);
                        }
                        catch (const std::exception &)
                        {
                        }
                    }
                    m_initialized = true;
                    notifyChipListChanged();
                }, Qt::QueuedConnection);
                return ISTAR_RESULT_SUCCESS;
            });
            return ISTAR_RESULT_SUCCESS;
        });
    }

    // ── wafer 处方全量缓存 ──
    if (RM_Client_Inherit *w = waferClient())
    {
        w->RMGetRcpDirectoryAsync([this](ResponseMessageStruct<StringList> &dirResponse) {
            StringList names = dirResponse.data;
            auto client = RM_GUIClient::GetInstance().GetRMClient("rm2");
            if (!client)
                return ISTAR_RESULT_SUCCESS;
            if (names.items.empty())
                return ISTAR_RESULT_SUCCESS;
            client->RMGetRcpAsync(names, [this](ResponseMessageStruct<RcpList> &rcpResponse) {
                RcpList rcps = rcpResponse.data;
                QMetaObject::invokeMethod(this, [this, rcps]() {
                    m_wafers.clear();
                    for (const auto &r : rcps.items)
                    {
                        try
                        {
                            S_RecipeContentWafer recipe = Serializer::from_rawJson<S_RecipeContentWafer>(r.body);
                            const auto &oneChip = recipe.material.oneChipTypeInfo;
                            const std::string chipName = oneChip.chipRecipe.chipRecipeName;
                            if (chipName.empty())
                                continue;
                            WaferEntry e;
                            e.chipName = chipName;
                            e.recipe = std::move(recipe);
                            auto it = m_chips.find(chipName);
                            if (it != m_chips.end())
                                e.chipUid = it->second.uid;
                            m_wafers[r.header.name] = std::move(e);
                        }
                        catch (const std::exception &)
                        {
                        }
                    }
                }, Qt::QueuedConnection);
                return ISTAR_RESULT_SUCCESS;
            });
            return ISTAR_RESULT_SUCCESS;
        });
    }
}

// ══ 每 5s 轮询：chip 对账（rm 目录，uid 变化才拉 body）→ wafer 目录同步（rm2 目录）══
void ChipWaferSync::tick()
{
    if (m_inFlight || !m_initialized)
        return;
    m_inFlight = true;
    chipDirectoryStep();
}

void ChipWaferSync::chipDirectoryStep()
{
    RM_Client_Inherit *c = chipClient();
    if (!c)
    {
        m_inFlight = false;
        return;
    }
    c->RMGetRcpDirectoryAsync([this](ResponseMessageStruct<StringList> &dirResponse) {
        chipHeaderStep(dirResponse.data);
        return ISTAR_RESULT_SUCCESS;
    });
}

void ChipWaferSync::chipHeaderStep(const StringList &names)
{
    auto client = RM_GUIClient::GetInstance().GetRMClient("rm");
    if (!client)
    {
        QMetaObject::invokeMethod(this, [this]() { m_inFlight = false; }, Qt::QueuedConnection);
        return;
    }
    client->RMGetRcpHeaderAsync(names, [this, names](ResponseMessageStruct<RcpHeaderList> &hdrResponse) {
        RcpHeaderList hdrs = hdrResponse.data;
        QMetaObject::invokeMethod(this, [this, names, hdrs]() {
            std::set<std::string> inDir(names.items.begin(), names.items.end());
            std::set<std::string> changed;
            std::vector<std::string> needBody;

            // 目录中已消失的 chip：从缓存移除（wafer 引用走快照兜底）
            for (auto it = m_chips.begin(); it != m_chips.end();)
            {
                if (inDir.count(it->first) == 0)
                    it = m_chips.erase(it);
                else
                    ++it;
            }

            // header 对账：新增/uid 变化 → 待拉 body 提 dims
            for (const auto &h : hdrs.items)
            {
                auto it = m_chips.find(h.name);
                if (it == m_chips.end())
                {
                    ChipEntry e;
                    e.uid = h.uid;
                    e.released = h.released;
                    m_chips[h.name] = std::move(e);
                    changed.insert(h.name);
                    needBody.push_back(h.name);
                }
                else if (it->second.uid != h.uid)
                {
                    it->second.uid = h.uid;
                    it->second.released = h.released;
                    changed.insert(h.name);
                    needBody.push_back(h.name);
                }
                else
                {
                    it->second.released = h.released;
                }
            }

            if (needBody.empty())
                waferDirectoryStep(changed);
            else
                chipBodyStep(changed, needBody);
        }, Qt::QueuedConnection);
        return ISTAR_RESULT_SUCCESS;
    });
}

void ChipWaferSync::chipBodyStep(const std::set<std::string> &changed, const std::vector<std::string> &needBody)
{
    auto client = RM_GUIClient::GetInstance().GetRMClient("rm");
    if (!client)
    {
        waferDirectoryStep(changed);
        return;
    }
    StringList bodyNames;
    bodyNames.items = needBody;
    client->RMGetRcpAsync(bodyNames, [this, changed](ResponseMessageStruct<RcpList> &rcpResponse) {
        RcpList rcps = rcpResponse.data;
        QMetaObject::invokeMethod(this, [this, changed, rcps]() {
            for (const auto &r : rcps.items)
            {
                try
                {
                    S_ChipRecipeRef ref = Serializer::from_rawJson<S_ChipRecipeRef>(r.body);
                    auto it = m_chips.find(r.header.name);
                    if (it == m_chips.end())
                    {
                        ChipEntry e;
                        e.uid = r.header.uid;
                        e.released = r.header.released;
                        m_chips[r.header.name] = std::move(e);
                        it = m_chips.find(r.header.name);
                    }
                    it->second.chipWidth = ref.material.dwChipMaterialInfo.chipWidth;
                    it->second.chipHeight = ref.material.dwChipMaterialInfo.chipHeight;
                    it->second.downAlignPoints = toPoints(ref.align.lst_downAlignPoints);
                }
                catch (const std::exception &)
                {
                }
            }
            waferDirectoryStep(changed);
        }, Qt::QueuedConnection);
        return ISTAR_RESULT_SUCCESS;
    });
}

void ChipWaferSync::waferDirectoryStep(const std::set<std::string> &changed)
{
    RM_Client_Inherit *w = waferClient();
    if (!w)
    {
        finishTick(changed);
        return;
    }
    w->RMGetRcpDirectoryAsync([this, changed](ResponseMessageStruct<StringList> &dirResponse) {
        StringList names = dirResponse.data;
        QMetaObject::invokeMethod(this, [this, changed, names]() {
            std::set<std::string> inDir(names.items.begin(), names.items.end());
            std::vector<std::string> newWafers;
            // 已消失的 wafer：移除缓存
            for (auto it = m_wafers.begin(); it != m_wafers.end();)
            {
                if (inDir.count(it->first) == 0)
                    it = m_wafers.erase(it);
                else
                    ++it;
            }
            // 新增的 wafer：拉 body 解析进缓存
            for (const auto &n : names.items)
            {
                if (m_wafers.count(n) == 0)
                    newWafers.push_back(n);
            }
            if (newWafers.empty())
                finishTick(changed);
            else
                waferBodyStep(changed, newWafers);
        }, Qt::QueuedConnection);
        return ISTAR_RESULT_SUCCESS;
    });
}

void ChipWaferSync::waferBodyStep(const std::set<std::string> &changed, const std::vector<std::string> &newWafers)
{
    auto client = RM_GUIClient::GetInstance().GetRMClient("rm2");
    if (!client)
    {
        finishTick(changed);
        return;
    }
    StringList names;
    names.items = newWafers;
    client->RMGetRcpAsync(names, [this, changed](ResponseMessageStruct<RcpList> &rcpResponse) {
        RcpList rcps = rcpResponse.data;
        QMetaObject::invokeMethod(this, [this, changed, rcps]() {
            for (const auto &r : rcps.items)
            {
                try
                {
                    S_RecipeContentWafer recipe = Serializer::from_rawJson<S_RecipeContentWafer>(r.body);
                    const auto &oneChip = recipe.material.oneChipTypeInfo;
                    const std::string chipName = oneChip.chipRecipe.chipRecipeName;
                    if (chipName.empty())
                        continue;
                    WaferEntry e;
                    e.chipName = chipName;
                    e.recipe = std::move(recipe);
                    auto it = m_chips.find(chipName);
                    if (it != m_chips.end())
                        e.chipUid = it->second.uid;
                    m_wafers[r.header.name] = std::move(e);
                }
                catch (const std::exception &)
                {
                }
            }
            finishTick(changed);
        }, Qt::QueuedConnection);
        return ISTAR_RESULT_SUCCESS;
    });
}

void ChipWaferSync::finishTick(const std::set<std::string> &changed)
{
    for (const auto &name : changed)
        updateWafersReferencing(name);
    notifyChipListChanged();
    m_inFlight = false;
}

// 对引用 chipName 且尚未同步到最新 uid 的 wafer：更新 dims → 重算点位 → 清键合点
//  当前正在查看的 wafer：只刷 UI（回调），不自动保存；其余自动保存
//  跟随规则：绑定那一刻 chip 为 released 即视为有效引用，后续 chip 变 unreleased 也持续同步；
//  仅当 chip 从目录中消失（删除）时无法取数，走快照兜底
void ChipWaferSync::updateWafersReferencing(const std::string &chipName)
{
    auto chipIt = m_chips.find(chipName);
    if (chipIt == m_chips.end())
        return;
    const ChipEntry &chip = chipIt->second;

    for (auto &kv : m_wafers)
    {
        WaferEntry &w = kv.second;
        if (w.chipName != chipName || w.chipUid == chip.uid)
            continue;

        // 更新结构体里的快照 dims
        auto &oneChip = w.recipe.material.oneChipTypeInfo;
        oneChip.chipRecipe.chipWidth = chip.chipWidth;
        oneChip.chipRecipe.chipHeight = chip.chipHeight;
        recomputeLayout(w.recipe);

        if (kv.first == m_currentWafer)
        {
            // 当前查看的 wafer：数据+UI 已更新，保存交给用户
            w.chipUid = chip.uid;
            if (onCurrentWaferUpdated)
                onCurrentWaferUpdated(w.recipe);
        }
        else
        {
            // 已保存 wafer：自动保存；失败保留旧 uid，下轮重试
            if (saveWafer(kv.first, w))
                w.chipUid = chip.uid;
        }
    }
}

// 按 wafer 处方当前参数 + 快照 dims 重算排布点位；键合点为绝对坐标，重算后失效 → 清空
void ChipWaferSync::recomputeLayout(S_RecipeContentWafer &recipe)
{
    auto &oneChip = recipe.material.oneChipTypeInfo;
    const double inch_to_mm = 25.4;
    const double waferRadius = oneChip.waferSize * inch_to_mm / 2 - oneChip.waferEdgeLength;
    auto chipsPos = EGA::create_chip_position(
        waferRadius,
        oneChip.chipRecipe.chipWidth, oneChip.chipRecipe.chipHeight,
        oneChip.xChipPitch, oneChip.yChipPitch,
        oneChip.waferLayoutMode == WaferLayoutMode::ChipCornerAndWaferCenterCoincide,
        oneChip.waferNotchOrient);

    oneChip.lst_nominalPoints.clear();
    for (int i = 0; i < chipsPos.rows(); ++i)
        oneChip.lst_nominalPoints.push_back({chipsPos(i, 0), chipsPos(i, 1)});

    recipe.bondPointSelection.lst_bondNominalPoint.clear();
}

bool ChipWaferSync::saveWafer(const std::string &waferName, WaferEntry &entry)
{
    auto client = RM_GUIClient::GetInstance().GetRMClient("rm2");
    if (!client)
        return false;
    try
    {
        RcpBodySaveInfo info;
        info.rcpName = waferName;
        info.body = Serializer::to_rawJson<S_RecipeContentWafer>(entry.recipe);
        if (client->RMSaveRcpBody(info) == ISTAR_RESULT_SUCCESS)
        {
            qInfo().noquote() << QStringLiteral("[ChipWaferSync] chip 更新，wafer 已自动重算并保存: ")
                              << QString::fromStdString(waferName);
            return true;
        }
        qWarning().noquote() << QStringLiteral("[ChipWaferSync] 自动保存 wafer 失败: ")
                             << QString::fromStdString(waferName);
    }
    catch (const std::exception &e)
    {
        qWarning().noquote() << QStringLiteral("[ChipWaferSync] 自动保存 wafer 异常: ")
                             << QString::fromStdString(waferName) << e.what();
    }
    return false;
}
