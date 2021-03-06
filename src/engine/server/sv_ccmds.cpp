/*
===========================================================================

OpenWolf GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 
Copyright (C) 2012 Dusan Jocic <dusanjocic@msn.com>

This file is part of the OpenWolf GPL Source Code (OpenWolf Source Code).  

OpenWolf Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

OpenWolf Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenWolf Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the OpenWolf Source Code is also subject to certain additional terms. 
You should have received a copy of these additional terms immediately following the 
terms and conditions of the GNU General Public License which accompanied the OpenWolf 
Source Code.  If not, please request a copy in writing from id Software at the address 
below.

If you have questions concerning this license or the applicable additional terms, you 
may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, 
Maryland 20850 USA.

===========================================================================
*/


#include "server.h"

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/


/*
==================
SV_GetPlayerByName

Returns the player with name from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByName(void) {
	client_t       *cl;
	int             i;
	char           *s, cleanName[64];

	// make sure server is running
	if(!com_sv_running->integer) {
		return NULL;
	}

	if(Cmd_Argc() < 2) {
		Com_Printf("No player specified.\n");
		return NULL;
	}

	s = Cmd_Argv(1);

	// check for a name match
	for(i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if(cl->state <= CS_ZOMBIE) {
			continue;
		}
		if(!Q_stricmp(cl->name, s)) {
			return cl;
		}

		Q_strncpyz(cleanName, cl->name, sizeof(cleanName));
		Q_CleanStr(cleanName);
		if(!Q_stricmp(cleanName, s)) {
			return cl;
		}
	}

	Com_Printf("Player %s is not on the server\n", s);

	return NULL;
}

/*
==================
SV_Map_f

Restart the server on a different map
==================
*/
static void SV_Map_f(void) {
	char           *cmd, *map, smapname[MAX_QPATH], mapname[MAX_QPATH], expanded[MAX_QPATH], *cl_profileStr = Cvar_VariableString("cl_profile");
	qboolean        killBots, cheat, buildScript;
	int             savegameTime = -1;

	map = Cmd_Argv(1);
	if(!map) {
		return;
	}

	if(!com_gameInfo.spEnabled) {
		if(!Q_stricmp(Cmd_Argv(0), "spdevmap") || !Q_stricmp(Cmd_Argv(0), "spmap")) {
			Com_Printf("Single Player is not enabled.\n");
			return;
		}
	}

	buildScript = (qboolean)Cvar_VariableIntegerValue("com_buildScript");

	if(SV_GameIsSinglePlayer()) {
		if(!buildScript && sv_reloading->integer && sv_reloading->integer != RELOAD_NEXTMAP) { // game is in 'reload' mode, don't allow starting new maps yet.
			return;
		}

		// Trap a savegame load
		if(strstr(map, ".sav")) {
			// open the savegame, read the mapname, and copy it to the map string
			char            savemap[MAX_QPATH], savedir[MAX_QPATH];
			byte           *buffer;
			int             size, csize;

			if(com_gameInfo.usesProfiles && cl_profileStr[0]) {
				Com_sprintf(savedir, sizeof(savedir), "profiles/%s/save/", cl_profileStr);
			} else {
				Q_strncpyz(savedir, "save/", sizeof(savedir));
			}

			if(!(strstr(map, savedir) == map)) {
				Com_sprintf(savemap, sizeof(savemap), "%s%s", savedir, map);
			} else {
				strcpy(savemap, map);
			}

			size = FS_ReadFile(savemap, NULL);
			if(size < 0) {
				Com_Printf("Can't find savegame %s\n", savemap);
				return;
			}

			//buffer = Hunk_AllocateTempMemory(size);
			FS_ReadFile(savemap, (void **)&buffer);

			if(Q_stricmp(savemap, va("%scurrent.sav", savedir)) != 0)
			{
				// copy it to the current savegame file
				FS_WriteFile(va("%scurrent.sav", savedir), buffer, size);
				// make sure it is the correct size
				csize = FS_ReadFile(va("%scurrent.sav", savedir), NULL);
				if(csize != size) {
					Hunk_FreeTempMemory(buffer);
					FS_Delete(va("%scurrent.sav", savedir));
// TTimo
#ifdef __linux__
					Com_Error(ERR_DROP, "Unable to save game.\n\nPlease check that you have at least 5mb free of disk space in your home directory.");
#else
					Com_Error(ERR_DROP, "Insufficient free disk space.\n\nPlease free at least 5mb of free space on game drive.");
#endif
					return;
				}
			}

			// set the cvar, so the game knows it needs to load the savegame once the clients have connected
			Cvar_Set("savegame_loading", "1");
			// set the filename
			Cvar_Set("savegame_filename", savemap);

			// the mapname is at the very start of the savegame file
			Q_strncpyz(savemap, (char *)(buffer + sizeof(int)), sizeof(savemap));	// skip the version
			Q_strncpyz(smapname, savemap, sizeof(smapname));
			map = smapname;

			savegameTime = *(int *)(buffer + sizeof(int) + MAX_QPATH);

			if(savegameTime >= 0) {
				svs.time = savegameTime;
			}

			Hunk_FreeTempMemory(buffer);
		} else {
			Cvar_Set("savegame_loading", "0");	// make sure it's turned off
												// set the filename
			Cvar_Set("savegame_filename", "");
		}
	} else {
		Cvar_Set("savegame_loading", "0");	// make sure it's turned off
											// set the filename
		Cvar_Set("savegame_filename", "");
	}

	// make sure the level exists before trying to change, so that
	// a typo at the server console won't end the game
	Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);
	
	if(FS_ReadFile(expanded, NULL) == -1) {
		Com_Printf("Can't find map %s\n", expanded);
		return;
	}

	Cvar_Set("gamestate", va("%i", GS_INITIALIZE));	// NERVE - SMF - reset gamestate on map/devmap

	Cvar_Set("g_currentRound", "0");				// NERVE - SMF - reset the current round
	Cvar_Set("g_nextTimeLimit", "0");				// NERVE - SMF - reset the next time limit

	// START    Mad Doctor I changes, 8/14/2002.  Need a way to force load a single player map as single player
	if(!Q_stricmp(Cmd_Argv(0), "spdevmap") || !Q_stricmp(Cmd_Argv(0), "spmap")) {
		// This is explicitly asking for a single player load of this map
		Cvar_Set("g_gametype", va("%i", com_gameInfo.defaultSPGameType));
		// force latched values to get set
		Cvar_Get("g_gametype", va("%i", com_gameInfo.defaultSPGameType), CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH, "test");
		// enable bot support for AI
		Cvar_Set("bot_enable", "1");
	}

	// Rafael gameskill
