find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

add_library(baludesk_app_lib STATIC
    MainWindow.cpp
    SettingsStore.cpp
    QtLogBridge.cpp
    ConnectedPage.cpp
    pairing/Countdown.cpp
    pairing/ServerUrlPage.cpp
    pairing/CertificatePage.cpp
    pairing/PairingCodePage.cpp
    pairing/CertificateChangedPage.cpp
)

target_include_directories(baludesk_app_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(baludesk_app_lib PUBLIC Qt6::Widgets baludesk_core baludesk_bridge)

qt_add_executable(baludesk
    main.cpp
)

target_link_libraries(baludesk PRIVATE baludesk_app_lib)

set_target_properties(baludesk PROPERTIES WIN32_EXECUTABLE ON)

# Qt-Laufzeitbibliotheken neben die ausführbare Datei kopieren, damit sie
# direkt aus dem Build-Verzeichnis startet.
if(WIN32)
    get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
    if(NOT _qt_core_location)
        get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION)
    endif()
    get_filename_component(_qt_bin_dir "${_qt_core_location}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${QT6_INSTALL_PREFIX}/bin" "${_qt_bin_dir}"
        REQUIRED
    )

    add_custom_command(TARGET baludesk POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:baludesk>"
        COMMENT "Qt-Laufzeitbibliotheken neben die .exe kopieren"
        VERBATIM
    )
endif()
