# WICHTIG: Dieses Ziel darf niemals Qt linken.
# Begründung in docs/superpowers/specs/2026-07-26-baludesk-qt-design.md, Abschnitt 2.4.
# Kein find_package(Qt6) und kein Qt6::-Ziel in dieser Datei.

add_library(baludesk_core STATIC
    util/AppConfig.cpp
)

target_include_directories(baludesk_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Schutz gegen versehentliches Verlinken von Qt. Bricht die Konfiguration ab,
# sobald jemand ein Qt6::-Ziel an baludesk_core hängt.
# Grenze: die Prüfung läuft an dieser Stelle im Konfigurationsdurchlauf, also
# bevor spätere Unterverzeichnisse (z. B. src/app) verarbeitet werden. Ein
# target_link_libraries(baludesk_core ... Qt6::...) aus einem später
# eingebundenen Unterverzeichnis würde hier nicht mehr erfasst.
get_target_property(_core_link_libraries baludesk_core LINK_LIBRARIES)
if(_core_link_libraries MATCHES "Qt6::")
    message(FATAL_ERROR
        "baludesk_core darf kein Qt linken (Spec Abschnitt 2.4). "
        "Gefunden: ${_core_link_libraries}")
endif()
