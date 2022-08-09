# --- Detect if the plugin is build out of tree or not ---
if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
  cmake_minimum_required(VERSION 3.18)
endif()

project(transition-table VERSION 0.2.5)
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
elseif(OS_MACOS)
	set(MACOS_BUNDLEID "com.exeldro.${PROJECT_NAME}")
	configure_file("CI/macos/installer-macos.pkgproj.in" "${CMAKE_BINARY_DIR}/installer-macos.generated.pkgproj")
endif()

if(BUILD_OUT_OF_TREE)
  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)
  include(cmake/ObsPluginHelpers.cmake)
  set(OBS_FRONTEND_API_NAME "obs-frontend-api")
else()
  set(OBS_FRONTEND_API_NAME "frontend-api")
endif()

add_library(${PROJECT_NAME} MODULE
	${transition-table_HEADERS}
	${transition-table_SOURCES})

# lines if you want add Qt UI in your plugin
find_qt(COMPONENTS Widgets COMPONENTS_LINUX Gui)
set_target_properties(${PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

target_link_libraries(${PROJECT_NAME}
		OBS::${OBS_FRONTEND_API_NAME}
		Qt::Widgets
		libobs)

if(BUILD_OUT_OF_TREE)
    if(NOT LIB_OUT_DIR)
        set(LIB_OUT_DIR "/lib/obs-plugins")
    endif()
    if(NOT DATA_OUT_DIR)
        set(DATA_OUT_DIR "/share/obs/obs-plugins/${PROJECT_NAME}")
    endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
    install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
    install(DIRECTORY data/locale
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
else()
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
    set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "plugins/exeldro")
endif()
setup_plugin_target(${PROJECT_NAME})
