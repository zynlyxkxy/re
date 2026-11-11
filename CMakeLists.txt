cmake_minimum_required(VERSION 3.26)
project(RE VERSION 1.0.0.0 LANGUAGES CXX)

# ============================================================
#  module/re —— 处方界面模块（Chip / Wafer 两套产品）
#
#  与 io/motion 不同的地方：
#   1) 平台（RM 协议、libUIRm、控制层 CTL_TL_RM）在子仓 platform 里，是一个
#      独立的 CMake 工程，有自己的 preset / install 树 / deploy_product.bat。
#      它由 scripts\build-*.bat 先构建，本工程只消费它的产物。
#   2) re 自己不做 RPC 生成（协议在 platform 仓内、生成物入库），
#      所以不 include istar_rpc。
#   3) 三个程序：libUIRE_CHIP + GUI_TL_RM_CHIP、libUIRE_Wafer + GUI_TL_RM_Wafer，
#      以及从平台搬来的 CTL_TL_RM.exe（re 只搬运，不构建）。
#   4) output/ 按"出口"分层：output/exe/ 给顶层装配（= exe/apps/re/ 的全部），
#      output/interface/ 给下游（gui 从这里取 libUIRE_*.dll）。
# ============================================================

# ---- 共享构建模块在本工程内（module/re/cmake/），独立构建不依赖任何外层工程 ----
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ---- 平台基础库在 platform 子仓里（re 自己没有 basiclibrary_output）----
# 必须在 include(istar_common) 之前覆盖，共享模块只在这些变量未定义时给默认值。
set(ISTAR_BASE "${CMAKE_SOURCE_DIR}/platform/basiclibrary_output")
set(BASIC_LIB  "${ISTAR_BASE}/lib")
set(BASIC_INC  "${ISTAR_BASE}/include")
# 运行目录自包含的来源：平台拍平后的部署集（ISTAR/Qt5/rm + platforms/styles + config）
set(ISTAR_RUNTIME_SRC "${CMAKE_SOURCE_DIR}/platform/deploy/$<CONFIG>")

include(istar_common)    # BASIC_* / ISTAR_RUNTIME_SRC / OUTER_CONFIG_DIR / ISTAR_OUTPUT_DIR
                         # istar_runtime_shared / istar_add_app()
include(istar_runtime)   # istar_deploy_runtime() / istar_stage()
include(istar_install)   # istar_install_interface() / istar_finalize_deploy()

# 平台工程的构建树（re 从这里搬运 CTL_TL_RM.exe / rm.dll / libUIRm*；
# 拍平后的 platform/deploy/<cfg> 只服务 build/<Config>/ 的自包含调试，
#  由 istar_runtime_shared 处理，这里不需要它）
set(RE_PLATFORM_BUILD "${CMAKE_SOURCE_DIR}/platform/build")

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

set(CMAKE_AUTOMOC ON)   # RecipeModule.h / Tabs/*.h 里的 Q_OBJECT
set(CMAKE_AUTOUIC ON)   # RecipeModule.ui 与 Tabs 下的 .ui

# ── 平台头文件 ──
set(PLATFORM_INCLUDES
    "${CMAKE_SOURCE_DIR}/platform/GUI_TL_RM/libUIRM/include"
    "${CMAKE_SOURCE_DIR}/platform/RM/include"
    "${ISTAR_BASE}/include/common/include"
    "${ISTAR_BASE}/include/istarbasicvalue/include"
    "${ISTAR_BASE}/include/istarjson/include"
    "${ISTAR_BASE}/include/istarlog/include"
    "${ISTAR_BASE}/include/istarmw/include"
    "${ISTAR_BASE}/include/istargf/include"
    "${ISTAR_BASE}/include/istaryaml/include"
    "${ISTAR_BASE}/include/istarsm/include"
    "${ISTAR_BASE}/include/istarthreadpool/include"
    "${ISTAR_BASE}/include/istarcsv/include"
    "${ISTAR_BASE}/include/istarencryption/include"
    "${ISTAR_BASE}/include/istarfilesystem/include"
    "${ISTAR_BASE}/include/istarserializer/include"
    "${ISTAR_BASE}/include/istarxml/include"
    "${ISTAR_BASE}/include/istarzmqworker/include"
    "${ISTAR_BASE}/include/istarguicomponent/include"
    "${ISTAR_BASE}/include/istaruistyle/include"
    "${ISTAR_BASE}/include/istarlanguage/include"
)

