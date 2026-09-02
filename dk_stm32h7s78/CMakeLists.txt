cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED)

project(
  stm32_h7s78
  DESCRIPTION "a simple stm32h7s78_dk project ported from stm32_h743zi"
  VERSION 1
  LANGUAGES C
)

file(GLOB APP_SOURCES src/*.c)

target_sources(app PRIVATE ${APP_SOURCES})

add_subdirectory(ui)

target_link_libraries(app PRIVATE ui)

target_include_directories(app PRIVATE include)
