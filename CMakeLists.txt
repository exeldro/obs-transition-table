cmake_minimum_required(VERSION 3.18)
project(transition-table VERSION 0.2.2)
set(PROJECT_FULL_NAME "Transition Table")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

find_package(Qt5Widgets)

set(CMAKE_AUTOMOC ON)

set(transition-table_HEADERS
	transition-table.hpp
	version.h)
set(transition-table_SOURCES
	transition-table.cpp)

if(WIN32)
	get_filename_component(ISS_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_FILES_DIR}" ISS_FILES_DIR)
	get_filename_component(ISS_PACKAGE_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PACKAGE_DIR}" ISS_PACKAGE_DIR)
	get_filename_component(ISS_SOURCE_DIR "${PROJECT_SOURCE_DIR}" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_SOURCE_DIR}" ISS_SOURCE_DIR)
	configure_file("installer.iss.in"
			"${PROJECT_BINARY_DIR}/installer.iss"
		)
	configure_file(resource.rc.in transition-table.rc)
	list(APPEND transition-table_SOURCES
		transition-table.rc)
endif()

if(BUILD_OUT_OF_TREE)
    find_package(LibObs REQUIRED)
endif()

add_library(transition-table MODULE
	${transition-table_HEADERS}
	${transition-table_SOURCES})

target_link_libraries(transition-table
		obs-frontend-api
		Qt5::Widgets
		libobs)

if(BUILD_OUT_OF_TREE)
    if(NOT LIB_OUT_DIR)
        set(LIB_OUT_DIR "/lib/obs-plugins")
    endif()
    if(NOT DATA_OUT_DIR)
        set(DATA_OUT_DIR "/share/obs/obs-plugins/transition-table")
    endif()
    set_target_properties(transition-table PROPERTIES PREFIX "")
    install(TARGETS transition-table
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
    install(DIRECTORY data/locale
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
else()
    target_include_directories(transition-table PRIVATE
        "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
    set_target_properties(transition-table PROPERTIES FOLDER "plugins/exeldro")
    install_obs_plugin_with_data(transition-table data)
endif()
