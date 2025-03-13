cmake_minimum_required(VERSION 3.5)

project(RatingSystemSimulation VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql Charts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        databasemanager.h
        databasemanager.cpp
        mainwindow.h
        mainwindow.ui
        glickoratingssystem.h glickoratingssystem.cpp
        gamegenerator.h gamegenerator.cpp
        gamegeneratorthread.h gamegeneratorthread.cpp
        importdatabasethread.h importdatabasethread.cpp
        playerinfowindow.h playerinfowindow.cpp playerinfowindow.ui
        gameinfowindow.h gameinfowindow.cpp gameinfowindow.ui
        ratingdistributionanalyzer.h ratingdistributionanalyzer.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(RatingSystemSimulation
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        databasemanager.h databasemanager.cpp

    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET RatingSystemSimulation APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(RatingSystemSimulation SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(RatingSystemSimulation
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(RatingSystemSimulation PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::Charts)
# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.RatingSystemSimulation)
endif()
set_target_properties(RatingSystemSimulation PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS RatingSystemSimulation
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(RatingSystemSimulation)
endif()
