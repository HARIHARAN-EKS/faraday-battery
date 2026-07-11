cmake_minimum_required(VERSION 3.21)

project(Faraday
    VERSION 0.1.0
    DESCRIPTION "Faraday - Battery Intelligence Suite"
    LANGUAGES CXX
)

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

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick)

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
)
target_include_directories(faradaylib PUBLIC src)
target_link_libraries(faradaylib PUBLIC
    Qt6::Core
    ole32
    oleaut32
    wbemuuid
)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
qt_add_executable(faraday
    src/main.cpp
)

qt_add_qml_module(faraday
    URI Faraday
    VERSION 1.0
    QML_FILES
        ui/Main.qml
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
