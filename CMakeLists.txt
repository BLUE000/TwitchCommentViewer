cmake_minimum_required(VERSION 3.16)
project(TwitchCommentManager VERSION 1.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql WebSockets Charts)

# Submodules
add_subdirectory(lib/TrustChain)

# Add source and tool directories
add_subdirectory(src)
add_subdirectory(tools/DBViewer)
add_subdirectory(tests)
