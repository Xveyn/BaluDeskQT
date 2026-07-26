find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# Sieht neben dem find_package(Qt6 ...) in src/app/CMakeLists.txt redundant
# aus, ist es aber nicht: importierte Ziele sind ohne GLOBAL
# verzeichnisgebunden, und tests/ ist ein Geschwisterverzeichnis von
# src/app, kein Nachfahre — Qt6::Core ist hier sonst schlicht unsichtbar.
# Ohne diese Zeile scheitert das get_target_property() weiter unten.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

# Schaltet AUTOMOC für dieses Verzeichnis ein, bevor irgendein Test es
# braucht. qt_standard_project_setup() wird in src/app/CMakeLists.txt
# aufgerufen und gilt nur dort; tests/ ist wie oben ein Geschwister- statt
# ein Nachfahrenverzeichnis.
#
# ACHTUNG, die Reihenfolge ist tragend: CMake setzt die AUTOMOC-Eigenschaft
# eines Ziels aus CMAKE_AUTOMOC in dem Moment, in dem das Ziel entsteht.
# Steht dieser Aufruf hinter add_executable(), bleibt er wirkungslos — die
# Ziele sind dann schon ohne AUTOMOC angelegt. Er gehört deshalb vor jedes
# add_executable() dieser Datei und darf nicht nach unten wandern.
#
# Noch deklariert kein Test hier ein Q_OBJECT, aber der erste, der eine
# Signal-Spy oder ein Fake-QObject braucht, würde sonst nicht verständlich
# fehlschlagen, sondern mit einem unaufgelösten vtable-Symbol beim Linken.
qt_standard_project_setup()

# --- Kern-Tests: linken bewusst kein Qt ---------------------------------
add_executable(baludesk_core_tests
    core/app_config_test.cpp
    core/logger_test.cpp
)

target_link_libraries(baludesk_core_tests PRIVATE
    baludesk_core
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(baludesk_core_tests)

# --- App-Tests: brauchen Qt --------------------------------------------
add_executable(baludesk_app_tests
    app/settings_store_test.cpp
    app/qt_log_bridge_test.cpp
)

target_link_libraries(baludesk_app_tests PRIVATE
    baludesk_app_lib
    GTest::gtest
    GTest::gtest_main
)

get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
if(NOT _qt_core_location)
    get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION)
endif()
get_filename_component(_qt_bin_dir "${_qt_core_location}" DIRECTORY)

# ENVIRONMENT_MODIFICATION statt ENVIRONMENT: der PATH enthält Semikola, und
# die würden in einer ENVIRONMENT-Liste als Trennzeichen zwischen Einträgen
# gelesen. path_list_prepend hängt den Qt-Pfad sauber vorne an, ohne den
# vorhandenen PATH anzufassen.
gtest_discover_tests(baludesk_app_tests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin_dir}"
)

# ENVIRONMENT_MODIFICATION wirkt nur auf die von gtest_discover_tests einzeln
# angelegten Tests, nicht auf den Discovery-Lauf selbst (CMakes
# GoogleTestAddTests.cmake ruft dafür execute_process() ohne jede
# Umgebungsanpassung auf). Ohne die Qt-DLLs direkt neben der .exe würde die
# Testerkennung auf einem frischen build/-Verzeichnis mit 0xc0000135 (DLL
# nicht gefunden) scheitern, bevor überhaupt ein Test läuft. windeployqt löst
# das, indem es die Laufzeitbibliotheken neben baludesk_app_tests.exe legt -
# genau wie für baludesk.exe in src/app/CMakeLists.txt.
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE_TESTS windeployqt
        HINTS "${QT6_INSTALL_PREFIX}/bin" "${_qt_bin_dir}"
        REQUIRED
    )

    add_custom_command(TARGET baludesk_app_tests POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE_TESTS}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:baludesk_app_tests>"
        COMMENT "Qt-Laufzeitbibliotheken neben die Testausführung kopieren"
        VERBATIM
    )
endif()
