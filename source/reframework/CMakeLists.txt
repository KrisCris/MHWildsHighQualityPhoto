set(MHWildsHighQualityPhoto_SOURCES
        "InjectClient/FileInjectClient.cpp"
        "InjectClient/FileInjectClient.hpp"
        "InjectClient/ReShadeAddOnInjectClient.cpp"
        "InjectClient/ReShadeAddOnInjectClient.hpp"
        "CImGuiRouteFix.cpp"
        "GameUIController.cpp"
        "GameUIController.hpp"
        "ModSettings.cpp"
        "ModSettings.hpp"
        "REFrameworkBorrowedAPI.cpp"
        "REFrameworkBorrowedAPI.hpp"
        "WebPCaptureInjector.cpp"
        "WebPCaptureInjector.hpp"
        "Plugin.hpp"
		"Plugin.cpp"
	)

add_link_options("/DELAYLOAD:cimgui.dll")
add_library(MHWildsHighQualityPhoto SHARED)

target_sources(MHWildsHighQualityPhoto PRIVATE ${MHWildsHighQualityPhoto_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${MHWildsHighQualityPhoto_SOURCES})

target_compile_features(MHWildsHighQualityPhoto PUBLIC
    cxx_std_23
)

target_link_libraries(MHWildsHighQualityPhoto PUBLIC
    cimgui
    reframework
    libwebp
    glaze::glaze
    nfd
    avir
    thread-pool
)

set_target_properties(MHWildsHighQualityPhoto PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE
        "${CMAKE_BINARY_DIR}/bin/reframework/plugins"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO
        "${CMAKE_BINARY_DIR}/bin/reframework/plugins"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG
        "${CMAKE_BINARY_DIR}/bin/reframework/plugins"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE
        "${CMAKE_BINARY_DIR}/bin/__lib__/"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO
        "${CMAKE_BINARY_DIR}/bin/__lib__/"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG
        "${CMAKE_BINARY_DIR}/bin/__lib__/"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE
        "${CMAKE_BINARY_DIR}/bin/__lib__/"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO
        "${CMAKE_BINARY_DIR}/bin/__lib__/"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG
        "${CMAKE_BINARY_DIR}/bin/__lib__/"
)