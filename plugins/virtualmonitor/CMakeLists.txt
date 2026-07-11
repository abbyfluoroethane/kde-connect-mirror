kdeconnect_add_plugin(kdeconnect_virtualmonitor SOURCES virtualmonitorplugin.cpp)
target_link_libraries(kdeconnect_virtualmonitor
    kdeconnectcore
    Qt::Core
    Qt::DBus
    Qt::Network
    KF6::I18n
    KF6::Notifications
)

install(FILES "kdeconnect_virtualmonitor_config.qml" DESTINATION ${KDE_INSTALL_DATADIR}/kdeconnect)
