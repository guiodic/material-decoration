cmake_minimum_required(VERSION 3.16)

project(DynamicMenuTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

add_executable(DynamicMenuTest
    main.cpp
    mainwindow.cpp
    mainwindow.h
    propertydialog.cpp
    propertydialog.h
)

target_link_libraries(DynamicMenuTest PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)
