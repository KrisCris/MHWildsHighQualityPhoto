add_library(MHWildsHighQualityPhoto_Reshade SHARED
    "Plugin.cpp"
)

target_link_libraries(MHWildsHighQualityPhoto_Reshade PRIVATE
    reshade
    sk_hdr_png
    subprocess_h
    stb
)

set_target_properties(MHWildsHighQualityPhoto_Reshade PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE
        "${CMAKE_BINARY_DIR}/bin/"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO
        "${CMAKE_BINARY_DIR}/bin/"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG
        "${CMAKE_BINARY_DIR}/bin/"
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

set_target_properties(MHWildsHighQualityPhoto_Reshade PROPERTIES SUFFIX ".addon")
add_custom_command(
    TARGET MHWildsHighQualityPhoto_Reshade
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/external/hdrfix/hdrfix.exe" "${CMAKE_BINARY_DIR}/bin/")