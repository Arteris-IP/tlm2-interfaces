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

if(TARGET scc)
    add_library(${PROJECT_NAME} 
       chi/chi_tlm.cpp
       chi/pe/chi_rn_initiator.cpp
       axi/axi_tlm.cpp
       axi/fsm/base.cpp
       axi/pe/simple_initiator.cpp
       axi/pe/axi_target_pe.cpp
       axi/pe/ordered_target.cpp
       axi/pe/reordering_target.cpp
       axi/pe/axi_initiator.cpp
       axi/checker/axi_protocol.cpp
       axi/checker/ace_protocol.cpp
    )
    target_include_directories (${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${PROJECT_NAME} PUBLIC scc-sysc)
else()
    add_library(${PROJECT_NAME} INTERFACE) 
    target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}) 
    target_link_libraries(${PROJECT_NAME} SystemC::systemc)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
      VERSION ${PROJECT_VERSION}
      FRAMEWORK FALSE
)

install(TARGETS ${PROJECT_NAME}
        LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE       DESTINATION ${CMAKE_INSTALL_LIBDIR}${SCC_ARCHIVE_DIR_MODIFIER}
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

