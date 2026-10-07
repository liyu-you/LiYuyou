cmake_minimum_required(VERSION 3.16)
project(TrayTest LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SRC "C:/Users/LiyuYou/Desktop/Q/PureRefCanvas")

qt_add_executable(traytest
    traytest.cpp
    ${SRC}/traycontroller.h ${SRC}/traycontroller.cpp
    ${SRC}/traymenustyle.h
)
target_include_directories(traytest PRIVATE ${SRC})
# so the header version label renders in the preview exactly as in production
target_compile_definitions(traytest PRIVATE APP_VERSION="1.0.0")
target_link_libraries(traytest PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
