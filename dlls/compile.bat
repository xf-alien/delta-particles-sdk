@echo off
echo Setting environment for minimal Visual C++ 6
set INCLUDE=%MSVCDir%\include
set LIB=%MSVCDir%\lib
set PATH=%MSVCDir%\bin;%PATH%

echo -- Compiler is MSVC6

set XASH3DSRC=..\..\Xash3D_original
set INCLUDES=-I../common -I../engine -I../pm_shared -I../game_shared -I../public -I.
set SOURCES=agrunt.cpp ^
	airtank.cpp ^
	aflock.cpp ^
	alias.cpp ^
	animating.cpp ^
	animation.cpp ^
	apache.cpp ^
	barnacle.cpp ^
	barney.cpp ^
	bigmomma.cpp ^
	bloater.cpp ^
	bmodels.cpp ^
	bullsquid.cpp ^
	buttons.cpp ^
	cbase.cpp ^
	client.cpp ^
	combat.cpp ^
	controller.cpp ^
	crossbow.cpp ^
	crowbar.cpp ^
	defaultai.cpp ^
	doors.cpp ^
	desert_eagle.cpp ^
	effects.cpp ^
	egon.cpp ^
	explode.cpp ^
	female_npc.cpp ^
	flyingmonster.cpp ^
	func_break.cpp ^
	func_tank.cpp ^
	game.cpp ^
	gamerules.cpp ^
	gargantua.cpp ^
	gauss.cpp ^
	genericmonster.cpp ^
	ggrenade.cpp ^
	globals.cpp ^
	wpn_shared/hl_wpn_glock.cpp ^
	gman.cpp ^
	h_ai.cpp ^
	h_battery.cpp ^
	h_cine.cpp ^
	h_cycler.cpp ^
	h_export.cpp ^
	handgrenade.cpp ^
	hassassin.cpp ^
	headcrab.cpp ^
	healthkit.cpp ^
	hgrunt.cpp ^
	hornet.cpp ^
	hornetgun.cpp ^
	houndeye.cpp ^
	ichthyosaur.cpp ^
	islave.cpp ^
	items.cpp ^
	leech.cpp ^
	lights.cpp ^
	locus.cpp ^
	maprules.cpp ^
	monstermaker.cpp ^
	monsters.cpp ^
	monsterstate.cpp ^
	mortar.cpp ^
	movewith.cpp ^
	mp5.cpp ^
	multiplay_gamerules.cpp ^
	nihilanth.cpp ^
	nodes.cpp ^
	osprey.cpp ^
	otis.cpp ^
	pathcorner.cpp ^
	pipewrench.cpp ^
	plane.cpp ^
	plats.cpp ^
	player.cpp ^
	playermonster.cpp ^
	python.cpp ^
	rat.cpp ^
	roach.cpp ^
	robotic_infantry.cpp ^
	rpg.cpp ^
	satchel.cpp ^
	AI_BaseNPC_Schedule.cpp ^
	scientist.cpp ^
	scripted.cpp ^
	shotgun.cpp ^
	singleplay_gamerules.cpp ^
	skill.cpp ^
	smg.cpp ^
	sniperrifle.cpp ^
	sound.cpp ^
	soundent.cpp ^
	spectator.cpp ^
	squadmonster.cpp ^
	squeakgrenade.cpp ^
	subs.cpp ^
	talkmonster.cpp ^
	teamplay_gamerules.cpp ^
	technician.cpp ^
	tempmonster.cpp ^
	tentacle.cpp ^
	triggers.cpp ^
	tripmine.cpp ^
	turret.cpp ^
	util.cpp ^
	weapons.cpp ^
	world.cpp ^
	xen.cpp ^
	zombie.cpp ^
	../pm_shared/pm_debug.c ../pm_shared/pm_math.c ../pm_shared/pm_shared.c ../game_shared/voice_gamemgr.cpp
set DEFINES=/DCLIENT_WEAPONS /Dsnprintf=_snprintf /DNO_VOICEGAMEMGR /DNDEBUG
set LIBS=user32.lib
set OUTNAME=hl.dll

cl %DEFINES% %LIBS% %SOURCES% %INCLUDES% -o %OUTNAME% /link /dll /out:%OUTNAME% /release /def:".\hl.def"

echo -- Compile done. Cleaning...

del *.obj *.exp *.lib *.ilk
echo -- Done.
