cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED)

project(
  combined_h7s78
  DESCRIPTION "STM32H7S78-DK touchscreen SVPWM controller (LVGL UI + SPI4 slave)"
  VERSION 1
  LANGUAGES C
)

file(GLOB APP_SOURCES src/*.c)

target_sources(app PRIVATE ${APP_SOURCES})
target_sources(app PRIVATE src/slave_ctl.c)

target_include_directories(app PRIVATE include)

add_subdirectory(ui)

target_link_libraries(app PRIVATE ui)

target_include_directories(app PRIVATE include)
