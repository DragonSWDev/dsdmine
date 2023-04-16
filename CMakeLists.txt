cmake_minimum_required(VERSION 3.7)
project(dsdmine VERSION "1.0")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

if(WIN32)
	set(SDL2_PATH ${SDL2_PATH} "${CMAKE_SOURCE_DIR}/sdl2/SDL2")
endif()

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

add_executable(dsdmine WIN32 MACOSX_BUNDLE
	src/imgui.cpp 
	src/imgui_draw.cpp 
	src/imgui_tables.cpp 
	src/imgui_widgets.cpp 
	src/imgui_impl_sdl.cpp 
	src/imgui_impl_sdlrenderer.cpp 
	src/dsdmine.cpp)

target_include_directories(dsdmine PRIVATE "${CMAKE_SOURCE_DIR}/include/")
target_link_libraries(dsdmine ${SDL2_LIBRARY})

if(APPLE)
	file(COPY "${CMAKE_SOURCE_DIR}/assets" DESTINATION "${CMAKE_BINARY_DIR}/dsdmine.app/Contents/Resources")

	set(MACOSX_BUNDLE_BUNDLE_NAME dsdmine)
	set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.DragonSWDev.dsdmine")
	set(MACOSX_BUNDLE_LONG_VERSION_STRING ${CMAKE_PROJECT_VERSION})
	set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${CMAKE_PROJECT_VERSION})
	set(MACOSX_BUNDLE_COPYRIGHT "Copyright 2023, DragonSWDev")
endif()
