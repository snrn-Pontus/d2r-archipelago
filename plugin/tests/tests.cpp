// Unit tests for the parts of the plugin that don't need the game.

#include "ap_client.h"
#include "config.h"
#include "game_data.h"
#include "ids.h"
#include "loot.h"
#include "save_state.h"

#include <cstdio>
#include <filesystem>
#include <iterator>
#include <random>
#include <span>
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
	EXPECT(!ap::ids::IsSkillItem(ap::ids::EmpowermentItem) && !ap::ids::IsSkillItem(ap::ids::LootBase));
	EXPECT(ap::ids::IsLootItem(ap::ids::LootBase + 5) && !ap::ids::IsLootItem(ap::ids::EmpowermentItem));
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

auto FindRecord(std::span<const ap::data::QualityItem> items, uint32_t record) -> const ap::data::QualityItem* {
	for (const auto& item : items) {
		if (item.record == record) {
			return &item;
		}
	}
	return nullptr;
}

void TestLootScalesToLevel() {
	std::mt19937 random(7);
	for (int level : { 1, 15, 30, 45, 60, 75, 90 }) {
		for (int i = 0; i < 50; ++i) {
			const auto weapon = ap::loot::Roll(ap::loot::Loot::UniqueWeapon, level, random);
			const auto* row = FindRecord(ap::data::Uniques, weapon.record);
			EXPECT(weapon.quality == D2RL::Items::Quality::Unique && row != nullptr && row->kind == ap::data::Weapon);
			// In the window, or among the closest few when the window is empty.
			EXPECT(row != nullptr && row->reqLevel <= level + 12);
			EXPECT(weapon.itemLevel >= static_cast<uint32_t>(level));
			const auto set = ap::loot::Roll(ap::loot::Loot::SetItem, level, random);
			EXPECT(set.quality == D2RL::Items::Quality::Set && FindRecord(ap::data::SetItems, set.record) != nullptr);
		}
	}
	// At level 80 nothing should come from the Normal-difficulty tier.
	for (int i = 0; i < 200; ++i) {
		const auto* row = FindRecord(ap::data::Uniques, ap::loot::Roll(ap::loot::Loot::RandomUnique, 80, random).record);
		EXPECT(row != nullptr && row->reqLevel >= 68);
	}
}

void TestOtherLoot() {
	std::mt19937 random(11);
	for (int i = 0; i < 200; ++i) {
		const auto rune = ap::loot::Roll(ap::loot::Loot::HighRune, 50, random);
		const char first = static_cast<char>(rune.code & 0xFF);
		const int number = (static_cast<char>((rune.code >> 8) & 0xFF) - '0') * 10 + (static_cast<char>((rune.code >> 16) & 0xFF) - '0');
		EXPECT(first == 'r' && number >= 21 && number <= 33);
		const auto base = ap::loot::Roll(ap::loot::Loot::SocketedBase, 40, random);
		EXPECT(base.sockets >= 3 && base.sockets <= 6 && base.quality == D2RL::Items::Quality::Normal);
		EXPECT(ap::loot::Roll(ap::loot::Loot::JunkBackpack, 1, random).code != 0);
	}
	EXPECT(ap::loot::Roll(ap::loot::Loot::LowRune, 1, random).code != 0);
	EXPECT(ap::loot::EstimateLevel(0, 0) == 1);
	EXPECT(ap::loot::EstimateLevel(2, 4) == 92);
}

} // namespace

int main() {
	TestLootScalesToLevel();
	TestOtherLoot();
	TestClientParsesServerPackets();
	TestConfigRoundTrip();
	TestConfigDefaults();
	TestSaveStateRoundTrip();
	TestIdsMatchApworld();
	TestGameData();
	std::printf(failures == 0 ? "all tests passed\n" : "%d failure(s)\n", failures);
	return failures == 0 ? 0 : 1;
}
