#pragma once

// Set to 1 to restore the vanilla Warzone 2100 skirmish lobby screen for debugging/consistency.
// Set to 0 to strip out the vanilla skirmish UI and drop straight into the 3D battle.
#define USA_RESTORE_VANILLA_SKIRMISH 0

extern bool bUsaPrebattle;
extern bool bUsaStripVanillaLobby;

void startUsaPrebattleScreen();
void runUsaPrebattleTick();
