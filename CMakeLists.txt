# Copyright 2020 Arteris IP
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.13)
if(POLICY CMP0048)
    cmake_policy(SET CMP0048 NEW)
endif()

project (tlm-interfaces LANGUAGES CXX VERSION 1.0.0)

set(SCC_ARCHIVE_DIR_MODIFIER "" CACHE STRING "additional directory levels to store static library archives") 

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(TARGET scc-sysc)
    add_library(${PROJECT_NAME} STATIC
       chi/chi_tlm.cpp
       chi/pe/chi_rn_initiator.cpp
       chi/lwtr/chi_lwtr.cpp
       axi/axi_tlm.cpp
       axi/fsm/base.cpp
       axi/pe/simple_initiator.cpp
       axi/pe/axi_target_pe.cpp
       axi/pe/ace_target_pe.cpp
       axi/pe/ordered_target.cpp
       axi/pe/reordering_target.cpp
       axi/pe/axi_initiator.cpp
       axi/scv/axi_ace_scv.cpp
       axi/lwtr/axi_ace_lwtr.cpp
       axi/checker/axi_protocol.cpp
       axi/checker/ace_protocol.cpp
    )
    target_include_directories (${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> # for headers when building
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> # for client in install mode
    )
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-deprecated)
    target_link_libraries(${PROJECT_NAME} PUBLIC scc-sysc)
    set(TLM-INTERFACES_CMAKE_CONFIG_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/scc)
else()
    add_library(${PROJECT_NAME} INTERFACE) 
    target_include_directories(${PROJECT_NAME} INTERFACE 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> # for headers when building
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> # for client in install mode
    ) 
    target_link_libraries(${PROJECT_NAME} SystemC::systemc)
    set(TLM-INTERFACES_CMAKE_CONFIG_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/tlm-interfaces)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
      VERSION ${PROJECT_VERSION}
      FRAMEWORK FALSE
)

install(TARGETS ${PROJECT_NAME} EXPORT tlm-interfaces-targets
        LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}${SCC_LIBRARY_DIR_MODIFIER}
        ARCHIVE       DESTINATION ${CMAKE_INSTALL_LIBDIR}${SCC_LIBRARY_DIR_MODIFIER}${SCC_ARCHIVE_DIR_MODIFIER}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
    DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}/atp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")
install(
    DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}/cache
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")
install(
    DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}/axi
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")
install(
    DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}/chi
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")


file(GLOB_RECURSE HDR_LIST LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "axi/*.h" "chi/*.h")
set(CMAKE_CONFIGURABLE_FILE_CONTENT "#ifndef _AXI_CHI_H\n#define _AXI_CHI_H")
foreach(filename ${HDR_LIST})
	set(CMAKE_CONFIGURABLE_FILE_CONTENT "${CMAKE_CONFIGURABLE_FILE_CONTENT}\n#include \"${filename}\"")
endforeach()
set(CMAKE_CONFIGURABLE_FILE_CONTENT "${CMAKE_CONFIGURABLE_FILE_CONTENT}\n#endif //_AXI_CHI_H\n")

configure_file("${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in"
  "${CMAKE_CURRENT_BINARY_DIR}/axi_chi.h"
  @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/axi_chi.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

include(CMakePackageConfigHelpers)

install(EXPORT tlm-interfaces-targets
    DESTINATION ${TLM-INTERFACES_CMAKE_CONFIG_DIR})

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/tlm-interfaces-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/tlm-interfaces-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/tlm-interfaces-config.cmake
    INSTALL_DESTINATION ${TLM-INTERFACES_CMAKE_CONFIG_DIR}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/tlm-interfaces-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/tlm-interfaces-config-version.cmake
    DESTINATION ${TLM-INTERFACES_CMAKE_CONFIG_DIR})

