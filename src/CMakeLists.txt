set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(SOURCES
    main.cpp
    MainWindow.cpp
    MainWindow.h
    MainWindow.ui
)

add_executable(TwitchCommentManager ${SOURCES})

# TrustChain setup (as per documentation)
trustchain_setup_target(TwitchCommentManager)

# TransCipher headers and lib
target_include_directories(TwitchCommentManager PRIVATE ${CMAKE_SOURCE_DIR}/lib/TransCipher-Dist/include)
target_link_directories(TwitchCommentManager PRIVATE ${CMAKE_SOURCE_DIR}/lib/TransCipher-Dist/lib)

target_link_libraries(TwitchCommentManager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    trustchain
    TransCipher
)

# ウォーターマークの暗号文と復号キーをアプリ本体にマクロとして注入
target_compile_definitions(TwitchCommentManager PRIVATE
    WATERMARK_ENCRYPTED_TEXT="${TRUSTCHAIN_DEFAULT_CREATOR}"
    WATERMARK_DECRYPT_KEY="${TRUSTCHAIN_WATERMARK_KEY}"
)

# DLLコピー設定
add_custom_command(TARGET TwitchCommentManager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/lib/TransCipher-Dist/bin/libTransCipher.dll"
    $<TARGET_FILE_DIR:TwitchCommentManager>
)
