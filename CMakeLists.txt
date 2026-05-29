set(OD_MODULE_DEPS uiODMain uiAttributes)
set(OD_FOLDER "OD Free Plugins")
set(OD_IS_PLUGIN YES)
set(OD_MODULE_GUI YES)
set(OD_MODULE_SOURCES
    uiseismicsimulator.cc
    uiseismic_pi.cc
    uimamutemainwindow.cc
    uimamutemainwindow_velocity.cc
    uimamutemainwindow_geometry.cc
    mamuteworkflow.cc
    uiseismogramviewer.cc
)

set( OD_PLUGIN_ALO_EXEC ${OD_MAIN_EXEC} )
if ( "${OD_SUBSYSTEM}" STREQUAL "od" )
    set( OD_NO_ALO_ENTRY TRUE ) # OpendTect SDK internal build
endif()

OD_INIT_MODULE()
