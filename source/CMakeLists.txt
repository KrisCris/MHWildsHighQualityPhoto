add_subdirectory(reshade)
add_subdirectory(reframework)

add_custom_target(MHWildsHighQualityPhoto_Mod 
    DEPENDS MHWildsCustomAlbumPhoto MHWildsHQQuestResultPhoto MHWildsHighQualityPhoto_Reshade)

add_custom_command(TARGET MHWildsHighQualityPhoto_Mod
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/HQQuestResult"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/bin/MHWildsHighQualityPhoto_Reshade.addon" "${CMAKE_BINARY_DIR}/bin/HQQuestResult/MHWildsHighQualityPhoto_Reshade.addon"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/assets/QuestResult/modinfo.ini" "${CMAKE_BINARY_DIR}/bin/HQQuestResult/modinfo.ini"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/assets/QuestResult/screenshot.jpg" "${CMAKE_BINARY_DIR}/bin/HQQuestResult/screenshot.jpg"
    
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/CustomAlbumPhoto"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/assets/AlbumPhoto/modinfo.ini" "${CMAKE_BINARY_DIR}/bin/CustomAlbumPhoto/modinfo.ini"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/assets/AlbumPhoto/screenshot.jpg" "${CMAKE_BINARY_DIR}/bin/CustomAlbumPhoto/screenshot.jpg")