# ── 平台 GUI 库 libUIRm / libUIRmd 的链接目录（平台构建树的硬路径）──
set(PLATFORM_UIRM_LIB
    "${RE_PLATFORM_BUILD}/$<CONFIG>/X86_64/Windows/GUI_TL_RM/uirm_build/$<CONFIG>")

# ── 公共链接库 ──
set(UIRE_LINK_LIBS
    $<$<CONFIG:Debug>:libUIRmd>
    $<$<CONFIG:Release>:libUIRm>
    $<$<CONFIG:Debug>:ISTARBasicValued>
    $<$<CONFIG:Release>:ISTARBasicValue>
    $<$<CONFIG:Debug>:ISTARGUIComponentd>
    $<$<CONFIG:Release>:ISTARGUIComponent>
    $<$<CONFIG:Debug>:ISTARUIStyled>
    $<$<CONFIG:Release>:ISTARUIStyle>
    $<$<CONFIG:Debug>:ISTARLogd>
    $<$<CONFIG:Release>:ISTARLog>
    $<$<CONFIG:Debug>:ISTARGFd>
    $<$<CONFIG:Release>:ISTARGF>
    $<$<CONFIG:Debug>:ISTARLanguaged>
    $<$<CONFIG:Release>:ISTARLanguage>
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
)

