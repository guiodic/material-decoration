find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    UiTools
)

find_package(KF6 REQUIRED COMPONENTS
    Config
    ConfigWidgets
    CoreAddons
    I18n
    KCMUtils
    Service
)

set(KCM_SRCS
    kcm.cpp
)

qt_wrap_ui(KCM_SRCS
    config.ui
)

add_library(materialdecoration_kcm MODULE ${KCM_SRCS})

target_link_libraries(materialdecoration_kcm
    PUBLIC
        materialdecoration_core
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::UiTools
        KF6::ConfigCore
        KF6::ConfigGui
        KF6::ConfigWidgets
        KF6::CoreAddons
        KF6::I18n
        KF6::KCMUtils
)

target_include_directories(materialdecoration_kcm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

install(TARGETS materialdecoration_kcm DESTINATION ${KDE_INSTALL_PLUGINDIR}/kcms/)
install(FILES materialdecoration-kcm.json DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES materialdecoration_kcm.desktop DESTINATION ${KDE_INSTALL_APPDIR})
