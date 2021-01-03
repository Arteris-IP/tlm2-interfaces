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

cmake_minimum_required(VERSION 3.12)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/../cmake) 
if(POLICY CMP0048)
	cmake_policy(SET CMP0048 NEW)
endif()

project (tlm2-interfaces LANGUAGES CXX VERSION 1.0.0)
option(HAS_PCT "Enable Platform Architect" OFF)

if(HAS_PCT)
	set(SystemCPackage SLSSystemC CACHE INTERNAL "" FORCE)
else()
	set(SystemCPackage OSCISystemC CACHE INTERNAL "" FORCE)
endif()

find_package(${SystemCPackage} REQUIRED)

include_directories(${TLM_INCLUDE_DIRS} ${SystemC_INCLUDE_DIRS})

set(LIB_SOURCES chi/chi_tlm.cpp axi/axi_tlm.cpp)
if(TARGET scc)
	set(LIB_SOURCES ${LIB_SOURCES} 
		axi/fsm/base.cpp
		axi/pe/simple_initiator.cpp
		axi/pe/simple_target.cpp
#		axi/pe/axi_initiator.cpp
#		chi/pe/chi_rn_initiator.cpp
#		clp/clp_tlm.cpp
	)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if(${CMAKE_VERSION} VERSION_LESS "3.13.0") 
	link_directories(${SystemC_LIBRARY_DIRS})
endif()

add_library(tlm2-interfaces ${LIB_SOURCES})
#FILE(GLOB LibHeaders axi/*.h chi/*.h clp/*.h)
set_target_properties(tlm2-interfaces PROPERTIES
	  VERSION ${PROJECT_VERSION}
	  FRAMEWORK FALSE
	  #PUBLIC_HEADER "${LibHeaders}" # specify the public headers
)
target_include_directories (tlm2-interfaces PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (tlm2-interfaces PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories (tlm2-interfaces PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../fmt/include)
target_include_directories (tlm2-interfaces PUBLIC ${SystemC_INCLUDE_DIRS})

if(TARGET scc)
	target_link_libraries(tlm2-interfaces PUBLIC scc)
else()
#	target_link_libraries(tlm2-interfaces SystemC::systemc)
endif()
if(NOT ${CMAKE_VERSION} VERSION_LESS "3.13.0") 
	target_link_directories(tlm2-interfaces PUBLIC ${SystemC_LIBRARY_DIRS})
endif()

add_subdirectory(examples/axi-axi)
install(TARGETS tlm2-interfaces
        RUNTIME       DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE       DESTINATION ${CMAKE_INSTALL_LIBDIR}/static
        #PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tlm
        )
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tlm
    FILES_MATCHING PATTERN "*.h")
        
