set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(SOURCES
    main.cpp
)

add_executable(TwitchCommentDbViewer ${SOURCES})

# TransCipher headers and lib
target_include_directories(TwitchCommentDbViewer PRIVATE ${CMAKE_SOURCE_DIR}/lib/TransCipher-Dist/include)
target_link_directories(TwitchCommentDbViewer PRIVATE ${CMAKE_SOURCE_DIR}/lib/TransCipher-Dist/lib)

target_link_libraries(TwitchCommentDbViewer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    TransCipher
)

# DLLコピー設定
add_custom_command(TARGET TwitchCommentDbViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/lib/TransCipher-Dist/bin/libTransCipher.dll"
    $<TARGET_FILE_DIR:TwitchCommentDbViewer>
)
