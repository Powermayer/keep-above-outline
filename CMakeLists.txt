cmake_minimum_required(VERSION 3.20)
project(keep-above-outline VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_MAJOR_VERSION 6)
set(QT_DEFAULT_MAJOR_VERSION 6)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick)
find_package(KF6 REQUIRED COMPONENTS CoreAddons ConfigWidgets KCMUtils)
find_package(KWin REQUIRED)

# --- Effect plugin ---
kcoreaddons_add_plugin(keep-above-outline
    SOURCES keepaboveoutline.cpp
    INSTALL_NAMESPACE "kwin/effects/plugins"
)
target_link_libraries(keep-above-outline PRIVATE
    KWin::kwin
)

# --- Config module plugin ---
qt6_wrap_ui(CONFIG_UI_SRCS keepaboveoutline_config.ui)

kcoreaddons_add_plugin(kwin_keep-above-outline_config
    SOURCES keepaboveoutline_config.cpp ${CONFIG_UI_SRCS}
    INSTALL_NAMESPACE "kwin/effects/configs"
)
target_include_directories(kwin_keep-above-outline_config PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(kwin_keep-above-outline_config PRIVATE
    Qt6::Widgets
    KF6::CoreAddons
    KF6::ConfigWidgets
    KF6::KCMUtils
)
