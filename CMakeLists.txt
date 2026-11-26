# FRONTIER: a space trading and combat RPG for the PlayStation
# Build with PSn00bSDK. See README.md.

cmake_minimum_required(VERSION 3.21)

project(
	FRONTIER
	LANGUAGES    C CXX ASM
	VERSION      0.20.1
	DESCRIPTION  "FRONTIER - space exploration, trading and combat RPG for the PlayStation"
)

psn00bsdk_add_executable(frontier GPREL
	src/main.c
	src/gfx.c
	src/input.c
	src/rng.c
	src/ship.c
	src/ships_data.c
	src/weapon.c
	src/fx.c
	src/drone.c
	src/enemy.c
	src/nav.c
	src/collide.c
	src/world.c src/dock.c src/outfit.c src/place.c src/clock.c src/market.c src/galaxy.c src/salvage.c src/faction.c src/escort.c src/sysmenu.c src/radar.c src/sprite.c src/sprites_data.c src/fit.c src/hulls_data.c src/editor.c src/scan.c src/mission.c src/save.c src/conquest.c src/galaxy_data.c src/veyr.c src/story.c src/audio.c src/audio_data.c src/title.c src/lounge.c src/model.c src/meshes_data.c src/font_data.c src/logo_data.c
)

# A division by zero must never freeze the console (MIPS traps on it by default):
# a wrong number for one frame is better than a hang.
target_compile_options(frontier PRIVATE -mno-check-zero-division)

# Test discs carry the dev menu (spawn ships, warp, credits, standings...).
# M16b: the dev menu ships on every disc, hidden behind the title-screen cheat code.
# -DFRONTIER_DEV=OFF still compiles it out completely.
option(FRONTIER_DEV "Include the dev menu" ON)
if(FRONTIER_DEV)
    target_compile_definitions(frontier PRIVATE FRONTIER_DEV)
endif()

psn00bsdk_add_cd_image(
	iso       # target name
	frontier  # output file name (frontier.bin + frontier.cue)
	iso.xml   # disc layout
	DEPENDS frontier system.cnf
)
