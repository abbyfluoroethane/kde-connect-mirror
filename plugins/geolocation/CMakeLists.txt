set(kdeconnect_geolocation_SRCS
     geolocationplugin.cpp
     socketworker.cpp
)
kdeconnect_add_plugin(kdeconnect_geolocation SOURCES  ${kdeconnect_geolocation_SRCS})
target_link_libraries(kdeconnect_geolocation kdeconnectcore
        Qt::DBus
        KF6::I18n
        ${kdeconnect_geolocation_WL_LINK_LIBS}
)

if(NOT DEFINED GEOLOCATION_SOCKET_PATH)
    set(GEOLOCATION_SOCKET_PATH "/run/kdeconnect-geoclue")
endif()

target_compile_definitions(kdeconnect_geolocation PRIVATE
        GEOLOCATION_SOCKET_PATH="${GEOLOCATION_SOCKET_PATH}"
)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

    # Tell geoclue to enable network-nmea source.
    install(
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/40-kdeconnect-geoclue.conf
            DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/geoclue/conf.d"
    )

    # Advertise kdeconnect NMEA socket using avahi.
    install(
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/kdeconnect-nmea.service
            DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/avahi/services"
    )
endif()