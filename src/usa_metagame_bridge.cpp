#include "usa_metagame_bridge.h"

#include "lib/framework/frame.h"
#include "lib/framework/input.h"
#include "lib/framework/physfs_ext.h"
#include "lib/ivis_opengl/pieblitfunc.h"
#include "lib/ivis_opengl/screen.h"
#include "lib/widget/widget.h"
#include "lib/widget/form.h"
#include "lib/widget/label.h"
#include "lib/widget/button.h"

#include "hci.h"
#include "intdisplay.h"
#include "multiplay.h"
#include "multiint.h"
#include "frontend.h"
#include "main.h"
#include "titleui/titleui.h"

#include <nlohmann/json.hpp>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "clparse.h"

bool bUsaPrebattle = false;
bool bUsaStripVanillaLobby = (USA_RESTORE_VANILLA_SKIRMISH == 0);

namespace
{
	constexpr UDWORD ID_USA_PANEL = 91000;
	constexpr UDWORD ID_USA_BTN_LAUNCH = 91001;
	constexpr UDWORD ID_USA_BTN_ABORT = 91002;

	bool screenInitialized = false;

	struct BriefingData
	{
		std::string stateName = "UNKNOWN SECTOR";
		std::string factionName = "HOSTILE FORCES";
		std::string mapName = "Sk-Rush";
		int turn = 1;
		int power = 1500;
		std::vector<std::string> techs;
		std::vector<std::string> squad;
	};

	BriefingData loadBriefingJson()
	{
		BriefingData data;
		if (!PHYSFS_exists("tests/usa_briefing.json")) return data;

		PHYSFS_File* file = PHYSFS_openRead("tests/usa_briefing.json");
		if (!file) return data;

		PHYSFS_sint64 len = PHYSFS_fileLength(file);
		std::string rawJson;
		if (len > 0)
		{
			rawJson.resize(static_cast<size_t>(len));
			PHYSFS_readBytes(file, rawJson.data(), static_cast<PHYSFS_uint64>(len));
		}
		PHYSFS_close(file);

		if (rawJson.empty()) return data;

		try
		{
			auto j = nlohmann::json::parse(rawJson);
			data.stateName = j.value("stateName", data.stateName);
			data.factionName = j.value("factionName", data.factionName);
			data.mapName = j.value("mapName", data.mapName);
			data.turn = j.value("turn", 1);
			data.power = j.value("power", 1500);

			if (j.contains("techs") && j["techs"].is_array())
			{
				for (const auto& t : j["techs"]) data.techs.push_back(t.get<std::string>());
			}
			if (j.contains("squad") && j["squad"].is_array())
			{
				for (const auto& s : j["squad"]) data.squad.push_back(s.get<std::string>());
			}
		}
		catch (...)
		{
			debug(LOG_WARNING, "USA Prebattle: Failed to parse tests/usa_briefing.json");
		}
		return data;
	}

	void makeBriefingLabel(const std::shared_ptr<WIDGET>& parent, int x, int y, int w, int h, const std::string& text, WzTextAlignment align = WLAB_ALIGNLEFT, PIELIGHT color = WZCOL_TEXT_BRIGHT)
	{
		auto lbl = std::make_shared<W_LABEL>();
		parent->attach(lbl);
		lbl->setGeometry(x, y, w, h);
		lbl->setString(WzString::fromUtf8(text));
		lbl->setTextAlignment(align);
		lbl->setFont(font_regular, color);
	}

