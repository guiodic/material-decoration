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
set(DQT_VERSION_MAJOR=6)

include (ECMInstallIcons)
include (KDEInstallDirs)
include (KDECMakeSettings)
include (KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 CONFIG REQUIRED COMPONENTS
    Widgets
    DBus
)
include(ECMQtDeclareLoggingCategory)

add_subdirectory (src/libdbusmenuqt)
add_subdirectory (src)

feature_summary(WHAT ALL)
