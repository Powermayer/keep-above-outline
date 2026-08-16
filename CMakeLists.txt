cmake_minimum_required(VERSION 3.20)
project(keep-above-outline VERSION 1.0.01)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_MAJOR_VERSION 6)
set(QT_DEFAULT_MAJOR_VERSION 6)

set(KWIN_BACKEND "WAYLAND" CACHE STRING "KWin backend to build for (WAYLAND or X11)")
set_property(CACHE KWIN_BACKEND PROPERTY STRINGS WAYLAND X11)

if(KWIN_BACKEND STREQUAL "WAYLAND")
    set(KWIN_PACKAGE KWin)
    set(KWIN_TARGET KWin::kwin)
    set(KWIN_EFFECT_NAMESPACE "kwin/effects/plugins")
    set(KWIN_CONFIG_NAMESPACE "kwin/effects/configs")
elseif(KWIN_BACKEND STREQUAL "X11")
    set(KWIN_PACKAGE KWinX11)
    set(KWIN_TARGET KWinX11::kwin)
    set(KWIN_EFFECT_NAMESPACE "kwin-x11/effects/plugins")
    set(KWIN_CONFIG_NAMESPACE "kwin-x11/effects/configs")
else()
    message(FATAL_ERROR
        "Unknown KWIN_BACKEND='${KWIN_BACKEND}'. Expected exactly WAYLAND or X11.")
endif()

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Widgets Quick)
find_package(KF6 REQUIRED COMPONENTS CoreAddons ConfigWidgets KCMUtils)
find_package(${KWIN_PACKAGE} REQUIRED)

if(KWIN_BACKEND STREQUAL "WAYLAND")
    set(DETECTED_KWIN_VERSION "${KWin_VERSION}")
else()
    set(DETECTED_KWIN_VERSION "${KWinX11_VERSION}")
endif()

# --- Effect plugin ---
kcoreaddons_add_plugin(keep-above-outline
    SOURCES keepaboveoutline.cpp
    INSTALL_NAMESPACE "${KWIN_EFFECT_NAMESPACE}"
)
target_link_libraries(keep-above-outline PRIVATE
    ${KWIN_TARGET}
)

if(KWIN_BACKEND STREQUAL "X11")
    target_compile_definitions(keep-above-outline PRIVATE
        KEEPABOVE_X11
        KEEPABOVE_LEGACY_PAINT_TYPES
        KEEPABOVE_PREPAINT_PRESENT_TIME
    )
    # KWin X11 6.7 headers use C++23 library types such as std::expected.
    if(DETECTED_KWIN_VERSION VERSION_GREATER_EQUAL "6.7")
        target_compile_features(keep-above-outline PRIVATE cxx_std_23)
    endif()
else()
    if(DETECTED_KWIN_VERSION VERSION_LESS "6.6")
        target_compile_definitions(keep-above-outline PRIVATE
            KEEPABOVE_LEGACY_PAINT_TYPES
        )
    endif()
    if(DETECTED_KWIN_VERSION VERSION_LESS "6.7")
        target_compile_definitions(keep-above-outline PRIVATE
            KEEPABOVE_PREPAINT_PRESENT_TIME
        )
    endif()
endif()

# --- Config module plugin ---
qt6_wrap_ui(CONFIG_UI_SRCS keepaboveoutline_config.ui)

kcoreaddons_add_plugin(kwin_keep-above-outline_config
    SOURCES keepaboveoutline_config.cpp ${CONFIG_UI_SRCS}
    INSTALL_NAMESPACE "${KWIN_CONFIG_NAMESPACE}"
)
target_include_directories(kwin_keep-above-outline_config PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(kwin_keep-above-outline_config PRIVATE
    Qt6::DBus
    Qt6::Widgets
    KF6::CoreAddons
    KF6::ConfigWidgets
    KF6::KCMUtils
)
