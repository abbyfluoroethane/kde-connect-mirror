
kdeconnect_add_plugin(kdeconnect_sendnotifications)

set(kdeconnect_sendnotifications_SRCS
    sendnotificationsplugin.cpp
    notificationslistener.cpp
    notifyingapplication.cpp
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
target_sources(kdeconnect_sendnotifications PRIVATE ${kdeconnect_sendnotifications_SRCS})
ecm_qt_declare_logging_category(kdeconnect_sendnotifications
    HEADER plugin_sendnotification_debug.h
    IDENTIFIER KDECONNECT_PLUGIN_SENDNOTIFICATION CATEGORY_NAME kdeconnect.plugin.sendnotification
    DEFAULT_SEVERITY Warning
    EXPORT kdeconnect-kde DESCRIPTION "kdeconnect (plugin sendnotification)")
target_link_libraries(kdeconnect_sendnotifications
    kdeconnectcore
    Qt::DBus
    KF${QT_MAJOR_VERSION}::I18n
    Qt::Gui
    KF${QT_MAJOR_VERSION}::IconThemes
    KF${QT_MAJOR_VERSION}::ConfigCore
)
if(WIN32)
   target_link_libraries(kdeconnect_sendnotifications windowsapp)
   target_compile_features(kdeconnect_sendnotifications PUBLIC cxx_std_17)
else()
   target_link_libraries(kdeconnect_sendnotifications PkgConfig::GIO)
endif()

# Config
kcoreaddons_add_plugin(kdeconnect_sendnotifications_config INSTALL_NAMESPACE "kdeconnect/kcms")

ki18n_wrap_ui(kdeconnect_sendnotifications_config sendnotifications_config.ui)
target_sources(kdeconnect_sendnotifications_config PRIVATE
     sendnotifications_config.cpp
     notifyingapplication.cpp
     notifyingapplicationmodel.cpp
)

target_link_libraries(kdeconnect_sendnotifications_config
    kdeconnectcore
    kdeconnectpluginkcm
    KF${QT_MAJOR_VERSION}::I18n
    KF${QT_MAJOR_VERSION}::KCMUtils
)