//  Cvar_Get ("g_gameskill", "3", CVAR_SERVERINFO | CVAR_LATCH);
	// done

	cmd = Cmd_Argv(0);

	if(!Q_stricmp(cmd, "devmap")) {
		cheat = qtrue;
		killBots = qtrue;
	} else if(!Q_stricmp(Cmd_Argv(0), "spdevmap")) {
		cheat = qtrue;
		killBots = qtrue;
	} else {
		cheat = qfalse;
		killBots = qfalse;
	}

	// save the map name here cause on a map restart we reload the q3config.cfg
	// and thus nuke the arguments of the map command
	Q_strncpyz(mapname, map, sizeof(mapname));

	// start up the map
	SV_SpawnServer(mapname, killBots);

	// set the cheat value
	// if the level was started with "map <levelname>", then
	// cheats will not be allowed.  If started with "devmap <levelname>"
	// then cheats will be allowed
	if(cheat) {
		Cvar_Set("sv_cheats", "1");
	} else {
		Cvar_Set("sv_cheats", "0");
	}
}

/*
================
SV_CheckTransitionGameState

NERVE - SMF
================
*/
static qboolean SV_CheckTransitionGameState(gamestate_t new_gs, gamestate_t old_gs) {
	if(old_gs == new_gs && new_gs != GS_PLAYING) {
		return qfalse;
	}

//  if ( old_gs == GS_WARMUP && new_gs != GS_WARMUP_COUNTDOWN )
//      return qfalse;

//  if ( old_gs == GS_WARMUP_COUNTDOWN && new_gs != GS_PLAYING )
//      return qfalse;

	if(old_gs == GS_WAITING_FOR_PLAYERS && new_gs != GS_WARMUP) {
		return qfalse;
	}

	if(old_gs == GS_INTERMISSION && new_gs != GS_WARMUP) {
		return qfalse;
	}

	if(old_gs == GS_RESET && (new_gs != GS_WAITING_FOR_PLAYERS && new_gs != GS_WARMUP)) {
		return qfalse;
	}

	return qtrue;
}

