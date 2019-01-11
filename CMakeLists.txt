cmake_minimum_required(VERSION 3.8.0 FATAL_ERROR)

project(HighloadCup2018 VERSION 0.1.0 LANGUAGES CXX)
add_executable(HighloadCup2018 draft2.cc)

target_link_libraries(HighloadCup2018 PRIVATE -lfmt)
target_link_libraries(HighloadCup2018 PRIVATE -pg)
target_link_libraries(HighloadCup2018 PRIVATE -g)

set(PEGTL_BUILD_TESTS OFF CACHE BOOL OFF FORCE)
set(PEGTL_BUILD_EXAMPLES OFF CACHE BOOL OFF FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third-party/PEGTL)
target_link_libraries(HighloadCup2018 PRIVATE taocpp::pegtl)

link_directories(PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third-party/icu/source/lib)
target_link_libraries(pegtl INTERFACE libicuuc.a libicudata.a libicui18n.a libicuio.a libicutu.a)
target_link_libraries(pegtl INTERFACE -ldl)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third-party/hopscotch-map)
target_link_libraries(HighloadCup2018 PRIVATE tsl::hopscotch_map)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third-party/array-hash)
target_link_libraries(HighloadCup2018 PRIVATE tsl::array_hash)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third-party/hat-trie)
target_link_libraries(HighloadCup2018 PRIVATE tsl::hat_trie)

target_include_directories(HighloadCup2018 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third-party/utf8_encode)

set_target_properties(${exename} PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)

target_compile_options(HighloadCup2018 PUBLIC -O3)
