cmake_minimum_required(VERSION 3.16)
project(ProbeTest LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Network OpenGL OpenGLWidgets Concurrent)

set(SRC "C:/Users/LiyuYou/Desktop/Q/PureRefCanvas")

qt_add_executable(mw_probe
    mw_probe.cpp
    ${SRC}/mainwindow.h ${SRC}/mainwindow.cpp
    ${SRC}/canvasview.h ${SRC}/canvasview.cpp
    ${SRC}/canvasscene.h ${SRC}/canvasscene.cpp
    ${SRC}/imageitem.h ${SRC}/imageitem.cpp
    ${SRC}/imageformats.h
    ${SRC}/imageio.h
    ${SRC}/notewidget.h ${SRC}/notewidget.cpp
    ${SRC}/materiallibrary.h ${SRC}/materiallibrary.cpp
    ${SRC}/materialwidget.h ${SRC}/materialwidget.cpp
    ${SRC}/contentbrowserpanel.h ${SRC}/contentbrowserpanel.cpp
    ${SRC}/userauth.h ${SRC}/userauth.cpp
    ${SRC}/logindialog.h ${SRC}/logindialog.cpp
    ${SRC}/accountdialog.h ${SRC}/accountdialog.cpp
    ${SRC}/booklibrary.h ${SRC}/booklibrary.cpp
    ${SRC}/readerwidget.h ${SRC}/readerwidget.cpp
    ${SRC}/musicplayer.h ${SRC}/musicplayer.cpp
    ${SRC}/songinfo.h ${SRC}/songmetadata.h ${SRC}/songmetadata.cpp
    ${SRC}/coverart.h
    ${SRC}/playpausebutton.h
    ${SRC}/playerwidgets.h
    ${SRC}/miniplayer.h ${SRC}/miniplayer.cpp
    ${SRC}/fullscreenplayer.h ${SRC}/fullscreenplayer.cpp
    ${SRC}/settingsdialog.h ${SRC}/settingsdialog.cpp
    ${SRC}/annotationitems.h ${SRC}/annotationitems.cpp
    ${SRC}/updatechecker.h ${SRC}/updatechecker.cpp
    ${SRC}/resources.qrc
)
target_include_directories(mw_probe PRIVATE ${SRC})
target_link_libraries(mw_probe PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent dwmapi crypt32)

# Regression test: constructing ReaderWidget must not crash in eventFilter()
# when setProperty() delivers DynamicPropertyChange while m_readingView is null.
qt_add_executable(readerwidget_test
    readerwidget_test.cpp
    ${SRC}/booklibrary.h ${SRC}/booklibrary.cpp
    ${SRC}/readerwidget.h ${SRC}/readerwidget.cpp
    ${SRC}/resources.qrc
)
target_include_directories(readerwidget_test PRIVATE ${SRC})
target_link_libraries(readerwidget_test PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent dwmapi crypt32)
