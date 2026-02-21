add_executable(viewer WIN32
    main.cpp
    viewer.manifest
)

target_compile_definitions(viewer PRIVATE UNICODE _UNICODE)

target_link_libraries(viewer
    PRIVATE
    core
    comctl32
    gdiplus
    shlwapi
    opennurbsStatic
)
