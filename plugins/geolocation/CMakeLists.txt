set(kdeconnect_geolocation_SRCS
    geolocationplugin.cpp
    geolocationworker.cpp
)

kdeconnect_add_plugin(kdeconnect_geolocation SOURCES ${kdeconnect_geolocation_SRCS})
target_link_libraries(kdeconnect_geolocation
    kdeconnectcore
    Qt::DBus
    Qt::Network
    KF6::I18n
    kdeconnectdbusinterfaces
    ${kdeconnect_geolocation_WL_LINK_LIBS}
)
