cmake_minimum_required(VERSION 3.16)

project(AudioFlux VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Поиск Qt c компонентами Widgets, DBus и OpenGL
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets DBus OpenGL LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets DBus OpenGL LinguistTools)

# 2. Поиск Wayland Layer Shell и системных библиотек
find_package(LayerShellQt REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse-simple)
pkg_check_modules(FFTW REQUIRED IMPORTED_TARGET fftw3)

set(TS_FILES AudioFlux_ru_BY.ts)

set(PROJECT_SOURCES
    main.cpp
    AudioEngine.h
    ThemeWatcher.h
    MprisWatcher.h
    visualizerwindow.h
    visualizerwindow.cpp
    ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(AudioFlux
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(AudioFlux SHARED ${PROJECT_SOURCES})
    else()
        add_executable(AudioFlux ${PROJECT_SOURCES})
    endif()
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# 3. Линковка библиотек
target_link_libraries(AudioFlux PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::DBus
    Qt${QT_VERSION_MAJOR}::OpenGL
    LayerShellQtInterface
    PkgConfig::PULSE
    PkgConfig::FFTW
    m
)

set_target_properties(AudioFlux PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS AudioFlux
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(AudioFlux)
endif()