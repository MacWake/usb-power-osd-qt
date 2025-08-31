
cmake_minimum_required(VERSION 3.16)

project(usb-power-osd VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS
        Core
        Widgets
        Bluetooth
        SerialPort
)

# Enable automoc for Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Source files
set(SOURCES
        src/Main.cpp
        src/MainWindow.cpp
        src/PowerMonitor.cpp
        src/BluetoothManager.cpp
        src/SerialManager.cpp
        src/DeviceManager.cpp
        src/OsdSettings.cpp
        src/SettingsDialog.cpp
        src/PowerDelivery.cpp
)

set(HEADERS
        src/MainWindow.h
        src/PowerMonitor.h
        src/BluetoothManager.h
        src/SerialManager.h
        src/DeviceManager.h
        src/OsdSettings.h
        src/SettingsDialog.h
        src/PowerDelivery.h
)

set(UI_FILES
        ui/Settings.ui
)

set(RESOURCES
        resources/resources.qrc
)

# Create executable
qt_add_executable(usb-power-osd ${SOURCES} ${HEADERS} ${UI_FILES} ${RESOURCES})

# Link Qt libraries
target_link_libraries(usb-power-osd PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Bluetooth
        Qt6::SerialPort
        Qt6::Network
)

# Include directories
target_include_directories(usb-power-osd PRIVATE src)

# Platform-specific settings
if (WIN32)
    set_target_properties(usb-power-osd PROPERTIES
            WIN32_EXECUTABLE TRUE
    )
endif ()

if (APPLE)
    set_target_properties(usb-power-osd PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    )
endif ()

# Finalize the executable (this replaces qt_finalize_executable)
qt6_finalize_target(usb-power-osd)

# Install
install(TARGETS usb-power-osd
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
)
