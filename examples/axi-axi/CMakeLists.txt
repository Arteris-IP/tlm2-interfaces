cmake_minimum_required(VERSION 2.8)

set(APP_NAME axi_axi_test)
option(HAS_PCT "Enable Platform Architect" OFF)

if(HAS_PCT)
	set(SystemCPackage SLSSystemC CACHE INTERNAL "" FORCE)
else()
	set(SystemCPackage OSCISystemC CACHE INTERNAL "" FORCE)
endif()

find_package(${SystemCPackage} REQUIRED)

include_directories(${TLM_INCLUDE_DIRS} ${SystemC_INCLUDE_DIRS})

project (${APP_NAME})
link_directories(${SystemC_LIBRARY_DIRS})

add_executable(${APP_NAME} sc_main.cpp)

target_include_directories (${APP_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../axi)
target_include_directories (${APP_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries (${APP_NAME} PUBLIC tlm2-interfaces)
target_link_libraries (${APP_NAME} LINK_PUBLIC ${SystemC_LIBRARIES} )
target_link_libraries (${APP_NAME} LINK_PUBLIC ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries (${APP_NAME} LINK_PUBLIC ${CMAKE_DL_LIBS})
