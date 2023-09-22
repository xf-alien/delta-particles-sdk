@echo off
echo Setting environment for minimal Visual C++ 6
set INCLUDE=%MSVCDir%\include
set LIB=%MSVCDir%\lib
set PATH=%MSVCDir%\bin;%PATH%

echo -- Compiler is MSVC6

set XASH3DSRC=..\..\Xash3D_original
set INCLUDES=-I../common -I../engine -I../pm_shared -I../game_shared -I../public -I../external -I../dlls -I../utils/vgui/include
set SOURCES=../dlls/crossbow.cpp ^
	../dlls/crowbar.cpp ^
	../dlls/egon.cpp ^
	../dlls/gauss.cpp ^
	../dlls/handgrenade.cpp ^
	../dlls/hornetgun.cpp ^
	../dlls/mp5.cpp ^
	../dlls/python.cpp ^
	../dlls/rpg.cpp ^
	../dlls/satchel.cpp ^
	../dlls/shotgun.cpp ^
	../dlls/squeakgrenade.cpp ^
	../dlls/tripmine.cpp ^
	../dlls/wpn_shared/hl_wpn_glock.cpp ^
	../dlls/pipewrench.cpp ^
	../dlls/sniperrifle.cpp ^
	../dlls/smg.cpp ^
	../dlls/desert_eagle.cpp ^
	ev_hldm.cpp ^
	hl/hl_baseentity.cpp ^
	hl/hl_events.cpp ^
	hl/hl_objects.cpp ^
	hl/hl_weapons.cpp ^
	ammo.cpp ^
	ammo_secondary.cpp ^
	ammohistory.cpp ^
	battery.cpp ^
	cdll_int.cpp ^
	com_weapons.cpp ^
	death.cpp ^
	demo.cpp ^
	entity.cpp ^
	ev_common.cpp ^
	events.cpp ^
	flashlight.cpp ^
	GameStudioModelRenderer.cpp ^
	geiger.cpp ^
	health.cpp ^
	hud.cpp ^
	hud_caption.cpp ^
	hud_msg.cpp ^
	hud_redraw.cpp ^
	hud_servers.cpp ^
	hud_spectator.cpp ^
	hud_update.cpp ^
	in_camera.cpp ^
	input.cpp ^
	inputw32.cpp ^
	menu.cpp ^
	message.cpp ^
	parsemsg.cpp ^
	particlemsg.cpp ^
	particlemgr.cpp ^
	particlesys.cpp ^
	../pm_shared/pm_debug.c ^
	../pm_shared/pm_math.c ^
	../pm_shared/pm_shared.c ^
	saytext.cpp ^
	status_icons.cpp ^
	statusbar.cpp ^
	studio_util.cpp ^
	StudioModelRenderer.cpp ^
	text_message.cpp ^
	train.cpp ^
	tri.cpp ^
	util.cpp ^
	view.cpp ^
	vgui_ClassMenu.cpp ^
	vgui_ConsolePanel.cpp ^
	vgui_CustomObjects.cpp ^
	vgui_int.cpp ^
	vgui_MOTDWindow.cpp ^
	vgui_SchemeManager.cpp ^
	vgui_ScorePanel.cpp ^
	vgui_ServerBrowser.cpp ^
	vgui_SpectatorPanel.cpp ^
	vgui_TeamFortressViewport.cpp ^
	vgui_teammenu.cpp ^
	../game_shared/vgui_checkbutton2.cpp ^
	../game_shared/vgui_grid.cpp ^
	../game_shared/vgui_helpers.cpp ^
	../game_shared/vgui_listbox.cpp ^
	../game_shared/vgui_loadtga.cpp ^
	../game_shared/vgui_scrollbar2.cpp ^
	../game_shared/vgui_slider2.cpp ^
	../game_shared/voice_banmgr.cpp ^
	../game_shared/voice_status.cpp ^
	../game_shared/voice_vgui_tweakdlg.cpp
set DEFINES=/DCLIENT_DLL /DCLIENT_WEAPONS /Dsnprintf=_snprintf /DNO_VOICEGAMEMGR /DNDEBUG
set LIBS=user32.lib Winmm.lib wsock32.lib ../utils/vgui/lib/win32_vc6/vgui.lib
set OUTNAME=client.dll

cl %DEFINES% %LIBS% %SOURCES% %INCLUDES% -o %OUTNAME% /link /dll /out:%OUTNAME% /release

echo -- Compile done. Cleaning...

del *.obj *.exp *.lib *.ilk
echo -- Done.