/*
================
SV_TransitionGameState

NERVE - SMF
================
*/
static qboolean SV_TransitionGameState(gamestate_t new_gs, gamestate_t old_gs, int delay) {
	if(!SV_GameIsSinglePlayer() && !SV_GameIsCoop()) {
		// we always do a warmup before starting match
		if(old_gs == GS_INTERMISSION && new_gs == GS_PLAYING) {
			new_gs = GS_WARMUP;
		}
	}

	// check if its a valid state transition
	if(!SV_CheckTransitionGameState(new_gs, old_gs)) {
		return qfalse;
	}

	if(new_gs == GS_RESET) {
		new_gs = GS_WARMUP;
	}

	Cvar_Set("gamestate", va("%i", new_gs));

	return qtrue;
}

void MSG_PrioritiseEntitystateFields(void);
void MSG_PrioritisePlayerStateFields(void);

/*
================
SV_FieldInfo_f
================
*/
static void SV_FieldInfo_f(void) {
	MSG_PrioritiseEntitystateFields();
	MSG_PrioritisePlayerStateFields();
}

/*
================
SV_MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
static void SV_MapRestart_f(void) {
	int             i, delay = 0;
	client_t       *client;
	char           *denied;
	qboolean        isBot;
	gamestate_t     new_gs, old_gs;	// NERVE - SMF

	// make sure we aren't restarting twice in the same frame
	if(com_frameTime == sv.serverId) {
		return;
	}

	// make sure server is running
	if(!com_sv_running->integer) {
		Com_Printf("Server is not running.\n");
		return;
	}

	if(Cmd_Argc() > 1) {
		delay = atoi(Cmd_Argv(1));
	}

	if(delay) {
		sv.restartTime = svs.time + delay * 1000;
		SV_SetConfigstring(CS_WARMUP, va("%i", sv.restartTime));
		return;
	}

	// NERVE - SMF - read in gamestate or just default to GS_PLAYING
	old_gs = (gamestate_t)atoi(Cvar_VariableString("gamestate"));

	if(SV_GameIsSinglePlayer() || SV_GameIsCoop()) {
		new_gs = GS_PLAYING;
	} else {
		if(Cmd_Argc() > 2) {
			new_gs = (gamestate_t)atoi(Cmd_Argv(2));
		} else {
			new_gs = GS_PLAYING;
		}
	}

	if(!SV_TransitionGameState(new_gs, old_gs, delay)) {
		return;
	}

	// check for changes in variables that can't just be restarted
	// check for maxclients change
	if(sv_maxclients->modified) {
		char mapname[MAX_QPATH];

		Com_Printf("sv_maxclients variable change -- restarting.\n");
		// restart the map the slow way
		Q_strncpyz(mapname, Cvar_VariableString("mapname"), sizeof(mapname));

		SV_SpawnServer(mapname, qfalse);
		return;
	}

	// Check for loading a saved game
	if(Cvar_VariableIntegerValue("savegame_loading")) {
		// open the current savegame, and find out what the time is, everything else we can ignore
		char    savemap[MAX_QPATH], *cl_profileStr = Cvar_VariableString("cl_profile");
		byte    *buffer;
		int     size, savegameTime;

		if(com_gameInfo.usesProfiles) {
			Com_sprintf(savemap, sizeof(savemap), "profiles/%s/save/current.sav", cl_profileStr);
		} else {
			Q_strncpyz(savemap, "save/current.sav", sizeof(savemap));
		}

		size = FS_ReadFile(savemap, NULL);
		if(size < 0) {
			Com_Printf("Can't find savegame %s\n", savemap);
			return;
		}

		//buffer = Hunk_AllocateTempMemory(size);
		FS_ReadFile(savemap, (void **)&buffer);

		// the mapname is at the very start of the savegame file
		savegameTime = *(int *)(buffer + sizeof(int) + MAX_QPATH);

		if(savegameTime >= 0) {
			svs.time = savegameTime;
		}

		Hunk_FreeTempMemory(buffer);
	}
	// done.

	// toggle the server bit so clients can detect that a
	// map_restart has happened
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// generate a new serverid
	// TTimo - don't update restartedserverId there, otherwise we won't deal correctly with multiple map_restart
	sv.serverId = com_frameTime;
	Cvar_Set("sv_serverid", va("%i", sv.serverId));

	// reset all the vm data in place without changing memory allocation
	// note that we do NOT set sv.state = SS_LOADING, so configstrings that
	// had been changed from their default values will generate broadcast updates
	sv.state = SS_LOADING;
	sv.restarting = qtrue;

	Cvar_Set("sv_serverRestarting", "1");

	SV_RestartGameProgs();

	// run a few frames to allow everything to settle
	for(i = 0; i < GAME_INIT_FRAMES; i++)
	{
		VM_Call(gvm, GAME_RUN_FRAME, svs.time);
		svs.time += FRAMETIME;
	}

	// create a baseline for more efficient communications
	// Gordon: meh, this wont work here as the client doesn't know it has happened
//  SV_CreateBaseline ();

	sv.state = SS_GAME;
	sv.restarting = qfalse;

	// connect and begin all the clients
	for(i = 0; i < sv_maxclients->integer; i++) {
		client = &svs.clients[i];

		// send the new gamestate to all connected clients
		if(client->state < CS_CONNECTED) {
			continue;
		}

		if(client->netchan.remoteAddress.type == NA_BOT) {
			if(SV_GameIsSinglePlayer() || SV_GameIsCoop()) {
				continue;		// dont carry across bots in single player
			}
			isBot = qtrue;
		} else {
			isBot = qfalse;
		}

		// add the map_restart command
		SV_AddServerCommand(client, "map_restart\n");

		// connect the client again, without the firstTime flag
		denied = (char*)VM_ExplicitArgPtr(gvm, VM_Call(gvm, GAME_CLIENT_CONNECT, i, qfalse, isBot));
		if(denied)
		{
			// this generally shouldn't happen, because the client
			// was connected before the level change
			SV_DropClient(client, denied);
			if((!SV_GameIsSinglePlayer()) || (!isBot)) {
				Com_Printf("SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i);	// bk010125
			}
			continue;
		}

		client->state = CS_ACTIVE;

		SV_ClientEnterWorld(client, &client->lastUsercmd);
	}

	// run another frame to allow things to look at all the players
	VM_Call(gvm, GAME_RUN_FRAME, svs.time);
	svs.time += FRAMETIME;

	Cvar_Set("sv_serverRestarting", "0");
}

/*
==================
SV_RehashServerRconFile

Helper to reload a "serverRcon_t" type of file
Returns number of entries loaded
==================
*/
static int SV_RehashServerRconFile(convar_t *fileName, int maxEntries, serverRcon_t buffer[])
{
	int index, filelen, numEntries = 0;
	fileHandle_t readfrom;
	char *textbuf, *curpos, *maskpos, *newlinepos, *endpos, filepath[MAX_QPATH];

	if(!fileName->string || !*fileName->string)
	{
		goto exit;
	}

	if(!(curpos = Cvar_VariableString("fs_game")) || !*curpos)
	{
		curpos = BASEGAME;
	}

	Com_sprintf(filepath, sizeof(filepath), "%s/%s", curpos, fileName->string);

	if((filelen = FS_SV_FOpenFileRead(filepath, &readfrom)) < 0)
	{
		Com_Printf("SV_RehashServerRconFile: failed to open %s\n", filepath);
		goto exit;
	}

	if(filelen < 2)
	{
		// Don't bother if file is too short.
		FS_FCloseFile(readfrom);
		goto exit;
	}

	curpos = textbuf = (char*)Z_Malloc(filelen);

	filelen = FS_Read(textbuf, filelen, readfrom);
	FS_FCloseFile(readfrom);

	endpos = textbuf + filelen;

	for(index = 0; index < maxEntries && curpos + 2 < endpos; index++)
	{
		// find the end of the address string
		for(maskpos = curpos + 2; maskpos < endpos && *maskpos != ' '; maskpos++);

		if(maskpos + 1 >= endpos)
		{
			break;
		}

		*maskpos = '\0';
		maskpos++;

		// find the end of the subnet specifier
		for(newlinepos = maskpos; newlinepos < endpos && *newlinepos != '\n'; newlinepos++);

		if(newlinepos >= endpos)
		{
			break;
		}

		*newlinepos = '\0';

		if(NET_StringToAdr(curpos + 2, &buffer[index].ip, NA_UNSPEC))
		{
			buffer[index].isexception = (qboolean)(curpos[0] != '0');
			buffer[index].subnet = atoi(maskpos);

			if(buffer[index].ip.type == NA_IP && (buffer[index].subnet < 1 || buffer[index].subnet > 32))
			{
				buffer[index].subnet = 32;
			}
			else if(buffer[index].ip.type == NA_IP6 && (buffer[index].subnet < 1 || buffer[index].subnet > 128))
			{
				buffer[index].subnet = 128;
			}
		}

		curpos = newlinepos + 1;
	}

	Z_Free(textbuf);
	numEntries = index;

exit:
	return numEntries;
}

