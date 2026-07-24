cmake_minimum_required(VERSION 3.4)
project(gmenu2x LANGUAGES CXX VERSION 1.0)

include(GNUInstallDirs)
include(FindPackageHandleStandardArgs)

option(CPUFREQ "Enable CPU frequency control" OFF)
if (CPUFREQ)
	add_compile_definitions(ENABLE_CPUFREQ)
endif(CPUFREQ)

option(CLOCK "Display current time at the bottom of the screen" ON)
if (CLOCK)
	add_compile_definitions(ENABLE_CLOCK)
endif (CLOCK)

option(BIND_CONSOLE "Support for binding/unbinding terminal" OFF)
if (BIND_CONSOLE)
	add_compile_definitions(BIND_CONSOLE)
endif (BIND_CONSOLE)

option(WINDOWED_MODE "Run windowed" OFF)
if (WINDOWED_MODE)
	add_compile_definitions(G2X_BUILD_OPTION_WINDOWED_MODE)
endif ()

set(SCREEN_WIDTH "" CACHE STRING "Screen / window width (empty: max available)")
if (SCREEN_WIDTH)
	add_compile_definitions(G2X_BUILD_OPTION_SCREEN_WIDTH=${SCREEN_WIDTH})
endif ()

set(SCREEN_HEIGHT "" CACHE STRING "Screen / window height (empty: max available)")
if (SCREEN_HEIGHT)
	add_compile_definitions(G2X_BUILD_OPTION_SCREEN_HEIGHT=${SCREEN_HEIGHT})
endif ()

set(SCREEN_DEPTH "" CACHE STRING "Screen / window depth (empty: max available)")
if (SCREEN_DEPTH)
	add_compile_definitions(G2X_BUILD_OPTION_SCREEN_DEPTH=${SCREEN_DEPTH})
endif ()

set(DEFAULT_FALLBACK_FONTS "" CACHE STRING "A list of fallback font paths")
if(DEFAULT_FALLBACK_FONTS)
	add_compile_definitions(DEFAULT_FALLBACK_FONTS=${DEFAULT_FALLBACK_FONTS})
endif ()

set(CARD_ROOT "/media" CACHE STRING "Top-level filesystem directory")

find_package(SDL REQUIRED)
find_package(SDL_image REQUIRED)
find_package(SDL_ttf REQUIRED)
find_package(PNG REQUIRED)

find_library(LIBSDL_GFX_LIBRARY SDL_gfx)
find_path(LIBSDL_GFX_INCLUDE_DIR SDL_gfxPrimitives.h ${SDL_INCLUDE_DIR})
find_package_handle_standard_args(libSDL_gfx DEFAULT_MSG
	LIBSDL_GFX_LIBRARY LIBSDL_GFX_INCLUDE_DIR)

if(LIBSDL_GFX_FOUND)
	set(LIBSDL_GFX_LIBRARIES ${LIBSDL_GFX_LIBRARY})
	set(LIBSDL_GFX_INCLUDE_DIRS ${LIBSDL_GFX_INCLUDE_DIR})
else()
	message(FATAL_ERROR "Unable to find SDL_gfx")
endif(LIBSDL_GFX_FOUND)

find_library(LIBOPK_LIBRARY opk)
find_path(LIBOPK_INCLUDE_DIR opk.h /usr/include)
find_package_handle_standard_args(libopk DEFAULT_MSG
	LIBOPK_LIBRARY LIBOPK_INCLUDE_DIR)

if(LIBOPK_FOUND)
	set(LIBOPK_LIBRARIES ${LIBOPK_LIBRARY})
	set(LIBOPK_INCLUDE_DIRS ${LIBOPK_INCLUDE_DIR})
	add_compile_definitions(HAVE_LIBOPK)

	option(INOTIFY "Monitor OPK folder with inotify" ON)
	if (INOTIFY)
		add_compile_definitions(ENABLE_INOTIFY)
	endif (INOTIFY)
endif(LIBOPK_FOUND)

find_library(LIBXDGMIME_LIBRARY xdgmime)
find_path(LIBXDGMIME_INCLUDE_DIR xdgmime.h /usr/include)
find_package_handle_standard_args(libxdgmime DEFAULT_MSG
	LIBXDGMIME_LIBRARY LIBXDGMIME_INCLUDE_DIR)

if (LIBXDGMIME_FOUND)
	set(LIBXDGMIME_LIBRARIES ${LIBXDGMIME_LIBRARY})
	set(LIBXDGMIME_INCLUDE_DIRS ${LIBXDGMIME_INCLUDE_DIR})
	add_compile_definitions(HAVE_LIBXDGMIME)
endif(LIBXDGMIME_FOUND)

file(GLOB OBJS src/*.cpp)

add_executable(${PROJECT_NAME} ${OBJS})

set_target_properties(${PROJECT_NAME} PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
)

target_link_libraries(${PROJECT_NAME} PRIVATE
					  ${SDL_LIBRARY}
					  ${SDL_IMAGE_LIBRARIES}
					  ${SDL_TTF_LIBRARIES}
					  ${LIBSDL_GFX_LIBRARIES}
					  ${PNG_LIBRARIES}
					  ${LIBOPK_LIBRARIES}
					  ${LIBXDGMIME_LIBRARIES}
					  stdc++fs
)

target_include_directories(${PROJECT_NAME} PRIVATE
						   ${SDL_INCLUDE_DIR}
						   ${SDL_IMAGE_INCLUDE_DIR}
						   ${SDL_TTF_INCLUDE_DIRS}
						   ${PNG_INCLUDE_DIRS}
						   ${LIBOPK_INCLUDE_DIRS}
						   ${LIBXDGMIME_INCLUDE_DIRS}
						   ${LIBSDL_GFX_INCLUDE_DIRS}
						   ${CMAKE_BINARY_DIR}
)

target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Debug>:-fsanitize=undefined;-fsanitize=address;-fsanitize-recover=address>")
target_link_libraries(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Debug>:-fsanitize=undefined;-fsanitize=address;-fsanitize-recover=address>")

install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY data/ DESTINATION ${CMAKE_INSTALL_DATADIR}/gmenu2x)

configure_file(buildopts.h.cmakein ${CMAKE_BINARY_DIR}/buildopts.h @ONLY)
