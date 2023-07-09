if (SAILFISHOS)
	set(SN_PLUGIN_ENABLED_BY_DEFAULT "true")
else()
	set(SN_PLUGIN_ENABLED_BY_DEFAULT "false")
endif()

configure_file(kdeconnect_sendnotifications.json.in kdeconnect_sendnotifications.json)

set(debug_file_SRCS)
ecm_qt_declare_logging_category(
    debug_file_SRCS HEADER plugin_sendnotification_debug.h
    IDENTIFIER KDECONNECT_PLUGIN_SENDNOTIFICATION CATEGORY_NAME kdeconnect.plugin.sendnotification
    DEFAULT_SEVERITY Warning
    EXPORT kdeconnect-kde DESCRIPTION "kdeconnect (plugin sendnotification)")

set(kdeconnect_sendnotifications_SRCS
    sendnotificationsplugin.cpp
    notificationslistener.cpp
    notifyingapplication.cpp
    kdeconnect_sendnotifications.json
    ${debug_file_SRCS}
)
if(WIN32)
set(kdeconnect_sendnotifications_SRCS
    ${kdeconnect_sendnotifications_SRCS}
    windowsnotificationslistener.cpp
)
else()
set(kdeconnect_sendnotifications_SRCS
    ${kdeconnect_sendnotifications_SRCS}
    dbusnotificationslistener.cpp
)
endif()

kdeconnect_add_plugin(kdeconnect_sendnotifications SOURCES ${kdeconnect_sendnotifications_SRCS})

target_link_libraries(kdeconnect_sendnotifications
    kdeconnectcore
    Qt::DBus
    KF5::I18n
    Qt::Gui
    KF5::IconThemes
    KF5::ConfigCore
)
if(WIN32)
   target_link_libraries(kdeconnect_sendnotifications windowsapp)
   target_compile_features(kdeconnect_sendnotifications PUBLIC cxx_std_17)
else()
   target_link_libraries(kdeconnect_sendnotifications PkgConfig::GIO)
endif()

#######################################
# Config
if (NOT WIN32 AND NOT SAILFISHOS)

kcoreaddons_add_plugin(kdeconnect_sendnotifications_config INSTALL_NAMESPACE "kdeconnect/kcms")

ki18n_wrap_ui(kdeconnect_sendnotifications_config sendnotifications_config.ui)
target_sources(kdeconnect_sendnotifications_config PRIVATE
     sendnotifications_config.cpp
     notifyingapplication.cpp
     notifyingapplicationmodel.cpp
)

target_link_libraries( kdeconnect_sendnotifications_config
    kdeconnectcore
    kdeconnectpluginkcm
    KF5::I18n
    KF5::KCMUtils
)
endif()
