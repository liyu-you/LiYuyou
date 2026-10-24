# NoteWidget regression tests (Word-style rich text features).
# Offscreen: configure once, then run via run.ps1 (ninja + offscreen exe).
cmake_minimum_required(VERSION 3.16)
project(NoteTest LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

set(SRC "C:/Users/Liyuyou/Desktop/lyy/Q/PureRefCanvas")

qt_add_executable(notetest
    notetest.cpp
    ${SRC}/notewidget.h ${SRC}/notewidget.cpp
    ${SRC}/spacemanager.h ${SRC}/spacemanager.cpp
)
target_include_directories(notetest PRIVATE ${SRC})
target_link_libraries(notetest PRIVATE Qt6::Widgets Qt6::PrintSupport)

# Editor-page screenshot probe (visual check of the toolbar/layout).
qt_add_executable(edshot
    edshot.cpp
    ${SRC}/notewidget.h ${SRC}/notewidget.cpp
    ${SRC}/spacemanager.h ${SRC}/spacemanager.cpp
)
target_include_directories(edshot PRIVATE ${SRC})
target_link_libraries(edshot PRIVATE Qt6::Widgets Qt6::PrintSupport)
