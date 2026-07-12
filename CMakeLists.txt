cmake_minimum_required(VERSION 3.21.1)

# Use override version if provided, otherwise use default
if(DEFINED PROJECT_VERSION_OVERRIDE)
    project(usb-power-osd VERSION ${PROJECT_VERSION_OVERRIDE} LANGUAGES CXX)
else()
    project(usb-power-osd VERSION 2.0.0 LANGUAGES CXX)
endif()

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
        Multimedia
        Test
)

# Enable automoc for Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Optional LLVM/Clang coverage instrumentation for the test target.
option(USBPOWEROSD_ENABLE_COVERAGE
       "Build usb-power-osd-tests with LLVM coverage instrumentation (Clang/AppleClang only)" OFF)

# Unit tests
set(TEST_SOURCES
        tests/main.cpp
        tests/test_v2_ble_protocol.cpp
        tests/test_v2_ble_protocol.h
        tests/test_serial_pld_protocol.cpp
        tests/test_serial_pld_protocol.h
        tests/test_measurement_pipeline.cpp
        tests/test_measurement_pipeline.h
)

set(TEST_LIB_SOURCES
        src/BluetoothManager.cpp
        src/BluetoothManager.h
        src/MeasurementPipeline.cpp
        src/MeasurementPipeline.h
        src/PowerDelivery.cpp
        src/PowerDelivery.h
        src/PowerMonitor.cpp
        src/PowerMonitor.h
        src/SerialManager.cpp
        src/SerialManager.h
        src/PowerData.h
        src/PowerDataSource.h
)

enable_testing()

add_executable(usb-power-osd-tests ${TEST_SOURCES} ${TEST_LIB_SOURCES})
target_link_libraries(usb-power-osd-tests PRIVATE
        Qt6::Core
        Qt6::Bluetooth
        Qt6::SerialPort
        Qt6::Test
)
target_include_directories(usb-power-osd-tests PRIVATE src)
add_test(NAME usb-power-osd-tests COMMAND usb-power-osd-tests)

