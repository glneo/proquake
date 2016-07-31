CFLAGS += -O3 -ffast-math -funroll-loops
LDFLAGS += -lm -lGL -lSDL2

#############################################################################
# SETUP AND BUILD
#############################################################################

all: quake

#############################################################################
# Quake
#############################################################################

QUAKE_OBJS = \
	common.o \
	cvar.o \
	crc.o \
	host.o \
	host_cmd.o \
	keys.o \
	location.o \
	menu.o \
	mathlib.o \
	zone.o \
	view.o \
	wad.o \
	world.o \
	sys_linux.o \
	cl_demo.o \
	cl_input.o \
	cl_main.o \
	cl_parse.o \
	cl_tent.o \
	chase.o \
	cmd.o \
	console.o \
	in_sdl.o \
	gl_draw.o \
	gl_mesh.o \
	gl_model.o \
	gl_refrag.o \
	gl_rlight.o \
	gl_rmain.o \
	gl_rmisc.o \
	gl_rsurf.o \
	gl_screen.o \
	gl_warp.o \
	gl_fullbright.o \
	gl_vidsdl.o \
	sbar.o \
	net_dgrm.o \
	net_loop.o \
	net_main.o \
	net_vcr.o \
	net_udp.o \
	net_bsd.o \
	pr_cmds.o \
	pr_edict.o \
	pr_exec.o \
	r_part.o \
	sv_main.o \
	sv_phys.o \
	sv_move.o \
	sv_user.o \
	snd_dma.o \
	snd_mem.o \
	snd_mix.o \
	snd_sdl.o \

quake : $(QUAKE_OBJS)
	$(CC) -o $@ $(LDFLAGS) $^

%.o : %.c
	$(CC) -c -o $@ $(CFLAGS) $<

#############################################################################
# MISC
#############################################################################

clean:
	$(RM) quake.spec quake $(QUAKE_OBJS)