/*
==================
SV_RehashRconWhitelist_f

Load RCON whitelist from file.
==================
*/
static void SV_RehashRconWhitelist_f(void)
{
	rconWhitelistCount = SV_RehashServerRconFile(sv_WhiteListRcon, MAX_RCON_WHITELIST, rconWhitelist);
}

/*
=================
SV_LoadGame_f
=================
*/
void SV_LoadGame_f(void) {
	char            filename[MAX_QPATH], mapname[MAX_QPATH], savedir[MAX_QPATH], *cl_profileStr = Cvar_VariableString("cl_profile");
	byte           *buffer;
	int             size;

	// dont allow command if another loadgame is pending
	if(Cvar_VariableIntegerValue("savegame_loading")) {
		return;
	}
	if(sv_reloading->integer) {
		// (SA) disabling
//  if(sv_reloading->integer && sv_reloading->integer != RELOAD_FAILED )    // game is in 'reload' mode, don't allow starting new maps yet.
		return;
	}

	Q_strncpyz(filename, Cmd_Argv(1), sizeof(filename));
	if(!filename[0]) {
		Com_Printf("You must specify a savegame to load\n");
		return;
	}

	if(com_gameInfo.usesProfiles && cl_profileStr[0]) {
		Com_sprintf(savedir, sizeof(savedir), "profiles/%s/save/", cl_profileStr);
	} else {
		Q_strncpyz(savedir, "save/", sizeof(savedir));
	}

	/*if ( Q_strncmp( filename, "save/", 5 ) && Q_strncmp( filename, "save\\", 5 ) ) {
	   Q_strncpyz( filename, va("save/%s", filename), sizeof( filename ) );
	   } */

	// go through a va to avoid vsnprintf call with same source and target
	Q_strncpyz(filename, va("%s%s", savedir, filename), sizeof(filename));

	// enforce .sav extension
	if(!strstr(filename, ".") || Q_strncmp(strstr(filename, ".") + 1, "sav", 3)) {
		Q_strcat(filename, sizeof(filename), ".sav");
	}
	// use '/' instead of '\\' for directories
	while(strstr(filename, "\\")) {
		*(char *)strstr(filename, "\\") = '/';
	}

	size = FS_ReadFile(filename, NULL);
	if(size < 0) {
		Com_Printf("Can't find savegame %s\n", filename);
		return;
	}

	//buffer = Hunk_AllocateTempMemory(size);
	FS_ReadFile(filename, (void **)&buffer);

	// read the mapname, if it is the same as the current map, then do a fast load
	Q_strncpyz(mapname, (const char *)(buffer + sizeof(int)), sizeof(mapname));

	if(com_sv_running->integer && (com_frameTime != sv.serverId)) {
		// check mapname
		if(!Q_stricmp(mapname, sv_mapname->string)) { // same

			if(Q_stricmp(filename, va("%scurrent.sav", savedir)) != 0) {
				// copy it to the current savegame file
				FS_WriteFile(va("%scurrent.sav", savedir), buffer, size);
			}

			Hunk_FreeTempMemory(buffer);

			Cvar_Set("savegame_loading", "2");	// 2 means it's a restart, so stop rendering until we are loaded
			// set the filename
			Cvar_Set("savegame_filename", filename);
			// quick-restart the server
			SV_MapRestart_f();	// savegame will be loaded after restart

			return;
		}
	}

	Hunk_FreeTempMemory(buffer);

	// otherwise, do a slow load
	if(Cvar_VariableIntegerValue("sv_cheats")) {
		Cbuf_ExecuteText(EXEC_APPEND, va("spdevmap %s", filename));
	} else { // no cheats
		Cbuf_ExecuteText(EXEC_APPEND, va("spmap %s", filename));
	}
}

