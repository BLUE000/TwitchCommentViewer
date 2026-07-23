include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/release-1.12.1.zip
)
# Windowsでビルドエラーを防ぐための設定
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

set(CMAKE_AUTOMOC ON)

add_executable(UnitTests 
    test_main.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/DatabaseManager.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/ConfigManager.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/TwitchEventCollectorImpl.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/TwitchEventCollectorImpl.h
    ${CMAKE_SOURCE_DIR}/src/modules/VoiceVoxIntegrationImpl.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/VoiceVoxIntegrationImpl.h
    ${CMAKE_SOURCE_DIR}/src/modules/ObsHttpServer.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/ObsHttpServer.h
)

target_include_directories(UnitTests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/interfaces
    ${CMAKE_SOURCE_DIR}/src/modules
    ${CMAKE_SOURCE_DIR}/lib/TransCipher-Dist/include
)
target_link_directories(UnitTests PRIVATE ${CMAKE_SOURCE_DIR}/lib/TransCipher-Dist/lib)

target_link_libraries(UnitTests PRIVATE
    gtest
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Sql
    Qt6::WebSockets
    TransCipher
)

if(WIN32)
    target_link_libraries(UnitTests PRIVATE crypt32 winmm)
endif()

# テスト実行フォルダにもDLLをコピー
add_custom_command(TARGET UnitTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/lib/TransCipher-Dist/bin/libTransCipher.dll"
    $<TARGET_FILE_DIR:UnitTests>
)

include(GoogleTest)

# QtのDLLとプラグイン(qsqlite.dll等)をテストフォルダにもコピー
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin" "${Qt6_DIR}/../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET UnitTests POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE} --no-translations --compiler-runtime "$<TARGET_FILE:UnitTests>"
        )
    endif()
endif()

gtest_discover_tests(UnitTests)
