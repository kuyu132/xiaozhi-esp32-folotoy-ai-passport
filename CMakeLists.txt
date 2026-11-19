# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

set(PROJECT_VER "2.6.0")
project(xiaozhi)

# Some IDF 6.1 development builds changed the GDMA API; the final release
# retains the original signature. Inject the shim only when the new API exists.
set(gdma_header "$ENV{IDF_PATH}/components/esp_driver_dma/include/esp_private/gdma.h")
set(gdma_struct_api "")
if(EXISTS "${gdma_header}")
    file(STRINGS "${gdma_header}" gdma_struct_api
         REGEX "^[^/]*gdma_get_channel_alignment_constraints\\(")
endif()
if(gdma_struct_api)
    idf_component_get_property(uart_uhci_lib 78__uart-uhci COMPONENT_LIB)
    if(uart_uhci_lib)
        target_compile_options(${uart_uhci_lib} PRIVATE
            "-include${CMAKE_CURRENT_LIST_DIR}/cmake/uart_uhci_idf61_compat.h")
    endif()
endif()