/*
==================
SV_TempBanNetAddress
==================
*/
void SV_TempBanNetAddress(netadr_t address, int length) {
	int i, oldesttime = 0, oldest = -1;

	for(i = 0; i < MAX_TEMPBAN_ADDRESSES; i++) {
		if(!svs.tempBanAddresses[i].endtime || svs.tempBanAddresses[i].endtime < svs.time) {
			// found a free slot
			svs.tempBanAddresses[i].adr = address;
			svs.tempBanAddresses[i].endtime = svs.time + (length * 1000);
			return;
		} else {
			if(oldest == -1 || oldesttime > svs.tempBanAddresses[i].endtime) {
				oldesttime = svs.tempBanAddresses[i].endtime;
				oldest = i;
			}
		}
	}

	svs.tempBanAddresses[oldest].adr = address;
	svs.tempBanAddresses[oldest].endtime = svs.time + length;
}

/*
==================
SV_TempBanIsBanned
==================
*/
qboolean SV_TempBanIsBanned(netadr_t address)
{
	int i;

	for(i = 0; i < MAX_TEMPBAN_ADDRESSES; i++) {
		if(svs.tempBanAddresses[i].endtime && svs.tempBanAddresses[i].endtime > svs.time) {
			if(NET_CompareAdr(address, svs.tempBanAddresses[i].adr)) {
				return qtrue;
			}
		}
	}

	return qfalse;
}

