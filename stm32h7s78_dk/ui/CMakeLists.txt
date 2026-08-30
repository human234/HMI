file(GLOB UI_SOURCE src/*.c)
add_library(ui STATIC ${UI_SOURCE})
target_include_directories(ui PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(ui PUBLIC zephyr_interface)
