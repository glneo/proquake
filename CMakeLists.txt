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
	src/matrix.cc
	src/zone.cc
	src/wad.cc
	src/sys_sdl.cc
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
	src/r_main.cc
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
		src/gl_shader.cc
		src/gl_brush.cc
		src/gl_sprite.cc
		src/gl_texmgr.cc
		src/gl_vidsdl.cc
		src/gl_warp.cc
	)
	set(GRAPHICS_LIBRARY "OpenGL ES2" CACHE STRING "Select graphics library")
	set_property(CACHE GRAPHICS_LIBRARY PROPERTY STRINGS "OpenGL ES2" "OpenGL")
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
		src/snd_main.cc
		src/snd_mem.cc
		src/snd_mix.cc
		src/snd_sdl.cc
	)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_program(XXD xxd)
set(PROQUAKE_SHADER_SOURCES
	src/alias.fs
	src/alias.vs
	src/brush.fs
	src/brush.vs
	src/draw.fs
	src/draw.vs
	src/particle.fs
	src/particle.vs
)
foreach(INPUT_FILE ${PROQUAKE_SHADER_SOURCES})
	get_filename_component(INPUT_FILENAME ${INPUT_FILE} NAME)
	set(OUTPUT_FILENAME ${INPUT_FILENAME}.h)
	set(OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILENAME})
	add_custom_command(
		OUTPUT ${OUTPUT_FILE}
		DEPENDS ${INPUT_FILE}
		COMMAND ${XXD} -i
			${INPUT_FILENAME}
			> ${OUTPUT_FILE}
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
		COMMENT "Converting ${INPUT_FILE} to header file")
	list(APPEND PROQUAKE_SHADERS ${OUTPUT_FILE})
endforeach()

add_executable(proquake ${PROQUAKE_SOURCES} ${PROQUAKE_HEADERS} ${PROQUAKE_SHADERS})

target_include_directories(proquake PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

if(GRAPHICS_LIBRARY STREQUAL "OpenGL")
	find_package(OpenGL REQUIRED)
	target_include_directories(proquake PUBLIC ${OPENGL_INCLUDE_DIRS})
	target_link_libraries(proquake ${OPENGL_LIBRARIES})
elseif(GRAPHICS_LIBRARY STREQUAL "OpenGL ES2")
	find_package(OpenGLES2 REQUIRED)
	target_include_directories(proquake PUBLIC ${OPENGLES2_INCLUDE_DIR})
	target_link_libraries(proquake ${OPENGLES2_LIBRARIES})
	target_compile_definitions(proquake PRIVATE OPENGLES=1)
elseif(NOT DISABLE_VIDEO)
	message(FATAL_ERROR "Please select a graphics library" ${GRAPHICS_LIBRARY})
endif()

find_package(SDL2 REQUIRED)
target_include_directories(proquake PUBLIC ${SDL2_INCLUDE_DIR})
target_link_libraries(proquake m ${SDL2_LIBRARY})
target_compile_options(proquake PRIVATE -g;-O0;-std=c++11;-ffast-math;-Wall;-fno-omit-frame-pointer)
# -Wconversion;-Werror;-fsanitize=address;
install(TARGETS proquake RUNTIME DESTINATION bin)
