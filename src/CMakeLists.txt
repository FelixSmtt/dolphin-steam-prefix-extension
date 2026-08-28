#--- ACTION PLUGIN ---
kcoreaddons_add_plugin(steamcompatpluginaction
    SOURCES dolphinactionplugin.cpp
    INSTALL_NAMESPACE "kf6/kfileitemaction"
)

target_link_libraries(steamcompatpluginaction
    PRIVATE
    KF6::CoreAddons
    KF6::KIOCore
    KF6::KIOWidgets
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

#---OVERLAY PLUGIN---
kcoreaddons_add_plugin(steamcompatoverlayplugin
    SOURCES dolphinoverlayplugin.cpp
    INSTALL_NAMESPACE "kf6/overlayicon"
)

target_link_libraries(steamcompatoverlayplugin
    PRIVATE
    KF6::CoreAddons
    KF6::KIOCore
    KF6::KIOWidgets
    Qt6::Core
    Qt6::Gui
)

#---Resources---
install(FILES icons/steam-non-steam.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
