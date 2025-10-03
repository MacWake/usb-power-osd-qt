
cmake_minimum_required(VERSION 3.21.1)

project(usb-power-osd VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(APP_NAME "USB Power OSD")
set(APP_DISPLAY_NAME "USB Power OSD")
set(BUNDLE_IDENTIFIER "de.macwake.usb-power-osd")
set(EXECUTABLE_NAME "USB-Power-OSD")
set(ICON_FILE "usbpower.icns")
set(MIN_MACOS_VERSION "10.15")
set(BLUETOOTH_USAGE_DESCRIPTION "This app uses Bluetooth to communicate with USB power monitoring devices.")
set(COPYRIGHT_NOTICE "Copyright © 2025 MacWake.de. All rights reserved.")

set(BUILD_NUMBER "1")

# Configure Info.plist from template
configure_file(
    ${CMAKE_SOURCE_DIR}/Info.plist.in
    ${CMAKE_BINARY_DIR}/Info.plist
    @ONLY
)

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
        src/BluetoothManager.cpp
        src/CurrentGraph.cpp
        src/DeviceManager.cpp
        src/DeviceSelectionDialog.cpp
        src/Main.cpp
        src/MainWindow.cpp
        src/MeasurementHistory.cpp
        src/OsdSettings.cpp
        src/PowerDelivery.cpp
        src/PowerMonitor.cpp
        src/SerialManager.cpp
        src/SettingsDialog.cpp
        src/AboutDialog.cpp
        src/AboutDialog.h
        src/lgplv3.cpp
        src/lgplv3.h
)

set(HEADERS
        src/BluetoothManager.h
        src/CurrentGraph.h
        src/DeviceManager.h
        src/DeviceSelectionDialog.h
        src/MainWindow.h
        src/MeasurementHistory.h
        src/OsdSettings.h
        src/PowerData.h
        src/PowerDelivery.h
        src/PowerMonitor.h
        src/SerialManager.h
        src/SettingsDialog.h
)

set(RESOURCES
        resources/resources.qrc
)

qt_add_executable(usb-power-osd ${SOURCES} ${HEADERS} ${UI_FILES} ${RESOURCES})

target_link_libraries(usb-power-osd PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Bluetooth
        Qt6::SerialPort
        Qt6::Network
)

target_include_directories(usb-power-osd PRIVATE src)

if (WIN32)
    set_target_properties(usb-power-osd PROPERTIES
            WIN32_EXECUTABLE TRUE
            OUTPUT_NAME ${EXECUTABLE_NAME}
    )
endif ()

if (APPLE)
    # Copy the icon file to the bundle resources
    set_source_files_properties(${CMAKE_SOURCE_DIR}/${ICON_FILE} 
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    
    target_sources(usb-power-osd PRIVATE ${CMAKE_SOURCE_DIR}/${ICON_FILE})
    
    set_target_properties(usb-power-osd PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "${APP_NAME}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_IDENTIFIER "${BUNDLE_IDENTIFIER}"
            MACOSX_BUNDLE_EXECUTABLE_NAME "${EXECUTABLE_NAME}"
            MACOSX_BUNDLE_ICON_FILE "${ICON_FILE}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${BUNDLE_IDENTIFIER}"
            MACOSX_BUNDLE_COPYRIGHT "${COPYRIGHT_NOTICE}"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/Info.plist"
            OUTPUT_NAME ${EXECUTABLE_NAME}
    )
endif ()

# For other platforms (Linux, etc.)
if (NOT WIN32 AND NOT APPLE)
    set_target_properties(usb-power-osd PROPERTIES
            OUTPUT_NAME ${EXECUTABLE_NAME}
    )
endif ()

# Install configuration
if (WIN32)
    install(TARGETS usb-power-osd
            RUNTIME DESTINATION .
    )
    
    if(EXISTS ${CMAKE_SOURCE_DIR}/deploy/windows)
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/deploy/windows/
                DESTINATION .
                FILES_MATCHING
                PATTERN "*.dll"
                PATTERN "platforms/*"
                PATTERN "styles/*"
                PATTERN "iconengines/*"
                PATTERN "imageformats/*"
        )
    endif()
else()
    install(TARGETS usb-power-osd
            BUNDLE DESTINATION .
            RUNTIME DESTINATION bin
    )
endif()

# Packaging with CPack
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "USB Power OSD")
set(CPACK_PACKAGE_VENDOR "MacWake.de")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "USB Power OSD - USB power monitoring and OSD application")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "USB Power OSD")

if (WIN32)
    set(CPACK_GENERATOR "WIX")
    
    # Set upgrade GUID for proper upgrade handling
    set(CPACK_WIX_UPGRADE_GUID "12345678-90AB-CDEF-1234-567890ABCDEF")
    
    # Configure Start Menu shortcuts using CPack's built-in mechanism
    set(CPACK_PACKAGE_EXECUTABLES "USB-Power-OSD" "USB Power OSD")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "USB Power OSD")
    
    # Optional: License file
    # set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
endif()

include(CPack)
