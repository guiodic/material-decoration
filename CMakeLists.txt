cmake_minimum_required (VERSION 3.16.0)
project (material-decoration)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions (-Wall -Werror)

include (FeatureSummary)
find_package (ECM 0.0.9 REQUIRED NO_MODULE)

set (CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${ECM_MODULE_PATH}
)

set(DKDE_INSTALL_USE_QT_SYS_PATHS=ON)
set(DQT_MAJOR_VERSION=6)
set(DQT_VERSION_MAJOR=6)

include (ECMInstallIcons)
include (KDEInstallDirs)
include (KDECMakeSettings)
include (KDECompilerSettings NO_POLICY_SCOPE)

# set(QT_MIN_VERSION "5.9.0")
# https://doc.qt.io/qt-6/cmake-qt5-and-qt6-compatibility.html#supporting-older-qt-5-versions
find_package(QT NAMES Qt6)
#find_package(QT NAMES Qt5)
find_package(QT NAMES Qt${QT_VERSION_MAJOR})

find_package(Qt6 CONFIG REQUIRED COMPONENTS
    Widgets
    DBus
)
include(ECMQtDeclareLoggingCategory)

# Remove Qt 5.15 Deprecations
# https://doc.qt.io/qt-6/portingguide.html
add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=0x050F00)

add_subdirectory (src/libdbusmenuqt)
add_subdirectory (src)

feature_summary(WHAT ALL)
