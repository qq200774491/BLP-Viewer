cmake_minimum_required(VERSION 3.16)

project(blp_qt_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets QUIET)
if (Qt6_FOUND)
    set(QT_PACKAGE Qt6)
    qt_standard_project_setup()
else()
    find_package(Qt5 COMPONENTS Widgets REQUIRED)
    set(QT_PACKAGE Qt5)
endif()

add_executable(blp_viewer
    src/main.cpp
    src/mainwindow.cpp
    src/image_io.cpp
    src/blp_api.cpp
    src/imageview.cpp
)

target_include_directories(blp_viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(blp_viewer PRIVATE ${QT_PACKAGE}::Widgets)
