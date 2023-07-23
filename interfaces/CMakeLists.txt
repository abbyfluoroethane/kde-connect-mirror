add_definitions(-DTRANSLATION_DOMAIN=\"kdeconnect-interfaces\")

function(geninterface source_h output_h)
    set(xml_file ${CMAKE_CURRENT_BINARY_DIR}/generated/${output_h}.xml)
    qt_generate_dbus_interface( ${source_h} ${xml_file})

    set_source_files_properties(${xml_file} PROPERTIES NO_NAMESPACE true)

    qt_add_dbus_interface(interfaces_SRC ${xml_file} generated/${output_h})
    set(interfaces_SRC ${interfaces_SRC} PARENT_SCOPE)
endfunction()

set(interfaces_SRC
    dbusinterfaces.cpp
    devicesmodel.cpp
    notificationsmodel.cpp
    devicessortproxymodel.cpp
    conversationmessage.cpp
    remotecommandsmodel.cpp
    remotesinksmodel.cpp
    devicespluginfilterproxymodel.cpp
    pluginmodel.cpp
    commandsmodel.cpp
)

# KDE Connect interfaces
geninterface(${PROJECT_SOURCE_DIR}/core/daemon.h daemoninterface)
geninterface(${PROJECT_SOURCE_DIR}/core/device.h deviceinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/battery/batteryplugin.h batteryinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/connectivity-report/connectivity_reportplugin.h connectivityinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/sftp/sftpplugin.h devicesftpinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/notifications/notificationsplugin.h devicenotificationsinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/findmyphone/findmyphoneplugin.h devicefindmyphoneinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/notifications/notification.h notificationinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/mprisremote/mprisremoteplugin.h mprisremoteinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/remotecontrol/remotecontrolplugin.h remotecontrolinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/lockdevice/lockdeviceplugin.h lockdeviceinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/remotecommands/remotecommandsplugin.h remotecommandsinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/remotekeyboard/remotekeyboardplugin.h remotekeyboardinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/sms/smsplugin.h smsinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/sms/conversationsdbusinterface.h conversationsinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/share/shareplugin.h shareinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/remotesystemvolume/remotesystemvolumeplugin.h remotesystemvolumeinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/bigscreen/bigscreenplugin.h bigscreeninterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/virtualmonitor/virtualmonitorplugin.h virtualmonitorinterface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/photo/photoplugin.h photointerface)
geninterface(${PROJECT_SOURCE_DIR}/plugins/clipboard/clipboardplugin.h deviceclipboardinterface)

# System dbus interfaces
if (UNIX AND NOT APPLE)
    # Fixes "redefinition of 'MediaPlayer2' as different kind of symbol"
    set_source_files_properties(
        systeminterfaces/org.mpris.MediaPlayer2.Player.xml
        systeminterfaces/org.mpris.MediaPlayer2.xml
        PROPERTIES NO_NAMESPACE true
    )

    qt_add_dbus_interface(interfaces_SRC systeminterfaces/org.freedesktop.login1.xml generated/systeminterfaces/login1)
    qt_add_dbus_interface(interfaces_SRC systeminterfaces/org.freedesktop.ScreenSaver.xml generated/systeminterfaces/screensaver)
    qt_add_dbus_interface(interfaces_SRC systeminterfaces/org.freedesktop.DBus.Properties.xml generated/systeminterfaces/dbusproperties)
    qt_add_dbus_interface(interfaces_SRC systeminterfaces/org.mpris.MediaPlayer2.Player.xml generated/systeminterfaces/mprisplayer)
    qt_add_dbus_interface(interfaces_SRC systeminterfaces/org.mpris.MediaPlayer2.xml generated/systeminterfaces/mprisroot)
    qt_add_dbus_interface(interfaces_SRC systeminterfaces/org.freedesktop.portal.RemoteDesktop.xml generated/systeminterfaces/remotedesktop)
endif()

add_library(kdeconnectinterfaces STATIC)
set_property(TARGET kdeconnectinterfaces PROPERTY POSITION_INDEPENDENT_CODE ON)

target_sources(kdeconnectinterfaces PRIVATE ${interfaces_SRC})

ecm_qt_declare_logging_category(kdeconnectinterfaces
    HEADER interfaces_conversation_message_debug.h
    IDENTIFIER CONVERSATION_MESSAGE_LOGGING_CATEGORY CATEGORY_NAME kdeconnect.interfaces.conversationmessage
    DEFAULT_SEVERITY Warning
    EXPORT kdeconnect-kde DESCRIPTION "kdeconnect (conversation message)")

ecm_qt_declare_logging_category(kdeconnectinterfaces
    HEADER interfaces_debug.h
    IDENTIFIER KDECONNECT_INTERFACES CATEGORY_NAME kdeconnect.interfaces
    DEFAULT_SEVERITY Warning
    EXPORT kdeconnect-kde DESCRIPTION "kdeconnect (interfaces)")


set_target_properties(kdeconnectinterfaces PROPERTIES
    VERSION ${KDECONNECT_VERSION}
    SOVERSION ${KDECONNECT_VERSION_MAJOR}
)

# pretend the dbus generated headers are from the system so that clazy and other similar
# tools do not show warnings about their contents
target_include_directories(kdeconnectinterfaces SYSTEM PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated)

generate_export_header(kdeconnectinterfaces EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/kdeconnectinterfaces_export.h BASE_NAME KDEConnectInterfaces)

target_link_libraries(kdeconnectinterfaces
LINK_PUBLIC
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::DBus
    KF${QT_MAJOR_VERSION}::ConfigCore
LINK_PRIVATE
    kdeconnectcore
    KF${QT_MAJOR_VERSION}::I18n
    kdeconnectcore
    KF${QT_MAJOR_VERSION}::CoreAddons
)

install(TARGETS kdeconnectinterfaces EXPORT kdeconnectLibraryTargets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)