/*
================
SV_Status_f
================
*/
static void SV_Status_f(void) {
	int             i, j, l, ping;
	client_t       *cl;
	playerState_t  *ps;
	const char     *s;

	// Dushan
	float           cpu, avg;

	// make sure server is running
	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running.\n");
		return;
	}

	// Dushan
	cpu = (svs.stats.latched_active + svs.stats.latched_idle);
	if (cpu) {
		cpu = 100 * svs.stats.latched_active / cpu;
	}
	avg = 1000 * svs.stats.latched_active / STATFRAMES;

	Com_Printf ("cpu utilization  : %3i%%\n",(int)cpu);
	Com_Printf ("avg response time: %i ms\n",(int)avg);

	Com_Printf("map: %s\n", sv_mapname->string);

	Com_Printf("num score ping name            lastmsg address               qport rate\n");
	Com_Printf("--- ----- ---- --------------- ------- --------------------- ----- -----\n");
	for(i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if(!cl->state) {
			continue;
		}
		Com_Printf("%3i ", i);
		ps = SV_GameClientNum(i);
		Com_Printf("%5i ", ps->persistant[PERS_SCORE]);

		if(cl->state == CS_CONNECTED) {
			Com_Printf("CNCT ");
		} else if(cl->state == CS_ZOMBIE) {
			Com_Printf("ZMBI ");
		} else {
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf("%4i ", ping);
		}

		Com_Printf("%s", cl->name);
		l = 16 - strlen(cl->name);
		for(j = 0; j < l; j++) {
			Com_Printf(" ");
		}

		Com_Printf("%7i ", svs.time - cl->lastPacketTime);

		s = NET_AdrToString(cl->netchan.remoteAddress);
		Com_Printf("%s", s);
		l = 22 - strlen(s);
		for(j = 0; j < l; j++) {
			Com_Printf(" ");
		}

		Com_Printf("%5i", cl->netchan.qport);

		Com_Printf(" %5i", cl->rate);

		Com_Printf("\n");
	}
	Com_Printf("\n");
}

/*
==================
SV_ConSay_f
==================
*/
static void SV_ConSay_f(void) {
	char	*p;
	char    text[1024];

	// make sure server is running
	if(!com_sv_running->integer) {
		Com_Printf("Server is not running.\n");
		return;
	}

	if(Cmd_Argc() < 2) {
		return;
	}

	strcpy(text, "console: ");
	p = Cmd_Args();

	if(*p == '"') {
		p++;
		p[strlen(p) - 1] = 0;
	}

	strcat(text, p);

	SV_SendServerCommand(NULL, "chat \"%s\"", text);
}


/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void SV_Heartbeat_f(void) {
	svs.nextHeartbeatTime = -9999999;
}


