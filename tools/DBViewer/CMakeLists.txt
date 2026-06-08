set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(SOURCES
    main.cpp
    ViewerMainWindow.cpp
    ViewerMainWindow.h
    ViewerMainWindow.ui
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
        "$<TARGET_FILE_DIR:TwitchCommentDbViewer>/libTransCipher.dll"
)

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin" "${Qt6_DIR}/../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET TwitchCommentDbViewer POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE} --no-translations --no-compiler-runtime $<TARGET_FILE:TwitchCommentDbViewer>
            COMMENT "Running windeployqt to deploy Qt plugins..."
        )
    endif()
endif()
