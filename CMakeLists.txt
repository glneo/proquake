cmake_minimum_required(VERSION 2.6)
project(ProQuake)

set(PROQUAKE_SOURCES
# Client
	cl_demo.c
	cl_input.c
	cl_main.c
	cl_parse.c
	cl_tent.c
	chase.c
	cmd.c
	common.c
	console.c
	crc.c
	cvar.c
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
	gl_test.c
	gl_warp.c
	gl_fullbright.c
	gl_vidfw.c
	host.c
	host_cmd.c
	iplog.c
	keys.c
	location.c
	matrix.c
	menu.c
	mathlib.c
# Net
	net_dgrm.c
	net_loop.c
	net_main.c
	net_vcr.c
	net_udp.c
	net_bsd.c
# Program
	pr_cmds.c
	pr_edict.c
	pr_exec.c
	r_part.c
	sbar.c
# Server
	sv_main.c
	sv_phys.c
	sv_move.c
	sv_user.c
	zone.c
	view.c
	wad.c
	world.c
	sys_linux.c
# Sound
	snd_dma.c
	snd_mem.c
	snd_mix.c
	snd_linux.c
	security.c
	vid_common_gl.c
)

add_executable(proquake ${PROQUAKE_SOURCES})
set_target_properties(proquake PROPERTIES COMPILE_FLAGS "-O3 -ffast-math -funroll-loops -fexpensive-optimizations")
target_link_libraries(proquake m GL glfw)