	void makeStyledButton(UDWORD parentFormId, UDWORD id, int x, int y, int w, int h, const std::string& label)
	{
		W_BUTINIT sButInit;
		sButInit.formID = parentFormId;
		sButInit.id = id;
		sButInit.x = static_cast<short>(x);
		sButInit.y = static_cast<short>(y);
		sButInit.width = static_cast<uint16_t>(w);
		sButInit.height = static_cast<uint16_t>(h);
		sButInit.style = WBUT_TXTCENTRE;
		sButInit.FontID = font_medium;
		sButInit.pText = label.c_str();
		sButInit.pDisplay = displayTextOption;
		sButInit.pUserData = new DisplayTextOptionCache();
		sButInit.onDelete = [](WIDGET* psWidget) {
			delete static_cast<DisplayTextOptionCache*>(psWidget->pUserData);
			psWidget->pUserData = nullptr;
			};

		widgAddButton(psWScreen, &sButInit);
	}
}
void startUsaPrebattleScreen()
{
	if (screenInitialized) return;

	widgDelete(psWScreen, FRONTEND_BOTFORM);
	widgDelete(psWScreen, FRONTEND_TOPFORM);

	BriefingData info = loadBriefingJson();
	auto root = psWScreen->psForm;

	const int panelW = 680;
	const int panelH = 440;
	const int panelX = (pie_GetVideoBufferWidth() - panelW) / 2;
	const int panelY = (pie_GetVideoBufferHeight() - panelH) / 2;

	auto panel = std::make_shared<IntFormAnimated>(false);
	root->attach(panel);
	panel->id = ID_USA_PANEL;
	panel->setGeometry(panelX, panelY, panelW, panelH);

	makeBriefingLabel(panel, 24, 18, panelW - 48, 28, "THE PROJECT // TACTICAL DEPLOYMENT BRIEFING", WLAB_ALIGNCENTRE, WZCOL_YELLOW);
	makeBriefingLabel(panel, 24, 58, panelW - 48, 22, "TARGET SECTOR: " + info.stateName + " (" + info.mapName + ")");
	makeBriefingLabel(panel, 24, 84, panelW - 48, 22, "OCCUPYING FACTION: " + info.factionName, WLAB_ALIGNLEFT, WZCOL_RED);
	makeBriefingLabel(panel, 24, 110, panelW - 48, 22, "CAMPAIGN TURN: " + std::to_string(info.turn) + "   |   STRATEGIC POWER: " + std::to_string(info.power));

	makeBriefingLabel(panel, 24, 150, 300, 22, "ACTIVE TECH RESEARCH:", WLAB_ALIGNLEFT, WZCOL_YELLOW);
	if (info.techs.empty())
	{
		makeBriefingLabel(panel, 32, 176, 290, 20, "- Standard Issue Only", WLAB_ALIGNLEFT, WZCOL_TEXT_MEDIUM);
	}
	else
	{
		for (size_t i = 0; i < info.techs.size() && i < 6; ++i)
		{
			makeBriefingLabel(panel, 32, 176 + static_cast<int>(i) * 22, 290, 20, "+ " + info.techs[i]);
		}
	}

	makeBriefingLabel(panel, 350, 150, 300, 22, "STRIKE SQUAD DROP ROSTER:", WLAB_ALIGNLEFT, WZCOL_YELLOW);
	if (info.squad.empty())
	{
		makeBriefingLabel(panel, 358, 176, 290, 20, "- Base HQ Drop Only", WLAB_ALIGNLEFT, WZCOL_TEXT_MEDIUM);
	}
	else
	{
		for (size_t i = 0; i < info.squad.size() && i < 6; ++i)
		{
			makeBriefingLabel(panel, 358, 176 + static_cast<int>(i) * 22, 290, 20, "* " + info.squad[i]);
		}
	}

	makeStyledButton(ID_USA_PANEL, ID_USA_BTN_LAUNCH, 30, panelH - 56, 290, 36, "[ LAUNCH TACTICAL ASSAULT ]");
	makeStyledButton(ID_USA_PANEL, ID_USA_BTN_ABORT, panelW - 320, panelH - 56, 290, 36, "[ ABORT / RETURN TO MAP ]");

	screenInitialized = true;
}

void runUsaPrebattleTick()
{
	if (!screenInitialized)
	{
		startUsaPrebattleScreen();
	}

	WidgetTriggers const& triggers = widgRunScreen(psWScreen);
	unsigned id = triggers.empty() ? 0 : triggers.front().widget->id;

	widgDisplayScreen(psWScreen);

	if (id == ID_USA_BTN_ABORT || keyPressed(KEY_ESC))
	{
		bUsaPrebattle = false;
		bUsaStripVanillaLobby = false;
		screenInitialized = false;
		std::exit(0);
	}

	if (id == ID_USA_BTN_LAUNCH || keyPressed(KEY_RETURN) || keyPressed(KEY_KPENTER))
	{
		bUsaPrebattle = false;
		bUsaStripVanillaLobby = (USA_RESTORE_VANILLA_SKIRMISH == 0);
		screenInitialized = false;
		widgDelete(psWScreen, ID_USA_PANEL);

		setHostLaunch(HostLaunch::Skirmish);
		SPinit(LEVEL_TYPE::SKIRMISH);
		bMultiPlayer = true;
		ingame.side = InGameSide::HOST_OR_SINGLEPLAYER;
		game.type = LEVEL_TYPE::SKIRMISH;
		changeTitleMode(TITLE);
		changeTitleUI(std::make_shared<WzMultiplayerOptionsTitleUI>(wzTitleUICurrent));
	}
}
