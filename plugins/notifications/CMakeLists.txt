kdeconnect_add_plugin(kdeconnect_notifications SOURCES sendreplydialog.cpp notificationsplugin.cpp notification.cpp otpdetector.cpp)

target_include_directories(kdeconnect_notifications PRIVATE ${CMAKE_BINARY_DIR})

ki18n_wrap_ui(kdeconnect_notifications sendreplydialog.ui)
target_link_libraries(kdeconnect_notifications
    kdeconnectcore
    Qt::DBus
    Qt::Widgets
    KF6::GuiAddons
    KF6::I18n
    KF6::Notifications
    KF6::WindowSystem
)

if (WITH_X11)
    target_link_libraries(kdeconnect_notifications Qt6::GuiPrivate)
endif()

install(FILES "kdeconnect_notifications_config.qml" DESTINATION ${KDE_INSTALL_DATADIR}/kdeconnect)
