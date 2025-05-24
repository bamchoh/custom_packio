cmake_minimum_required(VERSION 3.11)

project(custom_packio CXX)
add_executable(custom_packio main.cpp)

add_compile_definitions(PACKIO_STANDALONE_ASIO=1)
add_compile_definitions(PACKIO_HAS_MSGPACK=0)
add_compile_definitions(PACKIO_HAS_NLOHMANN_JSON=1)
add_compile_definitions(PACKIO_HAS_BOOST_JSON=0)
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
# add_compile_definitions(PACKIO_LOGGING)

include_directories(
    ${CMAKE_SOURCE_DIR}/.
    ${CMAKE_SOURCE_DIR}/packio/include
    ${CMAKE_SOURCE_DIR}/asio/asio/include
    ${CMAKE_SOURCE_DIR}/json/include
    ${CMAKE_SOURCE_DIR}/spdlog/include
    ${CMAKE_SOURCE_DIR}/llhttp/build
)

set(LLHTTP_BUILD_COMMAND CFLAGS=${CMAKE_CLANG_CFLAGS} ${CMAKE_SOURCE_DIR}/llhttp_build.sh)
if(WIN32)
  set(LLHTTP_BUILD_COMMAND set CFLAGS=${CMAKE_CLANG_CFLAGS} && ${CMAKE_SOURCE_DIR}/llhttp_build.bat)
endif()

# llhttp_build.sh を実行するカスタムターゲットを作成
add_custom_target(build_llhttp
    COMMAND ${LLHTTP_BUILD_COMMAND}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/libllhttp.a
    COMMENT "Building llhttp"
)

# custom_packio は build_llhttp に依存
add_dependencies(custom_packio build_llhttp)

if (MINGW)
  target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32) # mswsock wsock32
  target_link_libraries(${PROJECT_NAME} PRIVATE wsock32) # mswsock wsock32
endif()

target_link_libraries(custom_packio
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/llhttp/build/libllhttp.a
)
# target_link_libraries~