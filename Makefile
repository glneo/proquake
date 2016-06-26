CFLAGS += -O3 -g -ffast-math -funroll-loops -fexpensive-optimizations
LDFLAGS += -lm -lGL -lglfw

#############################################################################
# SETUP AND BUILD
#############################################################################

all: glquake

#############################################################################
# GLQuake
#############################################################################

GLQUAKE_OBJS = \
	cl_demo.o \
	cl_input.o \
	cl_main.o \
	cl_parse.o \
	cl_tent.o \
	chase.o \
	cmd.o \
	common.o \
	console.o \
	crc.o \
	cvar.o \
	\
	gl_draw.o \
	gl_mesh.o \
	gl_model.o \
	gl_refrag.o \
	gl_rlight.o \
	gl_rmain.o \
	gl_rmisc.o \
	gl_rsurf.o \
	gl_screen.o \
	gl_test.o \
	gl_warp.o \
	gl_fullbright.o \
	\
	host.o \
	host_cmd.o \
	iplog.o \
	keys.o \
	location.o \
	matrix.o \
	menu.o \
	mathlib.o \
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
	sbar.o \
	sv_main.o \
	sv_phys.o \
	sv_move.o \
	sv_user.o \
	zone.o	\
	view.o	\
	wad.o \
	world.o \
	sys_linux.o \
	snd_dma.o \
	snd_mem.o \
	snd_mix.o \
	snd_linux.o \
	security.o \
	vid_common_gl.o

FW_OBJS = gl_vidfw.o

glquake : $(GLQUAKE_OBJS) $(FW_OBJS)
	$(CC) -o $@ $(GLQUAKE_OBJS) $(FW_OBJS) $(LDFLAGS)

%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

#############################################################################
# MISC
#############################################################################

clean:
	$(RM) glquake.spec glquake $(GLQUAKE_OBJS) $(FW_OBJS)
