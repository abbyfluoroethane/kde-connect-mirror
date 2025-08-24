kdeconnect_add_plugin(kdeconnect_mobilevolume
    SOURCES
        mobilevolumeplugin.cpp
)

target_link_libraries(kdeconnect_mobilevolume
    kdeconnectcore
    Qt::DBus
    KF6::I18n
)

# Ensure QML files are included in resources
# kdeconnect_add_qml_resources(mobilevolume
#    ${CMAKE_CURRENT_SOURCE_DIR}/qml/VolumeDialog.qml
# )