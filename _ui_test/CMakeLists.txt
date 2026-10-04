cmake_minimum_required(VERSION 3.16)
project(AccountTest LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SRC "C:/Users/LiyuYou/Desktop/Q/PureRefCanvas")

qt_add_executable(accounttest
    accounttest.cpp
    ${SRC}/accountdialog.h ${SRC}/accountdialog.cpp
    ${SRC}/userauth.h ${SRC}/userauth.cpp
)
target_include_directories(accounttest PRIVATE ${SRC})
target_link_libraries(accounttest PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
