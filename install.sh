#!/usr/bin/env bash

set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

if [[ ! -r /etc/os-release ]]; then
    echo "Unsupported distribution: /etc/os-release is unavailable." >&2
    exit 1
fi

# shellcheck disable=SC1091
source /etc/os-release
distro_id=${ID:-}
distro_like=${ID_LIKE:-}

case " $distro_id $distro_like " in
    *" arch "*) distro_family=arch ;;
    *" debian "*|*" ubuntu "*) distro_family=debian ;;
    *" fedora "*) distro_family=fedora ;;
    *" opensuse "*|*" suse "*) distro_family=opensuse ;;
    *)
        echo "Unsupported distribution: ${PRETTY_NAME:-${distro_id:-unknown}}." >&2
        exit 1
        ;;
esac

package_installed()
{
    local package_name=$1

    case "$distro_family" in
        arch)
            pacman -Q "$package_name" >/dev/null 2>&1
            ;;
        debian)
            [[ $(dpkg-query -W -f='${Status}' "$package_name" 2>/dev/null || true) == "install ok installed" ]]
            ;;
        fedora|opensuse)
            rpm --quiet -q --whatprovides "$package_name"
            ;;
    esac
}

case "$distro_family" in
    arch)
        wayland_runtime=kwin
        x11_runtime=kwin-x11
        ;;
    debian)
        wayland_runtime=kwin-wayland
        x11_runtime=kwin-x11
        ;;
    fedora)
        wayland_runtime=kwin-wayland
        x11_runtime=kwin-x11
        ;;
    opensuse)
        wayland_runtime=kwin6
        x11_runtime=kwin6-x11
        ;;
esac

has_wayland=0
has_x11=0
if package_installed "$wayland_runtime"; then
    has_wayland=1
fi
if package_installed "$x11_runtime"; then
    has_x11=1
fi

echo "Installed compositors:"
if (( has_wayland )); then
    echo -n "- Wayland ($wayland_runtime)"
fi
if (( has_x11 )); then
    echo ", X11 ($x11_runtime)"
fi

echo "Checking dependencies ..."

print_dependency_guidance()
{
    local include_wayland=$has_wayland
    local include_x11=$has_x11
    local -a packages

    # Wayland is the default supported target when no compositor is installed.
    if (( !include_wayland && !include_x11 )); then
        include_wayland=1
    fi

    case "$distro_family" in
        arch)
            packages=(base-devel cmake extra-cmake-modules qt6-base
                      kcoreaddons kconfig kconfigwidgets kcmutils vulkan-headers)
            (( include_wayland )) && packages+=(kwin)
            (( include_x11 )) && packages+=(kwin-x11)
            printf 'sudo pacman -S'
            ;;
        debian)
            packages=(build-essential cmake extra-cmake-modules qt6-base-dev
                      qt6-declarative-dev libkf6coreaddons-dev libkf6config-dev
                      libkf6configwidgets-dev libkf6kcmutils-dev libvulkan-dev)
            (( include_wayland )) && packages+=(kwin-wayland kwin-dev)
            (( include_x11 )) && packages+=(kwin-x11 kwin-x11-dev)
            printf 'sudo apt install'
            ;;
        fedora)
            packages=(gcc-c++ cmake extra-cmake-modules qt6-qtbase-devel
                      qt6-qtdeclarative-devel kf6-kcoreaddons-devel
                      kf6-kconfig-devel kf6-kconfigwidgets-devel
                      kf6-kcmutils-devel vulkan-headers)
            (( include_wayland )) && packages+=(kwin kwin-devel)
            (( include_x11 )) && packages+=(kwin-x11 kwin-x11-devel)
            printf 'sudo dnf install'
            ;;
        opensuse)
            packages=(gcc-c++ cmake kf6-extra-cmake-modules qt6-base-devel
                      qt6-declarative-devel kf6-kcoreaddons-devel
                      kf6-kconfig-devel kf6-kconfigwidgets-devel
                      kf6-kcmutils-devel vulkan-headers)
            (( include_wayland )) && packages+=(kwin6 kwin6-devel)
            (( include_x11 )) && packages+=(kwin6-x11 kwin6-x11-devel)
            printf 'sudo zypper install'
            ;;
    esac

    printf ' %q' "${packages[@]}"
    printf '\n'
}

dependencies_missing=0
if ! command -v cmake >/dev/null 2>&1 || ! command -v c++ >/dev/null 2>&1; then
    dependencies_missing=1
elif (( !has_wayland && !has_x11 )); then
    dependencies_missing=1
else
    dependency_probe_dir=$(mktemp -d "${TMPDIR:-/tmp}/keep-above-outline-deps.XXXXXX")
    cleanup_dependency_probe()
    {
        cmake -E remove_directory "$dependency_probe_dir"
    }
    trap cleanup_dependency_probe EXIT

    mkdir -p "$dependency_probe_dir/source"
    cat >"$dependency_probe_dir/source/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(keep-above-outline-dependency-probe LANGUAGES CXX)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Widgets Quick)
find_package(KF6 REQUIRED COMPONENTS CoreAddons ConfigWidgets KCMUtils)
find_package(${KWIN_PROBE_PACKAGE} REQUIRED)
EOF

    if (( has_wayland )) && ! cmake -S "$dependency_probe_dir/source" \
        -B "$dependency_probe_dir/wayland" -DKWIN_PROBE_PACKAGE=KWin \
        >/dev/null 2>&1; then
        dependencies_missing=1
    fi
    if (( has_x11 )) && ! cmake -S "$dependency_probe_dir/source" \
        -B "$dependency_probe_dir/x11" -DKWIN_PROBE_PACKAGE=KWinX11 \
        >/dev/null 2>&1; then
        dependencies_missing=1
    fi

    cleanup_dependency_probe
    trap - EXIT
fi

if (( dependencies_missing )); then
    echo "-----------------------------------"
    echo "Dependencies missing, install with:"
    print_dependency_guidance
    echo ""
    echo "- Run this script again after installing."
    exit 1
fi

echo "Building ..."

declare -a build_directories=()
declare -a backend_names=()
declare -a backend_values=()

if (( has_wayland )); then
    build_directories+=("$script_dir/build-wayland")
    backend_names+=("Wayland")
    backend_values+=("WAYLAND")
fi
if (( has_x11 )); then
    build_directories+=("$script_dir/build-x11")
    backend_names+=("X11")
    backend_values+=("X11")
fi

# Finish every configure before starting any compilation.
for index in "${!build_directories[@]}"; do
    cmake -S "$script_dir" -B "${build_directories[$index]}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DKWIN_BACKEND="${backend_values[$index]}"
done

for build_directory in "${build_directories[@]}"; do
    cmake --build "$build_directory"
done

backend_summary=${backend_names[0]}
for ((index = 1; index < ${#backend_names[@]}; ++index)); do
    backend_summary+=", ${backend_names[$index]}"
done
echo "-----------------------------------------------------"
echo "Built successfully for: $backend_summary"
echo "Install the effect with:"

install_command=""
for index in "${!backend_values[@]}"; do
    if [[ -n $install_command ]]; then
        install_command+=" && "
    fi
    if [[ ${backend_values[$index]} == WAYLAND ]]; then
        install_command+="sudo cmake --install build-wayland"
    else
        install_command+="sudo cmake --install build-x11"
    fi
done
echo "$install_command"
