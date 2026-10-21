set(kdeconnect_geolocation_SRCS
    geolocationplugin.cpp
    geolocationworker.cpp
)

if(UNIX AND NOT APPLE)
    list(APPEND kdeconnect_geolocation_SRCS
            advertise/nmeaadvertiser_linux.cpp
    )
else()
    list(APPEND kdeconnect_geolocation_SRCS
            advertise/nmeaadvertiser_stub.cpp
    )
endif()

kdeconnect_add_plugin(kdeconnect_geolocation SOURCES ${kdeconnect_geolocation_SRCS})
target_link_libraries(kdeconnect_geolocation
    kdeconnectcore
    Qt::DBus
    Qt::Network
    KF6::I18n
    kdeconnectdbusinterfaces
    ${kdeconnect_geolocation_WL_LINK_LIBS}
)
