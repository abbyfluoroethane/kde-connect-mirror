set(kdeconnect_NetworkStatusPlugin_SRCS
    NetworkStatusPlugin.cpp
)

kdeconnect_add_plugin(kdeconnect_NetworkStatusPlugin JSON kdeconnect_NetworkStatusPlugin.json SOURCES ${kdeconnect_NetworkStatusPlugin_SRCS})

target_link_libraries(kdeconnect_NetworkStatusPlugin
    kdeconnectcore
    Qt5::DBus
    KF5::I18n
)
