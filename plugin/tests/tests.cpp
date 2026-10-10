// Unit tests for the parts of the plugin that don't need the game.

#include "ap_client.h"
#include "config.h"
#include "game_data.h"
#include "ids.h"
#include "save_state.h"

#include <cstdio>
#include <filesystem>
#include <iterator>
#include <string_view>

namespace {

int failures = 0;

#define EXPECT(condition)                                                        \
	do {                                                                         \
		if (!(condition)) {                                                      \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);     \
			++failures;                                                          \
		}                                                                        \
	} while (false)

void TestConfigRoundTrip() {
	ap::Config config;
	config.server = "archipelago.gg:38281";
	config.slot = "Snrn \"the\" Sorc";
	config.password = R"(p\w)";
	config.autoConnect = false;
	config.codexQuality = D2RL::Items::Quality::Rare;
	config.oskillProperty = 300;
	const ap::Config parsed = ap::ParseConfig(ap::FormatConfig(config));
	EXPECT(parsed.server == config.server);
	EXPECT(parsed.slot == config.slot);
	EXPECT(parsed.password == config.password);
	EXPECT(!parsed.autoConnect);
	EXPECT(parsed.codexItem == "cm1");
	EXPECT(parsed.codexQuality == D2RL::Items::Quality::Rare);
	EXPECT(parsed.oskillProperty == 300);
}

void TestConfigDefaults() {
	const ap::Config parsed = ap::ParseConfig("[archipelago]\nserver = \"\" # none yet\noskill_property = 265 # oskill\n");
	EXPECT(parsed.server.empty());
	EXPECT(parsed.autoConnect);
	EXPECT(parsed.oskillProperty == 265);
	EXPECT(parsed.codexQuality == D2RL::Items::Quality::Magic);
}

void TestSaveStateRoundTrip() {
	const auto path = std::filesystem::temp_directory_path() / "d2rl-ap-test" / "seed_slot.txt";
	ap::SaveState state;
	state.checks = { ap::ids::QuestLocation(0, 1), ap::ids::AreaLocation(38), ap::ids::LevelLocation(30) };
	state.fillersDelivered = 7;
	state.goalReached = true;
	EXPECT(state.Save(path));
	ap::SaveState loaded;
	EXPECT(loaded.Load(path));
	EXPECT(loaded.checks == state.checks);
	EXPECT(loaded.fillersDelivered == 7);
	EXPECT(loaded.goalReached);
	std::filesystem::remove_all(path.parent_path());
}

// Mirrors apworld/d2r_archipelago/test/test_generation.py.
void TestIdsMatchApworld() {
	EXPECT(ap::ids::QuestLocation(0, 1) == 2'420'001);
	EXPECT(ap::ids::QuestLocation(2, 40) == 2'420'240);
	EXPECT(ap::ids::AreaLocation(38) == 2'421'038);
	EXPECT(ap::ids::LevelLocation(30) == 2'422'030);
	EXPECT(ap::ids::IsSkillItem(2'430'054) && ap::ids::SkillOf(2'430'054) == 54);
	EXPECT(!ap::ids::IsSkillItem(ap::ids::FillerBase));
}

void TestGameData() {
	EXPECT(std::size(ap::data::Skills) == 240);
	EXPECT(std::size(ap::data::Quests) == 27);
	bool teleport = false;
	for (const auto& skill : ap::data::Skills) {
		teleport |= skill.id == 54 && std::string_view(skill.name) == "Teleport";
	}
	EXPECT(teleport);
}

void TestClientParsesServerPackets() {
	ap::Client client([](const std::string&) {});
	client.Handle(R"json([{"cmd":"DataPackage","data":{"games":{"Diablo II Resurrected":{"item_name_to_id":{"Teleport (Sorceress)":2430054},
		"location_name_to_id":{"Normal: Den of Evil":2420001},"checksum":"x"}}}}])json");
	client.Handle(R"json([{"cmd":"Connected","team":0,"slot":1,"players":[{"team":0,"slot":1,"alias":"Snrn","name":"Snrn","class":"NetworkPlayer"}],
		"missing_locations":[2420002],"checked_locations":[2420001],"slot_data":{"goal":2,"goal_quest":40,"area_checks":false,"level_checks":true,
		"level_step":5,"level_cap":90},"slot_info":{"1":{"name":"Snrn","game":"Diablo II Resurrected","type":1,"group_members":[],"class":"NetworkSlot"}},
		"hint_points":0}])json");
	client.Handle(R"json([{"cmd":"ReceivedItems","index":0,"items":[{"item":2430054,"location":-2,"player":1,"flags":1,"class":"NetworkItem"}]}])json");
	EXPECT(client.State() == ap::ConnectionState::Connected);
	EXPECT(client.LocationCount() == 2);
	EXPECT(client.Checked().contains(2420001));
	EXPECT(client.Slots().goal == 2 && !client.Slots().areaChecks && client.Slots().levelCap == 90);
	EXPECT(client.Items().size() == 1 && client.Items()[0].item == 2430054);
	EXPECT(client.ItemName(2430054, 1) == "Teleport (Sorceress)");
	// A gap in the item index must not append anything.
	client.Handle(R"json([{"cmd":"ReceivedItems","index":5,"items":[{"item":2431000,"location":3,"player":1,"flags":0}]}])json");
	EXPECT(client.Items().size() == 1);
}

} // namespace

int main() {
	TestClientParsesServerPackets();
	TestConfigRoundTrip();
	TestConfigDefaults();
	TestSaveStateRoundTrip();
	TestIdsMatchApworld();
	TestGameData();
	std::printf(failures == 0 ? "all tests passed\n" : "%d failure(s)\n", failures);
	return failures == 0 ? 0 : 1;
}
