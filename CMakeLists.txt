# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

set(PROJECT_VER "2.5.0-folotoy")
project(xiaozhi)

# uart-uhci 0.3.2 still uses the pre-6.1 GDMA API name. Keep the workaround
# scoped to that managed component until its upstream release catches up.
if((IDF_VERSION_MAJOR GREATER 6) OR
   ((IDF_VERSION_MAJOR EQUAL 6) AND (IDF_VERSION_MINOR GREATER_EQUAL 1)))
    idf_component_get_property(uart_uhci_lib 78__uart-uhci COMPONENT_LIB)
    if(uart_uhci_lib)
        target_compile_options(${uart_uhci_lib} PRIVATE
            "-include${CMAKE_CURRENT_LIST_DIR}/cmake/uart_uhci_idf61_compat.h")
    endif()
endif()
