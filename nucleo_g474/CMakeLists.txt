cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED)

project(
  g474_svpwm
  DESCRIPTION "SVPWM 3-phase inverter drive (STM32H743ZI -> STM32G474RE port)"
  VERSION 1
  LANGUAGES C
)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/control.c)
target_sources(app PRIVATE src/spi_cmd.c)

target_include_directories(app PRIVATE include)
