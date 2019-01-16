cmake_minimum_required(VERSION 3.13.2 FATAL_ERROR)

project(HighloadCup2018 VERSION 0.1.0 LANGUAGES CXX)
add_executable(HighloadCup2018 draft2.cc)

set_target_properties(HighloadCup2018 PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)

target_compile_options(HighloadCup2018 PUBLIC -O3 -pg -g)

add_subdirectory(${CMAKE_SOURCE_DIR}/third-party/fmt)
target_link_libraries(HighloadCup2018 PRIVATE fmt::fmt-header-only)

set(PEGTL_BUILD_TESTS OFF CACHE BOOL OFF FORCE)
set(PEGTL_BUILD_EXAMPLES OFF CACHE BOOL OFF FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/third-party/PEGTL)
target_link_libraries(HighloadCup2018 PRIVATE taocpp::pegtl)

target_link_directories(pegtl INTERFACE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third-party/icu/source/lib>)
target_link_libraries(pegtl INTERFACE icuuc icudata icui18n icuio icutu)
target_link_libraries(pegtl INTERFACE -ldl)

add_subdirectory(${CMAKE_SOURCE_DIR}/third-party/hopscotch-map)
target_link_libraries(HighloadCup2018 PRIVATE tsl::hopscotch_map)

add_subdirectory(${CMAKE_SOURCE_DIR}/third-party/array-hash)
target_link_libraries(HighloadCup2018 PRIVATE tsl::array_hash)

add_subdirectory(${CMAKE_SOURCE_DIR}/third-party/hat-trie)
target_link_libraries(HighloadCup2018 PRIVATE tsl::hat_trie)

target_include_directories(HighloadCup2018 PRIVATE ${CMAKE_SOURCE_DIR}/third-party/utf8_encode)

target_include_directories(HighloadCup2018 PRIVATE ${CMAKE_SOURCE_DIR}/third-party/asio/asio/include)
target_include_directories(HighloadCup2018 PRIVATE ${CMAKE_SOURCE_DIR}/third-party/config/include)
target_link_libraries(HighloadCup2018 PRIVATE -lpthread)
