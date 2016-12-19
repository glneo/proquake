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
	src/location.c
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
else(DISABLE_VIDEO)
	set(PROQUAKE_SOURCES
		${PROQUAKE_SOURCES}
		src/gl_alias.c
		src/gl_draw.c
		src/gl_fullbright.c
		src/gl_rlight.c
		src/gl_rmain.c
		src/gl_particle.c
		src/gl_rsurf.c
		src/gl_screen.c
		src/gl_sprite.c
		src/gl_texture.c
		src/gl_vidsdl.c
		src/gl_warp.c
		src/menu.c
		src/sbar.c
		src/view.c
	)
endif(DISABLE_VIDEO)

# Sound
option(DISABLE_AUDIO "Disable sound support" OFF)
if(DISABLE_AUDIO)
	set(PROQUAKE_SOURCES
		${PROQUAKE_SOURCES}
		src/snd_null.c
	)
else(DISABLE_AUDIO)
	set(PROQUAKE_SOURCES
		${PROQUAKE_SOURCES}
		src/snd_dma.c
		src/snd_mem.c
		src/snd_mix.c
		src/snd_sdl.c
	)
endif(DISABLE_AUDIO)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_package(OpenGLES REQUIRED)
#find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)

add_executable(proquake ${PROQUAKE_SOURCES} ${PROQUAKE_HEADERS})
#target_compile_options(proquake PRIVATE -O3;-ffast-math;-Wall)
target_compile_options(proquake PRIVATE -g;-Wall;-std=gnu99)
target_include_directories(proquake PUBLIC ${OPENGLES_INCLUDE_DIR} ${SDL2_INCLUDE_DIR})
target_link_libraries(proquake m ${OPENGLES_LIBRARIES} ${SDL2_LIBRARY})
#target_include_directories(proquake PUBLIC ${OPENGL_INCLUDE_DIRS} ${SDL2_INCLUDE_DIR})
#target_link_libraries(proquake m ${OPENGL_LIBRARIES} ${SDL2_LIBRARY})
install(TARGETS proquake RUNTIME DESTINATION bin)