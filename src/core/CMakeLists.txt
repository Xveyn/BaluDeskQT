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
    util/WindowsCredentialStore.cpp
    api/HttpClient.cpp
    api/CurlTransport.cpp
    api/ServerUrl.cpp
)

target_include_directories(baludesk_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# spdlog bleibt PUBLIC: Logger.h ist ein öffentlicher Header und bindet
# spdlog/spdlog.h ein, das braucht also jeder, der baludesk_core linkt.
# OpenSSL und curl dagegen tauchen in keinem öffentlichen Header auf - nur in
# .cpp-Dateien -, deshalb PRIVATE: CMakes eigene Sicht auf die
# Include-Verzeichnisse (INTERFACE_INCLUDE_DIRECTORIES) bleibt damit korrekt
# auf diese Bibliothek beschränkt, und ein Linker- oder Build-System-Tausch
# in der Zukunft würde die Trennung respektieren. Bei einer statischen
# Bibliothek propagiert CMake die Linkabhängigkeit trotzdem über
# $<LINK_ONLY:> - nur die Include-Pfade hören CMake-seitig auf zu sickern.
#
# ACHTUNG, das schließt die Lücke nicht vollständig: vcpkg installiert alle
# Pakete in EIN gemeinsames <triplet>/include/-Verzeichnis (curl/, openssl/
# und spdlog/ liegen dort nebeneinander). Weil spdlog::spdlog PUBLIC bleibt,
# gelangt genau dieses gemeinsame Verzeichnis über spdlog trotzdem in
# baludesk_app_lib - ein #include <curl/curl.h> unter src/app fände die
# Header also physisch weiterhin. Geprüft per Testkompilat (siehe
# Abschlussbericht); eine vollständige physische Trennung bräuchte ein
# eigenes vcpkg-Overlay oder eine andere Paketquelle je Bibliothek - das
# sprengt diesen Aufräum-Durchgang.
target_link_libraries(baludesk_core
    PUBLIC
        spdlog::spdlog
    PRIVATE
        OpenSSL::Crypto
        CURL::libcurl
)

if(WIN32)
    target_link_libraries(baludesk_core PRIVATE Advapi32)
endif()
