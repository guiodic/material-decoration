find_package (KDecoration3 REQUIRED)

find_package (Qt6 REQUIRED COMPONENTS
    Core
    Gui
    GuiTools
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

if(X11_FOUND AND XCB_XCB_FOUND)
    set(HAVE_X11 ON)
    set(X11_LIBRARIES XCB::XCB)
endif()

set(HAVE_KDecoration3_5_25 ON)


configure_file(BuildConfig.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/BuildConfig.h)

set (decoration_SRCS
    AppMenuModel.cc
    AppMenuButton.cc
    AppMenuButtonGroup.cc
    BoxShadowHelper.cc
    Button.cc
    Decoration.cc
    MenuOverflowButton.cc
    SearchButton.cc
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
        Qt6::Core
        Qt6::Gui
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
# The QX11Info class has been removed.
target_link_libraries (materialdecoration
    PUBLIC
        Qt6::GuiPrivate
)

include(KDEInstallDirs)
install(TARGETS materialdecoration
        DESTINATION ${KDE_INSTALL_PLUGINDIR}/org.kde.kdecoration3)

#install (TARGETS materialdecoration
#         DESTINATION ${PLUGIN_INSTALL_DIR}/org.kde.kdecoration3)
