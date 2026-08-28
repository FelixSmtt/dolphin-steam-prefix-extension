include(FetchContent)

FetchContent_Declare(
    source_parsers
    GIT_REPOSITORY https://github.com/tasbox-org/source-parsers.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(source_parsers)


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
    SOURCES
        dolphinoverlayplugin.cpp
        # Compile source-parsers vdfparser directly to avoid shared library visibility issues
        ${source_parsers_SOURCE_DIR}/packages/vdfparser/vdf.cpp
        ${source_parsers_SOURCE_DIR}/packages/vdfparser/keyvalue.cpp
        ${source_parsers_SOURCE_DIR}/packages/source-parsers-shared/internal/case-insensitive-map.cpp
        ${source_parsers_SOURCE_DIR}/packages/source-parsers-shared/internal/peekable-stream.cpp
        ${source_parsers_SOURCE_DIR}/packages/source-parsers-shared/internal/offset-data-view.cpp
    INSTALL_NAMESPACE "kf6/overlayicon"
)

target_include_directories(steamcompatoverlayplugin
    PRIVATE
    ${source_parsers_SOURCE_DIR}/packages
    ${source_parsers_SOURCE_DIR}/packages/vdfparser
    ${source_parsers_SOURCE_DIR}/packages/source-parsers-shared
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
