/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VARIABLES_H
#define GAME_VARIABLES_H
#undef GAME_VARIABLES_H // this file will be included several times


// client
MACRO_CONFIG_INT(ClPredict, cl_predict, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Predict client movements")
MACRO_CONFIG_INT(ClNameplates, cl_nameplates, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show name plates")
MACRO_CONFIG_INT(ClNameplatesAlways, cl_nameplates_always, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Always show name plates disregarding of distance")
MACRO_CONFIG_INT(ClNameplatesTeamcolors, cl_nameplates_teamcolors, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Use team colors for name plates")
MACRO_CONFIG_INT(ClNameplatesSize, cl_nameplates_size, 50, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Size of the name plates from 0 to 100%")
MACRO_CONFIG_INT(ClAutoswitchWeapons, cl_autoswitch_weapons, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Auto switch weapon on pickup")

MACRO_CONFIG_INT(ClShowhud, cl_showhud, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame HUD")
MACRO_CONFIG_INT(ClShowChatFriends, cl_show_chat_friends, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show only chat messages from friends")
MACRO_CONFIG_INT(ClShowfps, cl_showfps, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame FPS counter")

MACRO_CONFIG_INT(ClAirjumpindicator, cl_airjumpindicator, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClThreadsoundloading, cl_threadsoundloading, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Load sound files threaded")

MACRO_CONFIG_INT(ClWarningTeambalance, cl_warning_teambalance, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Warn about team balance")

MACRO_CONFIG_INT(ClMouseDeadzone, cl_mouse_deadzone, 300, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMouseFollowfactor, cl_mouse_followfactor, 60, 0, 200, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMouseMaxDistance, cl_mouse_max_distance, 800, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(EdShowkeys, ed_showkeys, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

//MACRO_CONFIG_INT(ClFlow, cl_flow, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(ClShowWelcome, cl_show_welcome, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMotdTime, cl_motd_time, 10, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "How long to show the server message of the day")

MACRO_CONFIG_STR(ClVersionServer, cl_version_server, 100, "version.teeworlds.com", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Server to use to check for new versions")

MACRO_CONFIG_STR(ClLanguagefile, cl_languagefile, 255, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "What language file to use")

MACRO_CONFIG_INT(PlayerUseCustomColor, player_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors")
MACRO_CONFIG_INT(PlayerColorBody, player_color_body, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player body color")
MACRO_CONFIG_INT(PlayerColorFeet, player_color_feet, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player feet color")
MACRO_CONFIG_STR(PlayerSkin, player_skin, 24, "default", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin")

MACRO_CONFIG_INT(UiPage, ui_page, 6, 0, 10, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface page")
MACRO_CONFIG_INT(UiToolboxPage, ui_toolbox_page, 0, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toolbox page")
MACRO_CONFIG_STR(UiServerAddress, ui_server_address, 64, "localhost:8303", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface server address")
MACRO_CONFIG_INT(UiScale, ui_scale, 100, 50, 150, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface scale")
MACRO_CONFIG_INT(UiMousesens, ui_mousesens, 100, 5, 100000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Mouse sensitivity for menus/editor")

MACRO_CONFIG_INT(UiColorHue, ui_color_hue, 160, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface color hue")
MACRO_CONFIG_INT(UiColorSat, ui_color_sat, 70, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface color saturation")
MACRO_CONFIG_INT(UiColorLht, ui_color_lht, 175, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface color lightness")
MACRO_CONFIG_INT(UiColorAlpha, ui_color_alpha, 228, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface alpha")

MACRO_CONFIG_INT(GfxNoclip, gfx_noclip, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Disable clipping")

/* race - client */
MACRO_CONFIG_INT(ClAutoRaceRecord, cl_auto_race_record, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Save the best demo of each race")
MACRO_CONFIG_INT(ClDemoName, cl_demo_name, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Save the playername within the demo")
MACRO_CONFIG_INT(ClShowOthers, cl_show_others, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Turn other players off in race")
MACRO_CONFIG_INT(ClRenderSpeedmeter, cl_render_speedmeter, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Render in game speedmeter")
MACRO_CONFIG_INT(ClSpeedmeterAccel, cl_speedmeter_accel, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show acceleration")
MACRO_CONFIG_INT(ClShowCheckpointDiff, cl_show_checkpoint_diff, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show checkpoint diff")
MACRO_CONFIG_INT(ClShowRecords, cl_show_records, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show records")
MACRO_CONFIG_INT(ClShowServerRecord, cl_show_server_record, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show server record")
MACRO_CONFIG_INT(ClRaceGhost, cl_race_ghost, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Enable ghost")
MACRO_CONFIG_INT(ClRaceSaveGhost, cl_race_save_ghost, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Save ghost")
MACRO_CONFIG_INT(ClGhostNamePlates, cl_ghost_nameplates, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ghost nameplates")
MACRO_CONFIG_INT(ClGhostNameplatesAlways, cl_ghost_nameplates_always, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Always show ghost nameplats disregarding of distance")

/* Webapp */
MACRO_CONFIG_STR(WaUsername, wa_username, 32, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Teerace username")
MACRO_CONFIG_STR(WaPassword, wa_password, 32, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Teerace password")
MACRO_CONFIG_STR(WaApiToken, wa_api_token, 25, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Api token for webapp")

// server
MACRO_CONFIG_INT(SvWarmup, sv_warmup, 0, 0, 0, CFGFLAG_SERVER, "Number of seconds to do warmup before round starts")
MACRO_CONFIG_INT(SvUnpauseTimer, sv_unpause_timer, 0, 0, 0, CFGFLAG_SERVER, "Number of seconds till the game continues")
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SERVER, "Message of the day to display for the clients")
MACRO_CONFIG_INT(SvTeamdamage, sv_teamdamage, 0, 0, 1, CFGFLAG_SERVER, "Team damage")
MACRO_CONFIG_STR(SvMaprotation, sv_maprotation, 768, "", CFGFLAG_SERVER, "Maps to rotate between")
MACRO_CONFIG_INT(SvRoundsPerMap, sv_rounds_per_map, 1, 1, 100, CFGFLAG_SERVER, "Number of rounds on each map before rotating")
MACRO_CONFIG_INT(SvRoundSwap, sv_round_swap, 1, 0, 1, CFGFLAG_SERVER, "Swap teams between rounds")
MACRO_CONFIG_INT(SvPowerups, sv_powerups, 1, 0, 1, CFGFLAG_SERVER, "Allow powerups like ninja")
MACRO_CONFIG_INT(SvScorelimit, sv_scorelimit, 0, 0, 1000, CFGFLAG_SERVER, "Score limit (0 disables)")
MACRO_CONFIG_INT(SvTimelimit, sv_timelimit, 0, 0, 1000, CFGFLAG_SERVER, "Time limit in minutes (0 disables)")
MACRO_CONFIG_STR(SvGametype, sv_gametype, 32, "race", CFGFLAG_SERVER, "Game type (dm, tdm, ctf)")
MACRO_CONFIG_INT(SvTournamentMode, sv_tournament_mode, 0, 0, 1, CFGFLAG_SERVER, "Tournament mode. When enabled, players joins the server as spectator")
MACRO_CONFIG_INT(SvSpamprotection, sv_spamprotection, 1, 0, 1, CFGFLAG_SERVER, "Spam protection")

MACRO_CONFIG_INT(SvRespawnDelayTDM, sv_respawn_delay_tdm, 3, 0, 10, CFGFLAG_SERVER, "Time needed to respawn after death in tdm gametype")

MACRO_CONFIG_INT(SvSpectatorSlots, sv_spectator_slots, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Number of slots to reserve for spectators")
MACRO_CONFIG_INT(SvTeambalanceTime, sv_teambalance_time, 0, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before autobalancing teams")
MACRO_CONFIG_INT(SvInactiveKickTime, sv_inactivekick_time, 3, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before taking care of inactive players")
MACRO_CONFIG_INT(SvInactiveKick, sv_inactivekick, 1, 0, 2, CFGFLAG_SERVER, "How to deal with inactive players (0=move to spectator, 1=move to free spectator slot/kick, 2=kick)")

MACRO_CONFIG_INT(SvLoadMapDefaults, sv_load_map_defaults, 1, 0, 1, CFGFLAG_SERVER, "Load map default settings")

MACRO_CONFIG_INT(SvStrictSpectateMode, sv_strict_spectate_mode, 0, 0, 1, CFGFLAG_SERVER, "Restricts information in spectator mode")
MACRO_CONFIG_INT(SvVoteSpectate, sv_vote_spectate, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to move players to spectators")
MACRO_CONFIG_INT(SvVoteSpectateRejoindelay, sv_vote_spectate_rejoindelay, 3, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before a player can rejoin after being moved to spectators by vote")
MACRO_CONFIG_INT(SvVoteKick, sv_vote_kick, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to kick players")
MACRO_CONFIG_INT(SvVoteKickMin, sv_vote_kick_min, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Minimum number of players required to start a kick vote")
MACRO_CONFIG_INT(SvVoteKickBantime, sv_vote_kick_bantime, 5, 0, 1440, CFGFLAG_SERVER, "The time to ban a player if kicked by vote. 0 makes it just use kick")

MACRO_CONFIG_INT(SvReservedSlots, sv_reserved_slots, 0, 0, 12, CFGFLAG_SERVER, "Number of reserved slots")
MACRO_CONFIG_STR(SvReservedSlotsPass, sv_reserved_slots_pass, 32, "", CFGFLAG_SERVER, "Password for reserved slots")

/* race - server*/
MACRO_CONFIG_INT(SvRegen, sv_regen, 0, 0, 50, CFGFLAG_SERVER, "Set regeneration")
MACRO_CONFIG_INT(SvStrip, sv_strip, 0, 0, 1, CFGFLAG_SERVER, "Enable or disable keeping weapon after teleporting")
MACRO_CONFIG_INT(SvInfiniteAmmo, sv_infinite_ammo, 0, 0, 1, CFGFLAG_SERVER, "Enable or disable infinite ammo")
MACRO_CONFIG_INT(SvNoItems, sv_no_items, 0, 0, 1, CFGFLAG_SERVER, "removes any items from the map if there are any")
MACRO_CONFIG_INT(SvTeleportGrenade, sv_teleport_grenade, 0, 0, 1, CFGFLAG_SERVER, "Enable or disable teleport of grenade")
MACRO_CONFIG_INT(SvTeleportVelReset, sv_teleport_vel_reset, 0, 0, 1, CFGFLAG_SERVER, "Reset velocity after teleport")
MACRO_CONFIG_INT(SvDeleteGrenadesAfterDeath, sv_delete_grenades_after_death, 1, 0, 1, CFGFLAG_SERVER, "Delete grenades after the player dies")
MACRO_CONFIG_INT(SvRocketJumpDamage, sv_rocket_jump_damage, 1, 0, 1, CFGFLAG_SERVER, "Enable or disable rocket jump damage")
MACRO_CONFIG_INT(SvPickupRespawn, sv_pickup_respawn, -1, -1, 120, CFGFLAG_SERVER, "Time before a pickup respawn")
MACRO_CONFIG_INT(SvScoreIP, sv_score_ip, 1, 0, 1, CFGFLAG_SERVER, "Check score for ip, too")
MACRO_CONFIG_INT(SvCheckpointSave, sv_checkpoint_save, 1, 0, 1, CFGFLAG_SERVER, "Save checkpoint times to score file")
MACRO_CONFIG_INT(SvShowBest, sv_load_best, 1, 0, 1, CFGFLAG_SERVER, "Loads the best time from scoring")

MACRO_CONFIG_INT(SvShowTimes, sv_show_times, 1, 0, 1, CFGFLAG_SERVER, "Shows the times of other players")
MACRO_CONFIG_INT(SvShowOthers, sv_show_others, 1, 0, 1, CFGFLAG_SERVER, "Shows the other players")

MACRO_CONFIG_STR(SvScore, sv_score, 32, "file", CFGFLAG_SERVER, "Scoring (file, web, mysql)")

/* SQL */
#if defined(CONF_SQL)
MACRO_CONFIG_STR(SvSqlUser, sv_sql_user, 32, "nameless", CFGFLAG_SERVER, "SQL User")
MACRO_CONFIG_STR(SvSqlPw, sv_sql_pw, 32, "tee", CFGFLAG_SERVER, "SQL Password")
MACRO_CONFIG_STR(SvSqlIp, sv_sql_ip, 32, "127.0.0.1", CFGFLAG_SERVER, "SQL Database IP")
MACRO_CONFIG_INT(SvSqlPort, sv_sql_port, 3306, 0, 65535, CFGFLAG_SERVER, "SQL Database port")
MACRO_CONFIG_STR(SvSqlDatabase, sv_sql_database, 16, "teeworlds", CFGFLAG_SERVER, "SQL Database name")
MACRO_CONFIG_STR(SvSqlPrefix, sv_sql_prefix, 16, "record", CFGFLAG_SERVER, "SQL Database table prefix")
#endif

/* Webapp */
MACRO_CONFIG_STR(WaWebappIp, wa_webapp_ip, 32, "race.teesites.net", CFGFLAG_CLIENT|CFGFLAG_SERVER, "Webapp IP")
MACRO_CONFIG_STR(WaApiPath, wa_api_path, 32, "/api/1", CFGFLAG_CLIENT|CFGFLAG_SERVER, "initial path to api")

#if defined(CONF_TEERACE)

MACRO_CONFIG_STR(WaApiKey, wa_api_key, 33, "", CFGFLAG_SERVER, "api key to register the server on the website")

MACRO_CONFIG_INT(WaAutoRecord, wa_auto_record, 1, 0, 1, CFGFLAG_SERVER, "Enables auto recording")
MACRO_CONFIG_INT(WaAutoAddMaps, wa_auto_add_maps, 0, 0, 1, CFGFLAG_SERVER, "Enables auto adding maps")

MACRO_CONFIG_STR(WaVoteDescription, wa_vote_description, 64, "change map to %s", CFGFLAG_SERVER, "Vote description for update map vote command")

MACRO_CONFIG_STR(WaMapTypes, wa_map_types, 128, "race", CFGFLAG_SERVER, "The type of maps to be fetched from teerace (e.g. 'race long')")
#endif

// debug
#ifdef CONF_DEBUG // this one can crash the server if not used correctly
	MACRO_CONFIG_INT(DbgDummies, dbg_dummies, 0, 0, 15, CFGFLAG_SERVER, "")
#endif

MACRO_CONFIG_INT(DbgFocus, dbg_focus, 0, 0, 1, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(DbgTuning, dbg_tuning, 0, 0, 1, CFGFLAG_CLIENT, "")
#endif
