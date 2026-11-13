#pragma once

#define USA_RESTORE_VANILLA_SKIRMISH 0

extern bool bUsaPrebattle;
extern bool bUsaStripVanillaLobby;
extern bool bUsaSessionActive;

void startUsaPrebattleScreen();
void runUsaPrebattleTick();
void updateUsaInGameHud();
