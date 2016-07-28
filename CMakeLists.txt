cmake_minimum_required(VERSION 2.6)
project(ProQuake)

set(PROQUAKE_SOURCES
	common.c
	cvar.c
	crc.c
	host.c
	host_cmd.c
	keys.c
	location.c
	menu.c
	mathlib.c
	zone.c
	view.c
	wad.c
	world.c
# Client
	cl_demo.c
	cl_input.c
	cl_main.c
	cl_parse.c
	cl_tent.c
	chase.c
	cmd.c
	console.c
	in_sdl.c
# Graphics
	gl_draw.c
	gl_mesh.c
	gl_model.c
	gl_refrag.c
	gl_rlight.c
	gl_rmain.c
	gl_rmisc.c
	gl_rsurf.c
	gl_screen.c
	gl_warp.c
	gl_fullbright.c
	gl_vidsdl.c
	sbar.c
# Net
	net_dgrm.c
	net_loop.c
	net_main.c
	net_vcr.c
# Program
	pr_cmds.c
	pr_edict.c
	pr_exec.c
	r_part.c
# Server
	sv_main.c
	sv_phys.c
	sv_move.c
	sv_user.c
# Sound
	snd_dma.c
	snd_mem.c
	snd_mix.c
)

if(WIN32)
	set(PROQUAKE_SOURCES ${PROQUAKE_SOURCES}
		windows/net_win.c
		windows/net_wins.c
		windows/snd_win.c
		windows/sys_win.c
	)
else()
	set(PROQUAKE_SOURCES ${PROQUAKE_SOURCES}
		net_udp.c
		net_bsd.c
		snd_linux.c
		sys_linux.c
	)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)

add_executable(proquake ${PROQUAKE_SOURCES})
target_compile_options(proquake PRIVATE -O3;-ffast-math;-funroll-loops;-fexpensive-optimizations;-Wall)
#target_compile_options(proquake PRIVATE -g)
target_include_directories(proquake PUBLIC ${OPENGL_INCLUDE_DIRS} ${SDL2_INCLUDE_DIR})
target_link_libraries(proquake m ${OPENGL_LIBRARIES} ${SDL2_LIBRARY})
