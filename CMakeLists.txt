cmake_minimum_required(VERSION 2.6)
project(ProQuake)

set(PROQUAKE_SOURCES
# Common
	src/common.c
	src/cvar.c
	src/crc.c
	src/host.c
	src/host_cmd.c
	src/keys.c
	src/mathlib.c
	src/zone.c
	src/wad.c
	src/world.c
	src/sys_linux.c
# Client
	src/cl_demo.c
	src/cl_efrag.c
	src/cl_input.c
	src/cl_main.c
	src/cl_parse.c
	src/cl_particle.c
	src/cl_tent.c
	src/chase.c
	src/cmd.c
	src/console.c
	src/in_sdl.c
	src/menu.c
	src/sbar.c
	src/screen.c
	src/view.c
# Models
	src/mod_main.c
	src/mod_alias.c
	src/mod_brush.c
	src/mod_sprite.c
# Net
	src/net_dgrm.c
	src/net_loop.c
	src/net_main.c
	src/net_vcr.c
	src/net_udp.c
	src/net_bsd.c
# Program
	src/pr_cmds.c
	src/pr_edict.c
	src/pr_exec.c
# Server
	src/sv_main.c
	src/sv_phys.c
	src/sv_move.c
	src/sv_user.c
)

# Graphics
option(DISABLE_VIDEO "Disable graphics support" OFF)
if(DISABLE_VIDEO)
	set(PROQUAKE_SOURCES
		${PROQUAKE_SOURCES}
		src/gl_vidnull.c
	)
	set(GRAPHICS_LIBRARY "None")
else()
	set(PROQUAKE_SOURCES
		${PROQUAKE_SOURCES}
		src/gl_alias.c
		src/gl_draw.c
		src/gl_fullbright.c
		src/gl_light.c
		src/gl_main.c
		src/gl_particle.c
		src/gl_surface.c
		src/gl_sprite.c
		src/gl_texmgr.c
		src/gl_vidsdl.c
		src/gl_warp.c
	)
	set(GRAPHICS_LIBRARY "OpenGL" CACHE STRING "Select graphics library")
	set_property(CACHE GRAPHICS_LIBRARY PROPERTY STRINGS "OpenGL" "OpenGL ES")
endif()

# Sound
option(DISABLE_AUDIO "Disable sound support" OFF)
if(DISABLE_AUDIO)
	set(PROQUAKE_SOURCES
		${PROQUAKE_SOURCES}
		src/snd_null.c
	)
else()
	set(PROQUAKE_SOURCES
		${PROQUAKE_SOURCES}
		src/snd_dma.c
		src/snd_mem.c
		src/snd_mix.c
		src/snd_sdl.c
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
target_compile_options(proquake PRIVATE -g;-std=gnu99;-ffast-math;-Wall;-Werror)
install(TARGETS proquake RUNTIME DESTINATION bin)
