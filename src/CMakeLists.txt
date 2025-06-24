
set(DKDE_INSTALL_USE_QT_SYS_PATHS=ON)
set(DQT_MAJOR_VERSION=6)
set(DQT_VERSION_MAJOR=6)

find_package (KDecoration3 REQUIRED)

find_package (Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Core
    Gui
)

find_package (KF6 REQUIRED COMPONENTS
    Config
    ConfigWidgets
    CoreAddons
    GuiAddons
    I18n
    IconThemes
    WindowSystem
    KCMUtils
)

find_package(KWinX11 REQUIRED)

include_directories(/usr/include/kwin-x11)

# X11
find_package(X11 REQUIRED)
set_package_properties(X11 PROPERTIES DESCRIPTION "X11 libraries"
    URL "http://www.x.org"
    TYPE REQUIRED
    PURPOSE "Required for building the X11 based workspace"
)

find_package(XCB MODULE REQUIRED COMPONENTS
    XCB
    RANDR
)
set_package_properties(XCB PROPERTIES TYPE REQUIRED)

#if (Qt6_FOUND)
    # The QX11Info class has been removed.
    # Clients that still rely on the functionality can include the private header <QtGui/private/qtx11extras_p.h> as a stopgap solution.
    # To enable private headers use QT += gui-private with qmake, or add a project dependency to Qt::GuiPrivate with CMake.
    # https://doc.qt.io/qt-6/extras-changes-qt6.html#changes-to-qt-x11-extras
    find_package(Qt${QT_VERSION_MAJOR} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS GuiTools)
#elseif(Qt5_FOUND)
#    find_package(Qt${QT_VERSION_MAJOR} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS X11Extras)
#endif()


if(X11_FOUND AND XCB_XCB_FOUND)
    set(HAVE_X11 ON)
    set(X11_LIBRARIES XCB::XCB)
endif()


# Wayland
#find_package (KF6 COMPONENTS
#    Wayland
#)

#if(KF5Wayland_FOUND)
    set(HAVE_Wayland OFF)
#    set(Wayland_LIBRARIES KF5::WaylandClient)
#endif()


# KDecoration3/Plasma Version
if(${KDecoration3_VERSION} VERSION_GREATER_EQUAL "5.25.0")
    set(HAVE_KDecoration3_5_25 ON)
else()
    set(HAVE_KDecoration3_5_25 OFF)
endif()
message(STATUS "HAVE_KDecoration3_5_25: ${HAVE_KDecoration3_5_25} (${KDecoration3_VERSION})")

# KF5 Version
#if(${KF5_VERSION} VERSION_GREATER_EQUAL "5.101.0")
    set(HAVE_KF5_101 ON)
#else()
#    set(HAVE_KF5_101 OFF)
#endif()
#message(STATUS "HAVE_KF5_101: ${HAVE_KF5_101} (${KF5_VERSION})")


configure_file(BuildConfig.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/BuildConfig.h)

set (decoration_SRCS
    AppMenuModel.cc
    AppMenuButton.cc
    AppMenuButtonGroup.cc
    BoxShadowHelper.cc
    Button.cc
    Decoration.cc
    MenuOverflowButton.cc
    TextButton.cc
    plugin.cc
)

kconfig_add_kcfg_files(decoration_SRCS
    InternalSettings.kcfgc
)

add_library (materialdecoration MODULE
    ${decoration_SRCS}
)

target_link_libraries (materialdecoration
    PUBLIC
        dbusmenuqt
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        # Qt${QT_VERSION_MAJOR}::X11Extras
        KF6::ConfigCore
        KF6::ConfigGui
        KF6::ConfigWidgets
        KF6::CoreAddons
        KF6::I18n
        KF6::GuiAddons
        KF6::IconThemes
        KF6::WindowSystem
        KF6::KCMUtils
        ${X11_LIBRARIES}

#        ${Wayland_LIBRARIES}

    PRIVATE
        KDecoration3::KDecoration
        kwin
)
if (Qt6_FOUND)
    # The QX11Info class has been removed.
    target_link_libraries (materialdecoration
        PUBLIC
            Qt${QT_VERSION_MAJOR}::GuiPrivate
    )
elseif(Qt5_FOUND)
    target_link_libraries (materialdecoration
        PUBLIC
            Qt${QT_VERSION_MAJOR}::X11Extras
    )
endif()

include(KDEInstallDirs)
install(TARGETS materialdecoration
        DESTINATION ${KDE_INSTALL_PLUGINDIR}/org.kde.kdecoration3)

#install (TARGETS materialdecoration
#         DESTINATION ${PLUGIN_INSTALL_DIR}/org.kde.kdecoration3)
