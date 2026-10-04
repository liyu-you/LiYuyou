cmake_minimum_required(VERSION 3.16)
project(AuthTest LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

set(SRC "C:/Users/LiyuYou/Desktop/Q/PureRefCanvas")

qt_add_executable(authtest
    authtest.cpp
    ${SRC}/userauth.h ${SRC}/userauth.cpp
)
target_include_directories(authtest PRIVATE ${SRC})
target_link_libraries(authtest PRIVATE Qt6::Core)
