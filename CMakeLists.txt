project(transition-table VERSION 0.1.2)
set(PROJECT_FULL_NAME "Transition Table")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

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

add_library(transition-table MODULE
	${transition-table_HEADERS}
	${transition-table_SOURCES})

target_link_libraries(transition-table
	obs-frontend-api
	Qt5::Widgets
	libobs)

set_target_properties(transition-table PROPERTIES FOLDER "plugins/exeldro")
install_obs_plugin_with_data(transition-table data)
