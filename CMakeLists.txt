cmake_minimum_required(VERSION 3.20)
project(ColorfulYomi VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/utf-8 /W3)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  add_compile_options(-Wall -Wextra)
endif()

set(CY_SOURCES src/cy_util.c src/cy_dic.c src/cy_analyze.c src/cy_number.c src/cy_koe.c src/cy_roman.c src/cy_api.c)

add_library(colorful_yomi STATIC ${CY_SOURCES})
target_include_directories(colorful_yomi PUBLIC include PRIVATE src)
set_target_properties(colorful_yomi PROPERTIES OUTPUT_NAME colorful_yomi_static)

add_library(colorful_yomi_dll SHARED ${CY_SOURCES})
target_include_directories(colorful_yomi_dll PUBLIC include PRIVATE src)
target_compile_definitions(colorful_yomi_dll PRIVATE CT_BUILD_DLL)
set_target_properties(colorful_yomi_dll PROPERTIES OUTPUT_NAME colorful_yomi C_VISIBILITY_PRESET hidden)

add_executable(cyomi tools/cyomi.c)
target_link_libraries(cyomi PRIVATE colorful_yomi)
if(WIN32)
  target_link_libraries(cyomi PRIVATE shell32)
endif()

enable_testing()
add_executable(cy_test tests/cy_test.c)
target_link_libraries(cy_test PRIVATE colorful_yomi)
target_include_directories(cy_test PRIVATE src)
add_test(NAME cy_test COMMAND cy_test ${CMAKE_SOURCE_DIR}/tests)
