add_library(core STATIC
    ThumbnailExtractor.cpp
    ThumbnailExtractor.h
)

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(core PUBLIC opennurbsStatic)

add_library(RhinoThumbsCore SHARED
    ThumbnailExtractor.cpp
    ThumbnailExtractor.h
)
target_include_directories(RhinoThumbsCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(RhinoThumbsCore PUBLIC opennurbsStatic)