if(USBPOWEROSD_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(usb-power-osd-tests PRIVATE
                -fprofile-instr-generate
                -fcoverage-mapping
                -O0
        )
        target_link_options(usb-power-osd-tests PRIVATE
                -fprofile-instr-generate
        )
        message(STATUS "LLVM coverage instrumentation enabled for usb-power-osd-tests")
    else()
        message(WARNING "USBPOWEROSD_ENABLE_COVERAGE is only supported with Clang/AppleClang; ignoring for ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

# Convenience target that runs the coverage report script.
add_custom_target(coverage
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/generate_coverage.sh" "${CMAKE_BINARY_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating HTML coverage report in ${CMAKE_BINARY_DIR}/coverage-html"
)

# Source files
set(SOURCES
        src/BluetoothManager.cpp
        src/CurrentGraph.cpp
        src/DeviceManager.cpp
        src/DeviceSelectionDialog.cpp
        src/Main.cpp
        src/MainWindow.cpp
        src/MeasurementHistory.cpp
        src/MeasurementPipeline.cpp
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
        src/MeasurementPipeline.h
        src/OsdSettings.h
        src/PowerData.h
        src/PowerDelivery.h
        src/PowerMonitor.h
        src/PowerDataSource.h
        src/SerialManager.h
        src/SettingsDialog.h
        src/AudioGenerator.h
)

set(RESOURCES
        resources/resources.qrc
)

# Python is needed to convert the Markdown user manual to HTML
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Convert user manual Markdown to HTML at build time
set(USER_MANUAL_MD "${CMAKE_SOURCE_DIR}/docs/USER_MANUAL.md")
set(USER_MANUAL_HTML "${CMAKE_BINARY_DIR}/USER_MANUAL.html")
set(USER_MANUAL_CONVERTER "${CMAKE_SOURCE_DIR}/docs/md2html.py")

add_custom_command(
    OUTPUT ${USER_MANUAL_HTML}
    COMMAND ${CMAKE_COMMAND} -E echo "Converting user manual to HTML..."
    COMMAND Python3::Interpreter ${USER_MANUAL_CONVERTER} ${USER_MANUAL_MD} ${USER_MANUAL_HTML}
    DEPENDS ${USER_MANUAL_MD} ${USER_MANUAL_CONVERTER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building user manual HTML"
)

add_custom_target(user_manual_html ALL DEPENDS ${USER_MANUAL_HTML})

qt_add_executable(usb-power-osd ${SOURCES} ${HEADERS} ${UI_FILES} ${RESOURCES})

target_link_libraries(usb-power-osd PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Bluetooth
        Qt6::SerialPort
        Qt6::Network
        Qt6::Multimedia
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

    # Install user manual next to the executable on Windows
    install(FILES ${USER_MANUAL_HTML}
            DESTINATION .
            RENAME USER_MANUAL.html
    )
elseif(APPLE)
    # Copy the user manual into the app bundle resources
    set_source_files_properties(${USER_MANUAL_HTML}
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(usb-power-osd PRIVATE ${USER_MANUAL_HTML})

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

    # Install user manual
    install(FILES ${USER_MANUAL_HTML}
            DESTINATION share/doc/usb-power-osd
            RENAME USER_MANUAL.html
    )

    # Install desktop shortcut to open the user manual
    if(EXISTS ${CMAKE_SOURCE_DIR}/usb-power-osd-manual.desktop)
        install(FILES ${CMAKE_SOURCE_DIR}/usb-power-osd-manual.desktop
                DESTINATION share/applications
        )
    endif()
endif()

# Packaging with CPack
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "usb-power-osd")
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
    
    # Configure WiX to use the executable's icon for shortcuts
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/usbpower.ico")
    set(CPACK_WIX_UI_BANNER "${CMAKE_CURRENT_SOURCE_DIR}/usbpower.ico")
    
    # Use the license file
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
    
elseif(UNIX AND NOT APPLE)
    # Linux packaging - generate both DEB and RPM
    set(CPACK_GENERATOR "DEB;RPM")
    
    # Common Linux settings
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "/usr")
    
    # DEB-specific settings
    set(CPACK_DEBIAN_PACKAGE_NAME "usb-power-osd")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "MacWake.de <noreply@macwake.de>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
    
    # DEB dependencies for Ubuntu/Debian
    if(LINUX_DISTRO STREQUAL "ubuntu-24.04")
        # Ubuntu 24.04 renamed core/gui/widgets Qt6 packages to the t64 ABI suffix
        set(CPACK_DEBIAN_PACKAGE_DEPENDS
            "libqt6core6t64, libqt6gui6t64, libqt6widgets6t64, libqt6serialport6, libqt6bluetooth6, libqt6multimedia6, libgl1, libxkbcommon-x11-0"
        )
    else()
        # Debian 12 (Bookworm) and 13 (Trixie) keep the original Qt6 package names
        set(CPACK_DEBIAN_PACKAGE_DEPENDS
            "libqt6core6, libqt6gui6, libqt6widgets6, libqt6serialport6, libqt6bluetooth6, libqt6multimedia6, libgl1, libxkbcommon-x11-0"
        )
    endif()

    # Do not mix automatic shlibdeps with the manually curated list above;
    # it produces conflicting versioned dependencies on distros with ABI renames.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
    
    # RPM-specific settings
    set(CPACK_RPM_PACKAGE_NAME "usb-power-osd")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
    set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
    
    # RPM dependencies for Fedora/RHEL/openSUSE
    set(CPACK_RPM_PACKAGE_REQUIRES
        "qt6-qtbase-gui, qt6-qtbase-widgets, qt6-qtserialport, qt6-qtconnectivity, qt6-qtmultimedia, mesa-libGL"
    )

    # Let rpm auto-detect soname deps as a safety net, but keep the explicit Qt6 list above.
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
    
    # Package file names. Use per-generator versions so prereleases can use
    # Debian-style tilde versions (e.g. 1.5.0~beta.1) for proper ordering.
    if(LINUX_DISTRO)
        set(CPACK_DEBIAN_FILE_NAME "usb-power-osd_${CPACK_DEBIAN_PACKAGE_VERSION}_${LINUX_DISTRO}_amd64.deb")
        set(CPACK_RPM_FILE_NAME "usb-power-osd-${CPACK_RPM_PACKAGE_VERSION}-${LINUX_DISTRO}.x86_64.rpm")
    else()
        set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
        set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
    endif()
    
endif()

include(CPack)
