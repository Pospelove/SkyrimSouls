cmake_minimum_required(VERSION 3.15)
project(SkyrimSouls LANGUAGES CXX)

file(GLOB_RECURSE SkyrimSoulsSrc "${CMAKE_CURRENT_LIST_DIR}/SkyrimSouls/*")
file(GLOB_RECURSE SkyrimSrc "${CMAKE_CURRENT_LIST_DIR}/Skyrim/*")
file(GLOB_RECURSE SKSE2Src "${CMAKE_CURRENT_LIST_DIR}/SKSE/*")

add_library(Skyrim STATIC ${SkyrimSrc})
add_library(SKSE2 STATIC ${SKSE2Src})
add_library(SkyrimSouls SHARED ${SkyrimSoulsSrc})

target_link_libraries(SkyrimSouls PRIVATE SKSE2 Skyrim)

foreach(target SkyrimSouls SKSE2 Skyrim)
  target_include_directories(${target}  PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Skyrim/include")
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/SKSE/include")

  set_property(TARGET ${target} APPEND_STRING PROPERTY COMPILE_FLAGS " /FI\"SKSE/DebugLog.h\" ")
  set_property(TARGET ${target} APPEND_STRING PROPERTY COMPILE_FLAGS " /FI\"Skyrim.h\" ")
endforeach()
