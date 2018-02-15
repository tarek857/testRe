project(api)
cmake_minimum_required(VERSION 3.4.3)



find_path(COMMLAYER_INC_DIR CommlayerAPI.h)

find_library(COMMLAYER_LIBRARY cl)

add_executable (api main.cpp)
target_link_libraries(api ${COMMLAYER_LIBRARY} pthread)
target_include_directories(api PRIVATE ${COMMLAYER_INC_DIR})

install(TARGETS api RUNTIME DESTINATION bin)
