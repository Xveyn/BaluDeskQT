# Diese Schicht darf Qt kennen, aber KEINE Widgets.
#
# baludesk_app_lib linkt Qt6::Widgets. Läge AuthBridge dort, könnte jederzeit
# eine QMessageBox in den Arbeiterthread rutschen - Widgets sind ausserhalb des
# GUI-Threads verboten, und der Fehler zeigt sich erst zur Laufzeit, an
# irgendeiner Stelle. Eine Bibliothek, die nur Qt6::Core linkt, macht daraus
# einen Übersetzungsfehler.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

qt_standard_project_setup()

add_library(baludesk_bridge STATIC
    AuthBridge.cpp
)

target_include_directories(baludesk_bridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(baludesk_bridge PUBLIC Qt6::Core baludesk_core)
