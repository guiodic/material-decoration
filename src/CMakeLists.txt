find_package (KDecoration3 REQUIRED)

find_package (Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Svg
    # GuiTools
)

find_package (KF6 REQUIRED COMPONENTS
    Config
    ConfigWidgets
    CoreAddons
    GuiAddons
    I18n
    IconThemes
    WindowSystem
    #KCMUtils
)

find_package(KWin REQUIRED)
find_package(KWinX11 QUIET)

# X11
find_package(X11 QUIET)
find_package(XCB MODULE QUIET COMPONENTS
    XCB
    # RANDR
)

if(KWinX11_FOUND AND X11_FOUND AND XCB_FOUND)
    set(HAVE_X11 ON)
    #include_directories(/usr/include/kwin-x11)
    #include_directories(materialdecoration PRIVATE
    #     $<TARGET_PROPERTY:KWinX11::kwin,INTERFACE_INCLUDE_DIRECTORIES>
    #)
    set(X11_LIBRARIES XCB::XCB)
    message(STATUS "Building with X11 support")
else()
    set(HAVE_X11 OFF)
    message(STATUS "Building without X11 support")
endif()

#set(HAVE_KDecoration3_5_25 ON)


configure_file(BuildConfig.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/BuildConfig.h)

set (decoration_SRCS
    AppMenuModel.cc
    NavigableMenu.cc
    AppMenuButton.cc
    AppMenuButtonGroup.cc
    BoxShadowHelper.cc
    Button.cc
    Decoration.cc
    IconProvider.cc
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
        materialdecoration_core
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Svg
        # Qt6::GuiPrivate
        # Qt${QT_VERSION_MAJOR}::X11Extras
        KF6::ConfigCore
        KF6::ConfigGui
        KF6::ConfigWidgets
        KF6::CoreAddons
        KF6::I18n
        KF6::GuiAddons
        KF6::IconThemes
        KF6::WindowSystem
        # KF6::KCMUtils
        ${X11_LIBRARIES}     
#        ${Wayland_LIBRARIES}

    PRIVATE
        KDecoration3::KDecoration
        KWin::kwin
)

if(KWinX11_FOUND)
    #target_link_libraries(materialdecoration PRIVATE KWinX11::kwin)
endif()

# Core library with shared code (e.g., settings)
set(core_SRCS)
kconfig_add_kcfg_files(core_SRCS InternalSettings.kcfgc)
add_library(materialdecoration_core STATIC ${core_SRCS})
target_link_libraries(materialdecoration_core PUBLIC
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::ConfigWidgets
)

set_property(TARGET materialdecoration_core PROPERTY POSITION_INDEPENDENT_CODE ON)

include(KDEInstallDirs)
install(TARGETS materialdecoration
        DESTINATION ${KDE_INSTALL_PLUGINDIR}/org.kde.kdecoration3)
        
add_subdirectory(kcm)

