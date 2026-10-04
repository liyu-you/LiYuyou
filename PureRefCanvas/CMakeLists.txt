cmake_minimum_required(VERSION 3.16)
project(PureRefCanvas VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Concurrent: off-thread decoding for the import path (see imageio.h).
# PrintSupport: PDF export from the notes editor (QPrinter).
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Network OpenGL OpenGLWidgets Concurrent Sql PrintSupport)

qt_add_executable(PureRefCanvas
    main.cpp
    mainwindow.h mainwindow.cpp
    canvasview.h canvasview.cpp
    canvasscene.h canvasscene.cpp
    imageitem.h imageitem.cpp
    imageformats.h
    imageio.h
    notewidget.h notewidget.cpp
    materiallibrary.h materiallibrary.cpp
    materialwidget.h materialwidget.cpp
    contentbrowserpanel.h contentbrowserpanel.cpp
    userauth.h userauth.cpp
    logindialog.h logindialog.cpp
    accountdialog.h accountdialog.cpp
    booklibrary.h booklibrary.cpp
    readerwidget.h readerwidget.cpp
    musicplayer.h musicplayer.cpp
    songinfo.h songmetadata.h songmetadata.cpp
    musiclibrarydb.h musiclibrarydb.cpp
    cueparser.h cueparser.cpp
    gaplessengine.h gaplessengine.cpp
    maengine.h maengine.cpp
    coverart.h
    playpausebutton.h
    playerwidgets.h
    miniplayer.h miniplayer.cpp
    fullscreenplayer.h fullscreenplayer.cpp
    settingsdialog.h settingsdialog.cpp
    annotationitems.h annotationitems.cpp
    updatechecker.h updatechecker.cpp
    resources.qrc
    app.rc
)

target_compile_definitions(PureRefCanvas PRIVATE APP_VERSION="${PROJECT_VERSION}")

target_link_libraries(PureRefCanvas PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::Network Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent Qt6::Sql Qt6::PrintSupport)

if(WIN32)
    # dwmapi: acrylic blur / rounded corners. crypt32: DPAPI, used to protect the
    # password saved by "记住密码" with the current Windows user account.
    # winmm/ole32: miniaudio WASAPI backend.
    target_link_libraries(PureRefCanvas PRIVATE dwmapi crypt32 winmm ole32)
    # OUTPUT_NAME must stay ASCII: CMake writes build.ninja in the ANSI codepage
    # while ninja reads it as UTF-8, so a non-ASCII target name breaks the
    # MANIFESTS check ("...manifest missing"). package.ps1 deploys the exe as
    # 里与由.exe, which keeps the shipped product name unchanged.
    set_target_properties(PureRefCanvas PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "LiyuYou"
    )
endif()
