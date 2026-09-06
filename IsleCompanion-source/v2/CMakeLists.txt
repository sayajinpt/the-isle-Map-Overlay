cmake_minimum_required(VERSION 3.21)
project(IsleCompanionV2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Gui)

set(V2_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/core/Settings.cpp
    src/core/AppState.cpp
    src/core/Paths.cpp
    src/core/CoordinateTransform.cpp
    src/core/CoordinateParser.cpp
    src/core/LayerRepository.cpp
    src/core/PrimeRun.cpp
    src/core/Navigation.cpp
    src/core/OverlayInteraction.cpp
    src/core/InteractionKeyMonitor.cpp
    src/tracking/ClipboardMonitor.cpp
    src/tracking/BoschBridge.cpp
    src/tracking/OcrTracker.cpp
    src/ui/MiniMapWindow.cpp
    src/ui/OptionsWindow.cpp
    src/ui/MapView.cpp
    src/ui/DataSourcePanel.cpp
    src/ui/OcrRegionDialog.cpp
    src/ui/CalibrationDialog.cpp
    src/ui/ServerChooserDialog.cpp
)

add_executable(IsleCompanionV2 WIN32 ${V2_SOURCES})

target_include_directories(IsleCompanionV2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(IsleCompanionV2 PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Gui
)

if(WIN32)
    target_compile_definitions(IsleCompanionV2 PRIVATE ISLE_COMPANION_WINDOWS=1)
endif()

# Shared assets live in the parent Python/v1 tree.
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.." ISLE_ASSET_ROOT)
# Escape backslashes for the compiler string literal.
string(REPLACE "\\" "/" ISLE_ASSET_ROOT_POSIX "${ISLE_ASSET_ROOT}")
target_compile_definitions(IsleCompanionV2 PRIVATE
    "ISLE_ASSET_ROOT=\"${ISLE_ASSET_ROOT_POSIX}\""
)

install(TARGETS IsleCompanionV2 RUNTIME DESTINATION bin)
