cmake_minimum_required(VERSION 2.6)
project(ProQuake)

set(PROQUAKE_SOURCES
# Common
	src/common.cc
	src/cvar.cc
	src/crc.cc
	src/host.cc
	src/host_cmd.cc
	src/keys.cc
	src/mathlib.cc
	src/zone.cc
	src/wad.cc
	src/sys_linux.cc
# Client
	src/cl_demo.cc
	src/cl_efrag.cc
	src/cl_input.cc
	src/cl_main.cc
	src/cl_parse.cc
	src/cl_particle.cc
	src/cl_tent.cc
	src/chase.cc
	src/cmd.cc
	src/console.cc
	src/in_sdl.cc
	src/menu.cc
	src/sbar.cc
	src/screen.cc
	src/view.cc
# Models
	src/mod_main.cc
	src/mod_alias.cc
	src/mod_brush.cc
	src/mod_sprite.cc
# Net
	src/net_dgrm.cc
	src/net_loop.cc
	src/net_main.cc
	src/net_udp.cc
	src/net_vcr.cc
	src/net_bsd.cc
# Program
	src/pr_cmds.cc
	src/pr_edict.cc
	src/pr_exec.cc
# Server
	src/sv_main.cc
	src/sv_phys.cc
	src/sv_move.cc
	src/sv_user.cc
	src/sv_world.cc
)

# Graphics
option(DISABLE_VIDEO "Disable graphics support" OFF)
if(DISABLE_VIDEO)
	set(PROQUAKE_SOURCES
		${PROQUAKE_SOURCES}
		src/gl_vidnull.cc
	)
	set(GRAPHICS_LIBRARY "None")
else()
	set(PROQUAKE_SOURCES
		${PROQUAKE_SOURCES}
		src/gl_alias.cc
		src/gl_draw.cc
		src/gl_light.cc
		src/gl_main.cc
		src/gl_particle.cc
		src/gl_surface.cc
		src/gl_sprite.cc
		src/gl_texmgr.cc
		src/gl_vidsdl.cc
		src/gl_warp.cc
	)
	set(GRAPHICS_LIBRARY "OpenGL ES" CACHE STRING "Select graphics library")
	set_property(CACHE GRAPHICS_LIBRARY PROPERTY STRINGS "OpenGL ES" "OpenGL")
endif()

# Sound
option(DISABLE_AUDIO "Disable sound support" OFF)
if(DISABLE_AUDIO)
	set(PROQUAKE_SOURCES
		${PROQUAKE_SOURCES}
		src/snd_null.cc
	)
else()
	set(PROQUAKE_SOURCES
		${PROQUAKE_SOURCES}
		src/snd_dma.cc
		src/snd_mem.cc
		src/snd_mix.cc
		src/snd_sdl.cc
	)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

add_executable(proquake ${PROQUAKE_SOURCES} ${PROQUAKE_HEADERS})

if(GRAPHICS_LIBRARY STREQUAL "OpenGL")
	find_package(OpenGL REQUIRED)
	target_include_directories(proquake PUBLIC ${OPENGL_INCLUDE_DIRS})
	target_link_libraries(proquake ${OPENGL_LIBRARIES})
elseif(GRAPHICS_LIBRARY STREQUAL "OpenGL ES")
	find_package(OpenGLES REQUIRED)
	target_include_directories(proquake PUBLIC ${OPENGLES_INCLUDE_DIR})
	target_link_libraries(proquake ${OPENGLES_LIBRARIES})
	target_compile_definitions(proquake PRIVATE OPENGLES=1)
elseif(NOT DISABLE_VIDEO)
	message(FATAL_ERROR "Please select a graphics library" ${GRAPHICS_LIBRARY})
endif()

find_package(SDL2 REQUIRED)
target_include_directories(proquake PUBLIC ${SDL2_INCLUDE_DIR})
target_link_libraries(proquake m ${SDL2_LIBRARY})
target_compile_options(proquake PRIVATE -g;-std=c++11;-ffast-math;-Wall;-fpermissive;-Wno-writable-strings;-Wno-narrowing)
install(TARGETS proquake RUNTIME DESTINATION bin)
