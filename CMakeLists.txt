
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
    # Add Windows icon resource
    if(EXISTS ${CMAKE_SOURCE_DIR}/usbpower.ico)
        configure_file(${CMAKE_SOURCE_DIR}/usbc-power-osd.exe.manifest ${CMAKE_BINARY_DIR}/usbc-power-osd.exe.manifest COPYONLY)
        set(WIN32_ICON_RC "${CMAKE_BINARY_DIR}/icon.rc")
        file(WRITE ${WIN32_ICON_RC} "IDI_ICON1 ICON DISCARDABLE \"${CMAKE_SOURCE_DIR}/usbpower.ico\"\n")
        target_sources(usb-power-osd PRIVATE ${WIN32_ICON_RC})
    endif()
    
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
    
    # Install all DLLs and plugins from the build directory after windeployqt runs
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Release/
            DESTINATION .
            FILES_MATCHING
            PATTERN "*.dll"
            PATTERN "platforms/*"
            PATTERN "styles/*"
            PATTERN "iconengines/*"
            PATTERN "imageformats/*"
            PATTERN "*.exe" EXCLUDE  # Don't duplicate the exe
    )
    
    # Install MSVC runtime libraries to root directory (not bin)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
elseif(APPLE)
    install(TARGETS usb-power-osd
            BUNDLE DESTINATION .
    )
else()
    # Linux install paths
    install(TARGETS usb-power-osd
            RUNTIME DESTINATION bin
    )
    
    # Install desktop file
    if(EXISTS ${CMAKE_SOURCE_DIR}/usbc-power-osd.desktop)
        install(FILES ${CMAKE_SOURCE_DIR}/usbc-power-osd.desktop
                DESTINATION share/applications
                RENAME usb-power-osd.desktop
        )
    endif()
    
    # Install icon
    if(EXISTS ${CMAKE_SOURCE_DIR}/usb-power-osd.png)
        install(FILES ${CMAKE_SOURCE_DIR}/usb-power-osd.png
                DESTINATION share/icons/hicolor/256x256/apps
        )
    endif()
endif()

# Packaging with CPack
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "USB Power OSD")
set(CPACK_PACKAGE_VENDOR "MacWake.de")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "USB Power OSD - USB power monitoring and OSD application")
set(CPACK_PACKAGE_DESCRIPTION "Monitor USB power consumption in real-time with an on-screen display. Supports USB-C power monitoring devices via Bluetooth and Serial connections.")
set(CPACK_PACKAGE_CONTACT "https://github.com/MacWake/usb-power-osd-qt")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/MacWake/usb-power-osd-qt")

if (WIN32)
    set(CPACK_GENERATOR "WIX")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "USB Power OSD")
    set(CPACK_WIX_UPGRADE_GUID "12345678-90AB-CDEF-1234-567890ABCDEF")
    set(CPACK_PACKAGE_EXECUTABLES "USB-Power-OSD" "USB Power OSD")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "USB Power OSD")
    
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
    
elseif(UNIX AND NOT APPLE)
    # Linux packaging - generate both DEB and RPM
    set(CPACK_GENERATOR "DEB;RPM")
    
    # Common Linux settings
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "/usr")
    
    # DEB-specific settings
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "MacWake.de <noreply@macwake.de>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
    
    # DEB dependencies for Ubuntu/Debian
    set(CPACK_DEBIAN_PACKAGE_DEPENDS 
        "qt6-base-dev, qt6-serialport-dev, qt6-connectivity-dev, libqt6core6t64, libqt6gui6, libqt6widgets6, libqt6serialport6, libqt6bluetooth6, libgl1-mesa-glx | libgl1, libxkbcommon-x11-0"
    )
    
    # Enable automatic dependency detection as fallback
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    
    # RPM-specific settings
    set(CPACK_RPM_PACKAGE_LICENSE "LGPL-3.0")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
    set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
    
    # RPM dependencies for Fedora/RHEL/openSUSE
    set(CPACK_RPM_PACKAGE_REQUIRES
        "qt6-qtbase, qt6-qtserialport, qt6-qtconnectivity, mesa-libGL"
    )
    
    # Enable automatic dependency detection
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
    
    # Package file names
    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")  # usb-power-osd_2.0.0_amd64.deb
    set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")     # usb-power-osd-2.0.0.x86_64.rpm
    
endif()

include(CPack)
