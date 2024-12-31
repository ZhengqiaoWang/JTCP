cmake_minimum_required(VERSION 3.11)

project(JTCP)

# 设置编译选项
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

include(cmake/BuildTemplate.cmake)
message(STATUS "Building ${CMAKE_PROJECT_NAME} ${PROJECT_VERSION}")

# 添加源文件
include_directories(${CMAKE_SOURCE_DIR}/include)
file(GLOB_RECURSE ALL_SRCS 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/server/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/*.cpp
)
message(STATUS "ALL_SRCS: ${ALL_SRCS}")

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/JResult)
add_library(${PROJECT_NAME} STATIC ${ALL_SRCS})
target_link_libraries(${PROJECT_NAME} JResult)

# 设置生成的库文件名
set(LIBRARY_OUTPUT_NAME ${PROJECT_NAME}_${PROJECT_VERSION}_${BUILD_TIME}_${GIT_COMMIT})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${LIBRARY_OUTPUT_NAME})
message(STATUS "LIBRARY_OUTPUT_NAME: ${LIBRARY_OUTPUT_NAME}")

# 创建软链接
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_BINARY_DIR}/lib${PROJECT_NAME}.a
)
message(STATUS "Create symlink lib${PROJECT_NAME}.a")

# 导出头文件路径
target_include_directories(JTCP PUBLIC ${PROJECT_SOURCE_DIR}/include)

# Enable code coverage
option(ENABLE_COVERAGE "Use GCOV" OFF)
message(STATUS ENABLE_COVERAGE=${ENABLE_COVERAGE})
if(ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -O0 -g")
endif()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    enable_testing()
    add_subdirectory(ut)

    add_subdirectory(example)
endif()

message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")