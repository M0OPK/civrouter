cmake_minimum_required(VERSION 3.30)
#set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
#set(CMAKE_EXE_LINKER_FLAGS "-s")

project(civrouter)

file(GLOB SRC "src/*.cpp")

if (NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif (NOT DEFINED CMAKE_BUILD_TYPE)

add_subdirectory("libs/async_comm")

#add_library(async_comm SHARED
#  src/comm.cpp
#  src/serial.cpp
#)

add_executable(civrouter 
  src/civrouter.cpp
  src/civconfig.cpp
  src/civserial.cpp
  src/civport.cpp
  src/civframe.cpp)

target_include_directories(civrouter PRIVATE "include")
target_include_directories(civrouter PRIVATE "libs/async_comm/include")
target_link_libraries(civrouter PRIVATE async_comm) 

