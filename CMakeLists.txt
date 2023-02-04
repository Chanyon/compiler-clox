cmake_minimum_required(VERSION 3.15)

#set the project name
project(clox VERSION 0.0.1)

configure_file(CloxConfig.h.in CloxConfig.h)
#set the C++ standard 支持特定 C++ 标准的最简单方法是使用
#set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_CXX_STANDARD_REQUIRED True)

#提供用户可以选择的选项(经典方法)
option(USE_MYMATH "use des/ provided math implementation" ON)

#提供用户可以选择的选项
#if(USE_MYMATH)
#  add_subdirectory(deps)
#  list(APPEND EXTRA_LIBS deps)
#endif()

set(SRC_LIST main.c)
set(SRC_LIST2 chunk.c memory.c debug.c value.c vm.c compiler.c scanner.c object.c)
add_executable(${PROJECT_NAME} ${SRC_LIST} ${SRC_LIST2})

#deps.h
#target_link_libraries(${PROJECT_NAME} PUBLIC ${EXTRA_LIBS})

target_include_directories(${PROJECT_NAME} PUBLIC
                        ${PROJECT_BINARY_DIR}
                        )
