# WICHTIG: Dieses Ziel darf niemals Qt linken.
# Begründung in docs/superpowers/specs/2026-07-26-baludesk-qt-design.md, Abschnitt 2.4.
# Kein find_package(Qt6) und kein Qt6::-Ziel in dieser Datei.
#
# Das eigentliche, tragende Mittel dafür ist, dass baludesk_core schlicht
# keine Qt-Include-Verzeichnisse bekommt: ein #include <QString> unter
# core/ scheitert damit einfach an der Kompilierung. Die Prüfung im
# Wurzel-CMakeLists.txt (nach add_subdirectory(tests)) ist nur das zweite
# Netz für den Fall, dass jemand versehentlich ein Qt6::-Ziel anlinkt, statt
# einen Qt-Header einzubinden.

find_package(spdlog CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

add_library(baludesk_core STATIC
    util/AppConfig.cpp
    util/Logger.cpp
    util/TlsTrust.cpp
    util/CredentialStore.cpp
    api/HttpClient.cpp
    api/CurlTransport.cpp
)

target_include_directories(baludesk_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(baludesk_core PUBLIC
    spdlog::spdlog
    OpenSSL::Crypto
    CURL::libcurl
)

if(WIN32)
    target_link_libraries(baludesk_core PRIVATE Advapi32)
endif()