# ============================================================
#  add_re_product(<dll target> <exe target> <dll 产物名> <源码目录>)
#
#  一套 RE 产品 = 界面库(dll) + 验收程序(exe)。
#  两套产品的源码互不相通，各自的列表由调用方 target_sources 补。
#
#  落点（output/ 按"出口"分层，与 io/motion 一致）：
#    build/<Config>/                  dll + exe，平铺，可调试（含 exe 旁的配置）
#    output/interface/gui/lib/<cfg>/  给 gui 的界面库（dll + 导入库 + pdb），
#                                     由下面调用方的 istar_install_interface 落位
#
#  验收程序 GUI_TL_RM_CHIP/Wafer.exe 只进 build/（单模块验收用），不是交付物 ——
#  交付的是界面库 dll 和平台的控制层 CTL_TL_RM.exe。
# ============================================================
function(add_re_product DLL_TARGET EXE_TARGET LIB_OUTPUT MAIN_DIR)
    # ---- 界面库 ----
    add_library(${DLL_TARGET} SHARED
        "${MAIN_DIR}/RecipeModule.h"
        "${MAIN_DIR}/RecipeModule.cpp")

    target_include_directories(${DLL_TARGET} PRIVATE
        ${PLATFORM_INCLUDES}
        "${MAIN_DIR}"
        "${CMAKE_SOURCE_DIR}/MainProject")

    target_link_directories(${DLL_TARGET} PRIVATE
        "${BASIC_LIB}"
        "${PLATFORM_UIRM_LIB}")

    target_link_libraries(${DLL_TARGET} PRIVATE ${UIRE_LINK_LIBS})

    # Tabs/*.cpp 里 #include "ui_RecipeModule.h"，AUTOUIC 要到 .ui 所在目录找
    set_target_properties(${DLL_TARGET} PROPERTIES
        OUTPUT_NAME "${LIB_OUTPUT}"
        AUTOUIC_SEARCH_PATHS "${MAIN_DIR}")

    # ---- 验收程序（平铺到 build/<Config>/）----
    istar_add_app(${EXE_TARGET} WIN32 SOURCES "${MAIN_DIR}/main.cpp")
    target_link_libraries(${EXE_TARGET} PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets)
    # exe 只是 uirm 宿主、动态加载本产品的 dll，这里显式建依赖让顺序确定
    add_dependencies(${EXE_TARGET} ${DLL_TARGET})

    # Debug 构建定义 GUI_DEBUG（与 gui 工程同一约定；不能改成 _DEBUG/QT_DEBUG）
    target_compile_definitions(${DLL_TARGET} PRIVATE $<$<CONFIG:Debug>:GUI_DEBUG>)
    target_compile_definitions(${EXE_TARGET} PRIVATE $<$<CONFIG:Debug>:GUI_DEBUG>)

    # 运行目录自包含：平台部署集由 istar_runtime_shared 拷一次，这里把本产品的 dll 摆到 exe 旁
    istar_deploy_runtime(${EXE_TARGET} TARGETS ${DLL_TARGET})

    # 只改界面库（GraphicsView/Tabs 之类）时 exe 不会重链，而上面 istar_deploy_runtime 的
    # POST_BUILD 挂在 exe 上 → 运行目录（Qt Creator 跑 exe 的那个目录，exe 旁边）里会一直留着
    # 上一版 dll，现场表现就是"代码改了却没生效"。这里再挂一份到 dll 自己身上：
    # dll 一重链就把 exe 旁那份刷新掉。
    # make_directory：干净构建树里 dll 先于 exe 链接，此时 exe 目录还可能不存在。
    # 注意 exe 正在运行时会拷贝失败（dll 被占用），此时先关掉 exe 再构建。
    add_custom_command(TARGET ${DLL_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${EXE_TARGET}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${DLL_TARGET}>" "$<TARGET_FILE_DIR:${EXE_TARGET}>"
        COMMENT "[istar] refresh ${LIB_OUTPUT}.dll next to ${EXE_TARGET}"
        VERBATIM)

    # ---- 配置：按"配置根"形状铺到 exe 旁，外层 1927project/config 覆盖 ----
    # 只落 build/<Config>/，供单独调试；交付形状（output/exe）里不带配置，
    # 现场配置由顶层 config/ 统一出到 exe/config/。
    # build/<Config>/ 就是调试时的配置根：mw_config.yaml 在根，rm_config\ 是模块目录。
    istar_stage(${EXE_TARGET} SRC "${CMAKE_SOURCE_DIR}/config/mw_config.yaml")
    istar_stage(${EXE_TARGET} SRC "${CMAKE_SOURCE_DIR}/config/recipe_config.yaml" TO rm_config)
    if(OUTER_CONFIG_DIR)
        istar_stage(${EXE_TARGET} SRC "${OUTER_CONFIG_DIR}/mw_config.yaml" OPTIONAL)
        istar_stage(${EXE_TARGET} SRC "${OUTER_CONFIG_DIR}/rm_config" OPTIONAL TO rm_config)
    endif()

    # Recipe_Schema.json 在 exe 根上会误导调试（模块只认 rm_config\Recipe_Schema.json），
    # 平台拍平部署集里的那份拷进来后显式删掉；rm_config\ 里那份保留。
    add_custom_command(TARGET ${EXE_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f
            "$<TARGET_FILE_DIR:${EXE_TARGET}>/Recipe_Schema.json"
        COMMENT "[istar] drop stray Recipe_Schema.json from the debug dir root"
        VERBATIM)
endfunction()

# ── Chip ──
add_re_product(UIRE_Chip GUI_TL_RM_CHIP "libUIRE_CHIP"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip")
target_sources(UIRE_Chip PRIVATE
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/RecipeModule.ui"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Bonding/BondingParamDelegate.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Bonding/BondingParamDelegate.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Bonding/TheoryBondData.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Bonding/SimulationCurveDialog.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Bonding/SimulationCurveDialog.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Point/PointCoordDelegate.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Tabs/MaterialTab.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Tabs/MaterialTab.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Tabs/LevelTab.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Tabs/LevelTab.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Tabs/AlignTab.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Tabs/AlignTab.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Tabs/BondingTab.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Tabs/BondingTab.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Tabs/PointTableOps.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectChip/Tabs/PointTableOps.cpp"
)

# ── Wafer ──
add_re_product(UIRE_Wafer GUI_TL_RM_Wafer "libUIRE_Wafer"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer")
target_sources(UIRE_Wafer PRIVATE
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/RecipeModule.ui"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/ChipWaferSync.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/ChipWaferSync.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/GraphicsView/ChipCalibrationController.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/GraphicsView/ChipCalibrationController.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Algorithm/EAGAlgorithm/EgaFit.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Algorithm/EAGAlgorithm/EgaFit.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/GraphicsView/WaferGraphicsView.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/GraphicsView/WaferGraphicsView.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/GraphicsView/DiagramBase.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/GraphicsView/DiagramBase.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/GraphicsView/BondPointRectItem.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/GraphicsView/BondPointRectItem.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/GraphicsView/LevelingMarkItem.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/GraphicsView/LevelingMarkItem.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Point/PointCoordDelegate.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/MaterialTab.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/MaterialTab.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/WaferLevelTab.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/WaferLevelTab.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/ChuckLevelTab.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/ChuckLevelTab.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/ChipCalibrationTab.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/ChipCalibrationTab.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/BondPointTab.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/BondPointTab.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/OperationModeTab.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/OperationModeTab.cpp"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/ContinuousTab.h"
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Tabs/ContinuousTab.cpp"
)
target_include_directories(UIRE_Wafer PRIVATE
    "${CMAKE_SOURCE_DIR}/MainProject/MainProjectWafer/Algorithm")

# ============================================================
#  output/exe/ —— 给顶层装配的那一份（= exe/apps/re/ 的全部）
#    = 控制层程序 + RM 运行库 + 两个 RE 界面库（Chip/Wafer 编辑器）。
#    只有 Release；顶层 assemble 整目录拷走。
#    平台部分是平台构建树的产物（re 只搬运，不构建）——按旧 :stage_mctrl 的清单，
#    但源不存在时 install 会直接报错，不再静默跳过。
#
#    不放平台基础库 dll / Qt dll：它们属于顶层共享层（exe/shared/platform、
#    exe/shared/system），assemble 时本来就会被过滤掉（deploy_ops 的 /XF）。
#    也不放配置：现场配置由顶层 config/ 统一出到 exe/config/rm_config/。
# ============================================================
set(_CTL_BIN "${RE_PLATFORM_BUILD}/Release/X86_64/Windows")

install(FILES
        "${_CTL_BIN}/CTL_TL_RM/Release/CTL_TL_RM.exe"
        "${_CTL_BIN}/RM/code/rm/Release/rm.dll"
    DESTINATION "exe" CONFIGURATIONS Release)

#  RE 界面库也进部署形状：libUIRE_CHIP/Wafer 是 re 自己的产物，此前只装进
#  interface/gui/（给下游 gui 编译链接用），部署包里一直没有这一份。
#    GUI_WM.exe 不对它们做静态导入，而是运行时按**裸名**动态加载：
#    gui/MainProject/PlatformModule/Rm/RMPage.cpp 用 QLibrary 载 libUIRm，再
#    UIRm_GetWidgetWithLib("rm"/"rm2", "libUIRE_CHIP"/"libUIRE_Wafer") 建
#    Chip/Wafer 处方编辑页；QLibrary 就是裸名 LoadLibrary，按 exe 目录 →
#    系统目录 → cwd → PATH 找。落到 exe/apps/re/ 后，顶层 run.bat 会把每个
#    apps\* 加进 PATH，GUI_WM 据此找到（与平台 libUIRm / libUI* 同一套机制）。
#    加载失败不报错、只退化成 "RM module load failed" 占位页，所以必须打进
#    部署包，调试期那份（build/<Config>/ 旁边的）只管 IDE 里直接跑。
install(FILES "$<TARGET_FILE:UIRE_Chip>" "$<TARGET_FILE:UIRE_Wafer>"
    DESTINATION "exe" CONFIGURATIONS Release)

# ============================================================
#  output/interface/{rpc,gui}/ —— 给下游工程 / 外部拿接口的那一份
#    interface/rpc/  = RM 协议完整包（5 个头 + rm.dll/rm.lib）—— 给任何说 RM 的消费方
#    interface/gui/  = 给 gui 的界面库（libUIRE_*.dll + 导入库 + pdb + 3 个头）；
#                      gui/MainProject/CMakeLists.txt 的 RE_OUT_DIR 读的就是这里。
#    形状同旧 :update_interface，只是 mctrl → rpc（按用途而不是按消费者命名）。
# ============================================================
set(_IF_GUI_INC "${CMAKE_SOURCE_DIR}/platform/GUI_TL_RM/libUIRM/include")
set(_IF_RM_INC  "${CMAKE_SOURCE_DIR}/platform/RM/include")
set(_IF_RM_API  "${CMAKE_SOURCE_DIR}/platform/RM/api")
set(_IF_PLAT_B  "${RE_PLATFORM_BUILD}")

# ---- 头文件 ----
install(FILES
        "${_IF_GUI_INC}/IREModule.h"
        "${_IF_GUI_INC}/IModule.h"
        "${_IF_GUI_INC}/libUIRM_global.h"
    DESTINATION interface/gui/include)
install(FILES
        "${_IF_RM_INC}/rm_client.h"
        "${_IF_RM_INC}/rm_define.h"
        "${_IF_RM_INC}/rm_server.h"
        "${_IF_RM_API}/rm_controller.h"
        "${_IF_RM_API}/rmc_data.h"
    DESTINATION interface/rpc/include)

# ---- re 自己的两个界面库（release/debug 各进各的目录）----
istar_install_interface(UIRE_Chip  SUB gui)
istar_install_interface(UIRE_Wafer SUB gui)
install(FILES "$<TARGET_PDB_FILE:UIRE_Chip>" "$<TARGET_PDB_FILE:UIRE_Wafer>"
    DESTINATION interface/gui/lib/debug CONFIGURATIONS Debug)

# ---- gui 包里的 RM / libUIRm（平台产物）----
install(FILES
        "${_IF_PLAT_B}/Debug/X86_64/Windows/RM/code/rm/Debug/rmd.dll"
        "${CMAKE_SOURCE_DIR}/platform/RM/lib/rmd.lib"
        "${_IF_PLAT_B}/Debug/X86_64/Windows/GUI_TL_RM/uirm_build/Debug/libUIRmd.dll"
        "${_IF_PLAT_B}/Debug/X86_64/Windows/GUI_TL_RM/uirm_build/Debug/libUIRmd.lib"
        "${_IF_PLAT_B}/Debug/X86_64/Windows/GUI_TL_RM/uirm_build/Debug/libUIRmd.pdb"
    DESTINATION interface/gui/lib/debug CONFIGURATIONS Debug)
install(FILES
        "${_IF_PLAT_B}/Release/X86_64/Windows/RM/code/rm/Release/rm.dll"
        "${CMAKE_SOURCE_DIR}/platform/RM/lib/rm.lib"
        "${_IF_PLAT_B}/Release/X86_64/Windows/GUI_TL_RM/uirm_build/Release/libUIRm.dll"
        "${_IF_PLAT_B}/Release/X86_64/Windows/GUI_TL_RM/uirm_build/Release/libUIRm.lib"
    DESTINATION interface/gui/lib/release CONFIGURATIONS Release)

# ---- rpc 包里的 RM ----
install(FILES
        "${_IF_PLAT_B}/Debug/X86_64/Windows/RM/code/rm/Debug/rmd.dll"
        "${CMAKE_SOURCE_DIR}/platform/RM/lib/rmd.lib"
    DESTINATION interface/rpc/lib/debug CONFIGURATIONS Debug)
install(FILES
        "${_IF_PLAT_B}/Release/X86_64/Windows/RM/code/rm/Release/rm.dll"
        "${CMAKE_SOURCE_DIR}/platform/RM/lib/rm.lib"
    DESTINATION interface/rpc/lib/release CONFIGURATIONS Release)

# 把 install 挂进默认构建（Qt Creator / VSCode / 命令行"构建一下"都会产出 output/）
istar_finalize_deploy()
