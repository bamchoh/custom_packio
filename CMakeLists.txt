cmake_minimum_required(VERSION 3.10)
project(asiotest CXX)
add_executable(asiotest main.cpp)

add_compile_definitions(PACKIO_STANDALONE_ASIO=1)
add_compile_definitions(PACKIO_HAS_MSGPACK=0)
add_compile_definitions(PACKIO_HAS_NLOHMANN_JSON=1)
add_compile_definitions(PACKIO_HAS_BOOST_JSON=0)
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
# add_compile_definitions(PACKIO_LOGGING)

include_directories(
    ${CMAKE_SOURCE_DIR}/.
    ${CMAKE_SOURCE_DIR}/../include/packio-2.6.0/include
    ${CMAKE_SOURCE_DIR}/../include/asio-1.30.2/include
    ${CMAKE_SOURCE_DIR}/../include/json-3.9.1/include
    ${CMAKE_SOURCE_DIR}/../include/spdlog/include
)

target_link_libraries(asiotest
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libllhttp.a
)
# target_link_libraries~