cmake_minimum_required(VERSION 3.25)

project(BaluDeskQT
    VERSION 0.1.0
    DESCRIPTION "Desktop-Synchronisationsclient für BaluHost NAS"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
endif()

enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/app)
add_subdirectory(tests)

# Absicherung der Schichtgrenze: baludesk_core darf kein Qt linken
# (Spec Abschnitt 2.4). Steht bewusst hier und nicht in src/core, weil zu
# diesem Zeitpunkt alle Unterverzeichnisse verarbeitet sind - eine
# Verknüpfung aus src/app, tests oder einem späteren src/bridge würde in
# src/core noch gar nicht sichtbar sein.
get_target_property(_core_link_libraries baludesk_core LINK_LIBRARIES)
get_target_property(_core_interface_libraries baludesk_core INTERFACE_LINK_LIBRARIES)
if("${_core_link_libraries};${_core_interface_libraries}" MATCHES "Qt6::")
    message(FATAL_ERROR
        "baludesk_core darf kein Qt linken (Spec Abschnitt 2.4). "
        "LINK_LIBRARIES: ${_core_link_libraries} / "
        "INTERFACE_LINK_LIBRARIES: ${_core_interface_libraries}")
endif()