/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f(void) {
	Com_Printf("Server info settings:\n");
	Info_Print(Cvar_InfoString(CVAR_SERVERINFO | CVAR_SERVERINFO_NOUPDATE));
}


/*
===========
SV_Systeminfo_f

Examine or change the serverinfo string
===========
*/
static void SV_Systeminfo_f(void) {
	Com_Printf("System info settings:\n");
	Info_Print(Cvar_InfoString(CVAR_SERVERINFO | CVAR_SERVERINFO_NOUPDATE));
}


/*
===========
SV_DumpUser_f

Examine all a users info strings FIXME: move to game
===========
*/
static void SV_DumpUser_f(void) {
	client_t *cl;

	// make sure server is running
	if(!com_sv_running->integer) {
		Com_Printf("Server is not running.\n");
		return;
	}

	if(Cmd_Argc() != 2) {
		Com_Printf("Usage: info <userid>\n");
		return;
	}

	cl = SV_GetPlayerByName();
	if(!cl) {
		return;
	}

	Com_Printf("userinfo\n");
	Com_Printf("--------\n");
	Info_Print(cl->userinfo);
}


/*
=================
SV_KillServer
=================
*/
static void SV_KillServer_f(void) {
	SV_Shutdown("killserver");
}

/*
=================
SV_GameCompleteStatus_f

NERVE - SMF
=================
*/
void SV_GameCompleteStatus_f(void) {
	SV_MasterGameCompleteStatus();
}

//===========================================================

/*
==================
SV_CompleteMapName
==================
*/
static void SV_CompleteMapName( char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteFilename( "maps", "bsp", qtrue );
	}
}

/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands(void) {
	static qboolean initialized;

	if(initialized) {
		return;
	}
	initialized = qtrue;

	Cmd_AddCommand("heartbeat", SV_Heartbeat_f, "^1Send a manual heartbeat to the master servers.");
	Cmd_AddCommand("status", SV_Status_f, "^1Status of currently connected server.");
	Cmd_AddCommand("serverinfo", SV_Serverinfo_f, "^1Gives information about local server from the console of that server.");
	Cmd_AddCommand("systeminfo", SV_Systeminfo_f, "^1Returns values for: g_syncronousclients, sv_serverid, and timescale.");
	Cmd_AddCommand("dumpuser", SV_DumpUser_f, "^1Display user info (handicap, model/color, rail color, more�).");
	Cmd_AddCommand("map_restart", SV_MapRestart_f, "^1Resets the game on the same map.");
	Cmd_AddCommand("fieldinfo", SV_FieldInfo_f, "^1Write in console informations from Entitystate fields and Playerstate fields in order of priority.");
	Cmd_AddCommand("sectorlist", SV_SectorList_f, "^1Use to display a list of sectors and number of entities on the current level.");
	Cmd_AddCommand("map", SV_Map_f, "^1Loads specified map.");
	Cmd_SetCommandCompletionFunc( "map", SV_CompleteMapName );
	Cmd_AddCommand("gameCompleteStatus", SV_GameCompleteStatus_f, "^1Sends gameCompleteStatus messages to specific master servers.");
#ifndef PRE_RELEASE_DEMO_NODEVMAP
	Cmd_AddCommand("devmap", SV_Map_f, "^1Load a devmap with bots spawned.");
	Cmd_SetCommandCompletionFunc( "devmap", SV_CompleteMapName );
	Cmd_AddCommand("spmap", SV_Map_f, "^1Load a level with bots spawned.");
	Cmd_SetCommandCompletionFunc( "devmap", SV_CompleteMapName );
	Cmd_AddCommand("spdevmap", SV_Map_f, "^1Load a devmap with bots spawned.");
	Cmd_SetCommandCompletionFunc( "devmap", SV_CompleteMapName );
#endif
	Cmd_AddCommand("loadgame", SV_LoadGame_f, "^1Use to load a saved game.");
	Cmd_AddCommand("killserver", SV_KillServer_f, "^1Stops server from running and broadcasting heartbeat.");
	if(com_dedicated->integer) {
		Cmd_AddCommand("say", SV_ConSay_f, "^1Say something to everyone on the server.");
	}
	Cmd_AddCommand("rehashrconwhitelist", SV_RehashRconWhitelist_f, "^1Load RCON whitelist from file.");
}
