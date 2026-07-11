cmake_minimum_required(VERSION 3.21)

project(Faraday
    VERSION 1.0.0
    DESCRIPTION "Faraday - Battery Intelligence Suite"
    LANGUAGES CXX
)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Sql Widgets LinguistTools)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

# ---------------------------------------------------------------------------
# faradaylib: acquisition + core logic, shared by the app and the tests
# ---------------------------------------------------------------------------
qt_add_library(faradaylib STATIC
    src/acquisition/BatteryTypes.h
    src/acquisition/WmiClient.h
    src/acquisition/WmiClient.cpp
    src/acquisition/BatteryReader.h
    src/acquisition/BatteryReader.cpp
    src/acquisition/PowercfgReport.h
    src/acquisition/PowercfgReport.cpp
    src/core/Metrics.h
    src/core/Metrics.cpp
    src/core/HealthVerdict.h
    src/core/HealthVerdict.cpp
    src/data/Database.h
    src/data/Database.cpp
    src/data/Settings.h
    src/data/Settings.cpp
    src/app/Sampler.h
    src/app/Sampler.cpp
    src/app/BatteryModel.h
    src/app/BatteryModel.cpp
    src/app/AlertManager.h
    src/app/AlertManager.cpp
    src/app/Autostart.h
    src/app/Autostart.cpp
    src/app/ChargeCap.h
    src/app/ChargeCap.cpp
    src/app/TrayManager.h
    src/app/TrayManager.cpp
    src/app/Calibration.h
    src/app/Calibration.cpp
    src/app/Exporter.h
    src/app/Exporter.cpp
)
target_include_directories(faradaylib PUBLIC src)
target_link_libraries(faradaylib PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
    Qt6::Widgets
    ole32
    oleaut32
    wbemuuid
)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
qt_add_executable(faraday
    src/main.cpp
    resources/faraday.rc
)

set_source_files_properties(ui/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(faraday
    URI Faraday
    VERSION 1.0
    QML_FILES
        ui/Main.qml
        ui/Theme.qml
        ui/components/HealthRing.qml
        ui/components/MetricCard.qml
        ui/pages/DashboardPage.qml
        ui/pages/MonitorPage.qml
        ui/pages/HistoryPage.qml
        ui/pages/AlertsPage.qml
        ui/pages/CalibrationPage.qml
        ui/pages/SettingsPage.qml
)

# i18n: English ships as the source language; additional languages only
# need new .ts files here. The .qm files are embedded under :/i18n/.
qt_add_translations(faraday
    TS_FILES i18n/faraday_en.ts
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -no-obsolete
)

set_target_properties(faraday PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(faraday PRIVATE
    faradaylib
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

enable_testing()
add_subdirectory(tests)
