// Unit tests for the parts of the plugin that don't need the game.

#include "ap_client.h"
#include "codex.h"
#include "config.h"
#include "game_data.h"
#include "ids.h"
#include "loot.h"
#include "quest_triggers.h"
#include "randomizer.h"
#include "table.h"
#include "save_state.h"

#include <cstdio>
#include <filesystem>
#include <algorithm>
#include <iterator>
#include <map>
#include <set>
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

void TestCodexLevels() {
	using ap::NetworkItem;
	const std::vector<NetworkItem> items {
		{ ap::ids::ItemBase + 54, 0, 1, 0 },     // Teleport
		{ ap::ids::ItemBase + 67, 0, 1, 0 },     // Teeth
		{ ap::ids::ItemBase + 54, 0, 1, 0 },     // Teleport again
		{ ap::ids::EmpowermentItem, 0, 1, 0 },
		{ ap::ids::LootBase + 1, 0, 1, 0 },      // a Perfect Gem, ignored
	};
	// Level 13 with +1 per 3 levels: 4 from scaling, 1 empowerment, plus copies.
	const auto entries = ap::codex::Entries(items, 13, 3);
	EXPECT(entries.size() == 2);
	EXPECT((entries[0] == ap::codex::Entry { 54, 2 + 1 + 4 }));
	EXPECT((entries[1] == ap::codex::Entry { 67, 1 + 1 + 4 }));
	// No scaling, and the fingerprint follows the levels.
	const auto flat = ap::codex::Entries(items, 13, 0);
	EXPECT(flat[0].level == 3 && flat[1].level == 2);
	EXPECT(ap::codex::Fingerprint(entries, 0, 2, 4) != ap::codex::Fingerprint(flat, 0, 2, 4));
	EXPECT(ap::codex::Fingerprint(entries, 0, 2, 4) == ap::codex::Fingerprint(ap::codex::Entries(items, 14, 3), 0, 2, 4));
	EXPECT(ap::codex::Fingerprint(entries, 0, 2, 4) != ap::codex::Fingerprint(entries, 0, 2, 3));
	// Scaling stops at its cap: level 60 at +1 per 5 levels would be +12.
	EXPECT(ap::codex::Entries(items, 60, 5, 10)[1].level == 1 + 1 + 10);
	EXPECT(ap::codex::Entries(items, 60, 5, 0)[1].level == 1 + 1 + 12);

	// Your class's skills get their own charm; other classes' go on charms from OtherClassChunk.
	const std::vector<ap::codex::Entry> mixed = { { 54, 3 }, { 126, 2 }, { 67, 1 }, { 132, 1 } };
	const auto chunks = ap::codex::Chunks(mixed, "Barbarian", 60);
	EXPECT(chunks.size() == ap::codex::ChunkCount);
	EXPECT((chunks[0] == std::vector<ap::codex::Entry> { { 126, 2 }, { 132, 1 } }));
	EXPECT((chunks[ap::codex::OtherClassChunk] == std::vector<ap::codex::Entry> { { 54, 3 }, { 67, 1 } }));
	EXPECT(chunks[1].empty() && chunks[ap::codex::OtherClassChunk + 1].empty());
	// Full chunks spill into the next; an unknown class puts everything with the other classes.
	const auto small = ap::codex::Chunks(mixed, "Barbarian", 1);
	EXPECT(small[0].size() == 1 && small[1].size() == 1 && small[ap::codex::OtherClassChunk + 1].size() == 1);
	const auto unknown = ap::codex::Chunks(mixed, "", 60);
	EXPECT(unknown[0].empty() && unknown[ap::codex::OtherClassChunk].size() == 4);
}

void TestQuestTriggers() {
	using ap::quests::Find;
	using ap::quests::Trigger;
	EXPECT(Find(Trigger::Kill, 156) == 6);   // Andariel
	EXPECT(Find(Trigger::Kill, 544) == 40);  // Baal
	EXPECT(Find(Trigger::Item, ap::quests::Code("hst")) == 10);
	EXPECT(Find(Trigger::Area, 38) == 4);    // Tristram
	EXPECT(!Find(Trigger::Kill, 1).has_value());
	// Every trigger points at a known quest, and every quest but Den of Evil has one.
	std::set<uint8_t> covered { 1 };
	for (const auto& trigger : ap::quests::Triggers) {
		bool known = false;
		for (const auto& quest : ap::data::Quests) {
			known |= quest.flag == trigger.flag;
		}
		EXPECT(known);
		covered.insert(trigger.flag);
	}
	EXPECT(covered.size() == std::size(ap::data::Quests));
}

void TestConfigRoundTrip() {
	ap::Config config;
	config.server = "archipelago.gg:38281";
	config.slot = "Snrn \"the\" Sorc";
	config.password = R"(p\w)";
	config.autoConnect = false;
	config.codexUnique = 500;
	config.oskillProperty = 300;
	const ap::Config parsed = ap::ParseConfig(ap::FormatConfig(config));
	EXPECT(parsed.server == config.server);
	EXPECT(parsed.slot == config.slot);
	EXPECT(parsed.password == config.password);
	EXPECT(!parsed.autoConnect);
	EXPECT(parsed.codexItem == "cm1");
	EXPECT(parsed.codexUnique == 500);
	EXPECT(parsed.oskillProperty == 300);
}

void TestConfigDefaults() {
	const ap::Config parsed = ap::ParseConfig("[archipelago]\nserver = \"\" # none yet\noskill_property = 265 # oskill\n");
	EXPECT(parsed.server.empty());
	EXPECT(parsed.autoConnect);
	EXPECT(parsed.oskillProperty == 265);
	EXPECT(parsed.codexUnique == ap::data::CodexUnique);
	EXPECT(parsed.codexPrefix == ap::data::CodexPrefix);
}

void TestSaveStateRoundTrip() {
	const auto path = std::filesystem::temp_directory_path() / "d2rl-ap-test" / "seed_slot.txt";
	ap::SaveState state;
	state.checks = { ap::ids::QuestLocation(0, 1), ap::ids::AreaLocation(0, 38), ap::ids::LevelLocation(30) };
	state.fillersDelivered = 7;
	state.goalReached = true;
	state.playerClass = 4;
	state.kills = { { 3, 120 }, { 24, 7 } };
	state.foundUniques = { 11, 4000000000U };
	state.foundSets = { 5 };
	state.ownHints = { 0, 3 };
	state.otherHints = { 2425500 };
	state.prophecies = 2;
	state.ledgerGiven = true;
	EXPECT(state.Save(path));
	ap::SaveState loaded;
	EXPECT(loaded.Load(path));
	EXPECT(loaded.checks == state.checks);
	EXPECT(loaded.fillersDelivered == 7);
	EXPECT(loaded.goalReached);
	EXPECT(loaded.playerClass == 4);
	EXPECT(loaded.kills == state.kills && loaded.foundUniques == state.foundUniques && loaded.foundSets == state.foundSets);
	EXPECT(loaded.ownHints == state.ownHints && loaded.otherHints == state.otherHints && loaded.prophecies == 2 && loaded.ledgerGiven);
	std::filesystem::remove_all(path.parent_path());
}

// Mirrors apworld/d2r_archipelago/test/test_generation.py.
void TestIdsMatchApworld() {
	EXPECT(ap::ids::QuestLocation(0, 1) == 2'420'001);
	EXPECT(ap::ids::QuestLocation(2, 40) == 2'420'240);
	EXPECT(ap::ids::AreaLocation(0, 38) == 2'421'038);
	EXPECT(ap::ids::AreaLocation(2, 132) == 2'421'532);
	EXPECT(ap::ids::LevelLocation(30) == 2'422'030);
	EXPECT(ap::ids::SuperUniqueLocation(0, 0) == 2'423'000);
	EXPECT(ap::ids::SuperUniqueLocation(2, 59) == 2'423'259);
	EXPECT(ap::ids::RuneLocation(33) == 2'425'033);
	EXPECT(ap::ids::GemLocation(34) == 2'425'134);
	EXPECT(ap::ids::SetLocation(31) == 2'425'231);
	EXPECT(ap::ids::SpecialLocation(12) == 2'425'312);
	EXPECT(ap::ids::CowLocation(1, 2) == 2'425'412);
	EXPECT(ap::ids::ShopLocation(4, 4) == 2'425'544);
	EXPECT(ap::ids::UberLocation(5) == 2'425'605);
	EXPECT(ap::ids::UniqueFindLocation(4) == 2'425'804 && ap::ids::SetFindLocation(3) == 2'425'813);
	EXPECT(ap::ids::QuestItemLocation(2, 14) == 2'425'954);
	EXPECT(ap::ids::KillLocation(2, 4, 2) == 2'426'242);
	EXPECT(ap::ids::IsLockItem(ap::ids::LockBase + 9) && !ap::ids::IsLockItem(ap::ids::LootBase) && !ap::ids::IsKeyItem(ap::ids::LockBase));
	EXPECT(static_cast<uint32_t>(ap::loot::Loot::Count) == 24);
	EXPECT(ap::ids::IsSkillItem(2'430'054) && ap::ids::SkillOf(2'430'054) == 54);
	EXPECT(!ap::ids::IsSkillItem(ap::ids::EmpowermentItem) && !ap::ids::IsSkillItem(ap::ids::LootBase));
	EXPECT(ap::ids::IsLootItem(ap::ids::LootBase + 5) && !ap::ids::IsLootItem(ap::ids::EmpowermentItem));
}

void TestGameData() {
	EXPECT(std::size(ap::data::Skills) == 240);
	EXPECT(std::size(ap::data::Quests) == 27);
	EXPECT(std::size(ap::data::SuperUniques) == 41);
	EXPECT(std::size(ap::data::Gems) == 35 && std::size(ap::data::Sets) == 32 && std::size(ap::data::Specials) == 13);
	EXPECT(std::string_view(ap::data::Sets[0].pieces[0]) == "Civerb's Ward" && ap::data::Sets[0].pieces[3] == nullptr);
	EXPECT(ap::data::Experience[0] == 0 && ap::data::Experience[1] == 500 && std::size(ap::data::Experience) == 99);
	EXPECT(std::size(ap::data::QuestItems) == 15 && std::size(ap::data::Ubers) == 6);
	EXPECT(ap::data::LevelActs[1] == 0 && ap::data::LevelActs[40] == 1 && ap::data::LevelActs[75] == 2 && ap::data::LevelActs[103] == 3
	       && ap::data::LevelActs[109] == 4);
	for (const auto& superUnique : ap::data::SuperUniques) {
		EXPECT(superUnique.id < 100 && superUnique.act < 5);
	}
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
	EXPECT(client.Missing() == std::vector<int64_t> { 2420002 });
	client.Handle(R"json([{"cmd":"RoomUpdate","checked_locations":[2420002]}])json");
	EXPECT(client.Missing().empty() && client.LocationCount() == 2);
	EXPECT(client.Slots().goal == 2 && !client.Slots().areaChecks && client.Slots().levelCap == 90);
	EXPECT(client.Items().size() == 1 && client.Items()[0].item == 2430054);
	EXPECT(client.ItemName(2430054, 1) == "Teleport (Sorceress)");
	// A gap in the item index must not append anything.
	client.Handle(R"json([{"cmd":"ReceivedItems","index":5,"items":[{"item":2431000,"location":3,"player":1,"flags":0}]}])json");
	EXPECT(client.Items().size() == 1);
	EXPECT(!client.Slots().deathLink && !client.Slots().superUniqueChecks && !client.Slots().locks.cube);
	// DeathLink is off for this seed, so deaths are ignored.
	client.Handle(R"json([{"cmd":"Bounced","tags":["DeathLink"],"data":{"time":1.0,"source":"Other","cause":"Other fell."}}])json");
	EXPECT(client.TakeDeaths().empty());
}

void TestClientDeathLink() {
	ap::Client client([](const std::string&) {});
	client.Connect("localhost", "Snrn", "");
	client.Disconnect();
	client.Handle(R"json([{"cmd":"Connected","team":0,"slot":1,"players":[],"missing_locations":[],"checked_locations":[],
		"slot_data":{"death_link":true,"superunique_checks":true,"shop_checks":3,"enforce_act_keys":true,
		"hints":[[77,2,2430951],[2420001,1,2430054]],"hint_balance":30,"hint_sources":{"cain":true,"page_chance":25,"prophecy_shop":true},
		"locks":{"cube":true,"waypoints":true}},"slot_info":{}}])json");
	EXPECT(client.Slots().hints.size() == 2 && client.Slots().hints[0].location == 77 && client.Slots().hints[0].player == 2
	       && client.Slots().hints[1].item == 2430054 && client.Slots().hintBalance == 30 && client.Slots().cainHints
	       && client.Slots().pageHintChance == 25 && client.Slots().prophecyShop);
	client.Handle(R"json([{"cmd":"LocationInfo","locations":[{"item":99,"location":2420002,"player":3,"flags":1,"class":"NetworkItem"}]}])json");
	EXPECT(client.Scouted().size() == 1 && client.Scouted().at(2420002).player == 3 && client.Scouted().at(2420002).flags == 1);
	EXPECT(client.Slots().shopChecks == 3 && client.Slots().enforceActKeys && client.Slots().locks.cube && client.Slots().locks.waypoints
	       && !client.Slots().locks.stash);
	EXPECT(client.Slots().deathLink && client.Slots().superUniqueChecks && client.Slots().randomizer.superUniqueChecks);
	client.Handle(R"json([{"cmd":"Bounced","tags":["DeathLink"],"data":{"time":1.0,"source":"Other","cause":"Other was slain by a cow."}}])json");
	// Our own death comes back from the server too; it's not a new one.
	client.Handle(R"json([{"cmd":"Bounced","tags":["DeathLink"],"data":{"time":2.0,"source":"Snrn","cause":"Snrn died."}}])json");
	client.Handle(R"json([{"cmd":"Bounced","tags":["Other"],"data":{"source":"Other"}}])json");
	client.Handle(R"json([{"cmd":"Bounced","tags":["DeathLink"],"data":{"time":3.0,"source":"Quiet"}}])json");
	const auto deaths = client.TakeDeaths();
	EXPECT(deaths.size() == 2 && deaths[0] == "Other was slain by a cow." && deaths[1] == "Quiet died.");
	EXPECT(client.TakeDeaths().empty());
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

void TestGoldExperienceCharms() {
	EXPECT(ap::loot::GoldAmount(1) == 1000 && ap::loot::GoldAmount(80) == 80000);
	// Level 1 needs 500 experience to level up; 15% of that is 75.
	EXPECT(ap::loot::ExperienceAmount(1) == 75);
	EXPECT(ap::loot::ExperienceAmount(99) == 0);
	EXPECT(ap::loot::ExperienceAmount(90) > 0);
	std::mt19937 random(11);
	std::set<std::string> charms;
	for (int i = 0; i < 60; ++i) {
		const auto charm = ap::loot::Roll(ap::loot::Loot::RandomCharm, 40, random);
		EXPECT(charm.quality == D2RL::Items::Quality::Magic && charm.itemLevel == 40);
		charms.insert(charm.name);
		const auto jewel = ap::loot::Roll(ap::loot::Loot::RandomJewel, 40, random);
		EXPECT(jewel.code == ap::loot::CodeFromText("jew"));
		EXPECT(jewel.quality == D2RL::Items::Quality::Magic || jewel.quality == D2RL::Items::Quality::Rare);
	}
	EXPECT(charms.size() == 3);
}

void TestShrinesAndTraps() {
	std::mt19937 random(13);
	const auto shrine = ap::loot::CommandsFor(ap::loot::Loot::RandomShrine, 30, 0, random);
	EXPECT(!shrine.trap && shrine.commands.front() == "loc player" && shrine.commands.back().rfind("shrine ", 0) == 0);
	const auto ambush = ap::loot::CommandsFor(ap::loot::Loot::AmbushTrap, 30, 4, random);
	EXPECT(ambush.trap && ambush.commands.back().rfind("mspawn ", 0) == 0 && ambush.commands.back().ends_with(",6"));
	const auto tax = ap::loot::CommandsFor(ap::loot::Loot::TaxCollectorTrap, 30, 0, random);
	EXPECT(tax.trap && tax.commands[0] == "gold -30000");
	EXPECT(ap::loot::CommandsFor(ap::loot::Loot::Gold, 30, 0, random).commands.empty());
	for (int i = 0; i < 50; ++i) {
		const int id = ap::loot::ShrineId(random);
		// Never a harmful or unused shrine.
		EXPECT(id != 0 && id != 4 && id != 5 && id != 16 && id != 17 && id < 19);
	}
	EXPECT(std::string_view(ap::loot::AmbushMonster(9, random)).size() > 0);
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

void TestTableRoundTrip() {
	for (const std::string text : { "a\tb\r\n1\t2\r\n\t\r\n", "a\tb\n1\t2", "Name\tId\nEl\t0\n" }) {
		const auto table = ap::Table::Parse(text);
		EXPECT(table.Format() == text);
	}
	auto table = ap::Table::Parse("a\tb\r\n1\r\n");
	EXPECT(table.Column("b") == 1 && table.Cell(0, 1).empty());
	table.Set(0, 1, "x");
	EXPECT(table.Format() == "a\tb\r\n1\tx\r\n");
}

auto MakeLevels() -> ap::Table {
	// Two acts with three monster areas each, a town, and the Cow Level.
	std::string text = "Name\tId\tAct\tmon1\tmon2\tnmon1\tumon1\r\n";
	text += "Town\t1\t0\t\t\t\t\r\n";
	text += "A\t2\t0\tzombie1\tfallen1\tzombie4\tfallen1\r\n";
	text += "B\t3\t0\tquill1\tfallen2\tquill4\tquill1\r\n";
	text += "C\t4\t0\tskeleton1\t\tskeleton4\tskeleton1\r\n";
	text += "D\t41\t1\tsandraider1\t\tsandraider4\t\r\n";
	text += "E\t42\t1\tvulture1\t\tvulture4\t\r\n";
	text += "F\t43\t1\tscarab1\tmummy1\tscarab4\t\r\n";
	text += "Cows\t39\t0\thellbovine\t\thellbovine\t\r\n";
	return ap::Table::Parse(text);
}

void TestMonsterShuffle() {
	auto lineup = [](const ap::Table& t, size_t row) { return t.Cell(row, 3) + "|" + t.Cell(row, 4) + "|" + t.Cell(row, 5) + "|" + t.Cell(row, 6); };
	const auto original = MakeLevels();
	std::mt19937 random(1);
	auto nearby = MakeLevels();
	std::multiset<std::string> act0, act1, before0, before1;
	const ap::Table noFamilies = ap::Table::Parse("Id\tBaseId\tLevel\r\n");
	ap::randomizer::ShuffleMonsters(nearby, noFamilies, ap::randomizer::Shuffle::Nearby, random);
	for (size_t row = 1; row <= 3; ++row) {
		act0.insert(lineup(nearby, row));
		before0.insert(lineup(original, row));
	}
	for (size_t row = 4; row <= 6; ++row) {
		act1.insert(lineup(nearby, row));
		before1.insert(lineup(original, row));
	}
	// Whole line-ups stay together and stay in their act.
	EXPECT(act0 == before0 && act1 == before1);
	EXPECT(lineup(nearby, 0) == lineup(original, 0) && lineup(nearby, 7) == lineup(original, 7));

	// Anywhere eventually mixes acts; the same seed gives the same result.
	bool mixed = false;
	for (uint32_t seed = 0; seed < 20 && !mixed; ++seed) {
		auto anywhere = MakeLevels();
		std::mt19937 r(seed);
		ap::randomizer::ShuffleMonsters(anywhere, noFamilies, ap::randomizer::Shuffle::Anywhere, r);
		// Area A (act I) ended up with an act II line-up.
		const std::string first = anywhere.Cell(1, 3);
		mixed = first == "sandraider1" || first == "vulture1" || first == "scarab1";
	}
	EXPECT(mixed);
	auto a = MakeLevels(), b = MakeLevels();
	std::mt19937 ra(5), rb(5);
	ap::randomizer::ShuffleMonsters(a, noFamilies, ap::randomizer::Shuffle::Anywhere, ra);
	ap::randomizer::ShuffleMonsters(b, noFamilies, ap::randomizer::Shuffle::Anywhere, rb);
	EXPECT(a.Format() == b.Format());
}

void TestBalancedAnywhere() {
	// A level 2 area and a level 30 area trade line-ups; each monster becomes
	// its family's variant nearest the new area's level.
	auto levels = ap::Table::Parse("Name\tId\tAct\tMonLvlEx\tmon1\tnmon1\tumon1\r\n"
	                               "Low\t2\t0\t2\tzombie1\tzombie4\tzombie1\r\nHigh\t110\t4\t30\tskeleton5\tskeleton5\tskeleton5\r\n");
	const auto monstats = ap::Table::Parse("Id\tBaseId\tLevel\r\nzombie1\tzombie1\t1\r\nzombie2\tzombie1\t2\r\nzombie3\tzombie1\t12\r\nzombie4\tzombie1\t22\r\n"
	                                       "skeleton1\tskeleton1\t1\r\nskeleton2\tskeleton1\t4\r\nskeleton5\tskeleton1\t33\r\n");
	for (uint32_t seed = 0; seed < 20; ++seed) {
		auto copy = levels;
		std::mt19937 random(seed);
		ap::randomizer::ShuffleMonsters(copy, monstats, ap::randomizer::Shuffle::Anywhere, random);
		if (copy.Cell(0, 4).rfind("skeleton", 0) == 0) {
			// Swapped: the low area gets weak skeletons, the high area strong zombies.
			EXPECT(copy.Cell(0, 4) == "skeleton1" && copy.Cell(0, 6) == "skeleton1");
			EXPECT(copy.Cell(1, 4) == "zombie4");
			// Nightmare/Hell columns keep the original monsters.
			EXPECT(copy.Cell(0, 5) == "skeleton5");
			return;
		}
	}
	EXPECT(false);
}

void TestDensityAndDrops() {
	auto levels = ap::Table::Parse("Name\tMonDen\tMonDen(N)\tMonUMin\tMonUMax\r\nTown\t0\t\t\t\r\nMoor\t800\t1500\t1\t3\r\n");
	ap::randomizer::ScaleDensity(levels, 200, 300);
	EXPECT(levels.Cell(0, 1) == "0" && levels.Cell(1, 1) == "1600" && levels.Cell(1, 2) == "3000");
	EXPECT(levels.Cell(1, 3) == "3" && levels.Cell(1, 4) == "9");
	auto ratio = ap::Table::Parse("Function\tUnique\tUniqueDivisor\tUniqueMin\tRare\tSet\r\nNormal\t400\t2\t6400\t160\t125\r\n");
	ap::randomizer::BoostDrops(ratio, 200);
	EXPECT(ratio.Cell(0, 1) == "200" && ratio.Cell(0, 2) == "2" && ratio.Cell(0, 3) == "3200" && ratio.Cell(0, 4) == "80");
}

void TestSuperUniqueShuffle() {
	auto superUniques = ap::Table::Parse("Superunique\tName\tClass\r\nBishibosh\tBishibosh\tfallenshaman1\r\nBonebreak\tBonebreak\tskeleton1\r\n"
	                                     "Radament\tRadament\tradament\r\nPindleskin\tPindleskin\treanimatedhorde5\r\nThe Summoner\tThe Summoner\tsummoner\r\n");
	const auto monstats = ap::Table::Parse("Id\tLevel\r\nfallenshaman1\t2\r\nskeleton1\t1\r\nreanimatedhorde5\t60\r\nradament\t16\r\nsummoner\t18\r\n");
	std::mt19937 random(3);
	ap::randomizer::ShuffleSuperUniques(superUniques, monstats, ap::randomizer::Shuffle::Anywhere, random);
	EXPECT(superUniques.Cell(2, 2) == "radament" && superUniques.Cell(4, 2) == "summoner");
	std::multiset<std::string> classes { superUniques.Cell(0, 2), superUniques.Cell(1, 2), superUniques.Cell(3, 2) };
	EXPECT((classes == std::multiset<std::string> { "fallenshaman1", "skeleton1", "reanimatedhorde5" }));
}

void TestRunewordShuffle() {
	auto runes = ap::Table::Parse("Name\t*Rune Name\tcomplete\tRune1\tRune2\tRune3\tRune4\tRune5\tRune6\r\n"
	                              "Runeword1\tSpirit\t1\tr07\tr10\tr11\tr09\t\t\r\nRuneword2\tOff\t0\tr01\t\t\t\t\t\r\nRuneword3\tZod\t1\tr33\t\t\t\t\t\r\n");
	std::mt19937 random(9);
	const auto recipes = ap::randomizer::ShuffleRunewords(runes, random);
	EXPECT(recipes.size() == 2 && recipes[0].rfind("Spirit:", 0) == 0);
	const int original[] = { 7, 10, 11, 9 };
	for (int i = 0; i < 4; ++i) {
		const int tier = std::stoi(runes.Cell(0, 3 + i).substr(1));
		EXPECT(tier >= original[i] - 2 && tier <= original[i] + 2);
	}
	EXPECT(runes.Cell(1, 3) == "r01");
	EXPECT(std::stoi(runes.Cell(2, 3).substr(1)) >= 31);
}

void TestSuperUniqueMonsters() {
	auto monstats = ap::Table::Parse("Id\t*hcIdx\tminion1\tLevel\r\nfallenshaman1\t0\tfallen1\t2\r\nzombie1\t1\t\t1\r\nExpansion\t\t\t\r\n"
	                                 "snowyeti4\t2\t\t40\r\n");
	auto superUniques = ap::Table::Parse("Superunique\tName\tClass\r\nBishibosh\tBishibosh\tfallenshaman1\r\nRadament\tRadament\tradament\r\n"
	                                     "Corpsefire\tCorpsefire\tzombie1\r\n");
	const size_t before = monstats.Rows().size();
	EXPECT(ap::randomizer::AddSuperUniqueMonsters(monstats, superUniques) == 2);
	// One row per field superunique, in data::SuperUniques order, at the end.
	EXPECT(monstats.Rows().size() == before + std::size(ap::data::SuperUniques));
	auto rowOf = [&](std::string_view name) {
		for (size_t i = 0; i < std::size(ap::data::SuperUniques); ++i) {
			if (name == ap::data::SuperUniques[i].name) {
				return before + i;
			}
		}
		return size_t { 0 };
	};
	const size_t bishibosh = rowOf("Bishibosh"), corpsefire = rowOf("Corpsefire");
	EXPECT(monstats.Cell(bishibosh, 0) == "ap_superunique_0" && monstats.Cell(bishibosh, 1) == "3");
	// Bishibosh's minions were fallen already; Corpsefire's stay plain zombies.
	EXPECT(monstats.Cell(bishibosh, 2) == "fallen1" && monstats.Cell(bishibosh, 3) == "2");
	EXPECT(monstats.Cell(corpsefire, 2) == "zombie1" && monstats.Cell(corpsefire, 3) == "1");
	EXPECT(superUniques.Cell(0, 2) == "ap_superunique_0" && superUniques.Cell(1, 2) == "radament" && superUniques.Cell(2, 2) == "ap_superunique_40");
	// Superuniques this table lacks still get a row, a copy of the first monster.
	EXPECT(monstats.Cell(rowOf("Frozenstein"), 0) == "ap_superunique_59" && monstats.Cell(rowOf("Frozenstein"), 3) == "2");

	ap::randomizer::Settings settings;
	settings.superUniqueChecks = true;
	EXPECT(settings.Any() && settings.Stamp().find("superunique_checks=1") != std::string::npos);
	EXPECT(ap::randomizer::Settings {}.Stamp().find("superunique_checks") == std::string::npos);
}

void TestSkillTreeShuffle() {
	auto skills = ap::Table::Parse("skill\tcharclass\tskilldesc\trestrict\r\nAttack\t\tattack\t\r\nFire Bolt\tsor\tfire bolt\t\r\nWarmth\tsor\twarmth\t\r\n"
	                               "Inferno\tsor\tinferno\t\r\nBash\tbar\tbash\t\r\nHowl\tbar\thowl\t\r\nLeap\tbar\tleap\t\r\n"
	                               "Wearwolf\tdru\twearwolf\t1\r\nFury\tdru\tfury\t2\r\n");
	auto desc = ap::Table::Parse("skilldesc\tSkillPage\tSkillRow\tSkillColumn\tListRow\r\nattack\t0\t0\t0\t0\r\nfire bolt\t1\t1\t2\t1\r\n"
	                             "warmth\t1\t1\t3\t1\r\ninferno\t1\t2\t1\t1\r\nbash\t1\t1\t2\t1\r\nhowl\t3\t1\t1\t3\r\nleap\t1\t2\t1\t1\r\n"
	                             "wearwolf\t2\t1\t1\t2\r\nfury\t2\t2\t1\t2\r\n");
	bool mixed = false;
	for (uint32_t seed = 1; seed < 20; ++seed) {
		auto s = skills;
		auto d = desc;
		std::mt19937 random(seed);
		const int moved = ap::randomizer::ShuffleSkillTrees(s, d, random);
		// Row 1 holds Fire Bolt, Warmth, Bash and Howl in some order: two Sorceress and two Barbarian slots.
		int sorceressRow1 = 0;
		for (size_t row : { 1U, 2U, 4U, 5U }) {
			EXPECT(d.Cell(row + 0, 2) == "1");
			sorceressRow1 += s.Cell(row, 1) == "sor" ? 1 : 0;
		}
		EXPECT(sorceressRow1 == 2);
		// Inferno and Leap trade places or stay; the Druid's skills and Attack never move.
		EXPECT(d.Cell(3, 2) == "2" && d.Cell(6, 2) == "2");
		EXPECT(s.Cell(7, 1) == "dru" && s.Cell(8, 1) == "dru" && s.Cell(0, 1).empty() && d.Cell(7, 1) == "2");
		mixed = mixed || moved > 0;
	}
	EXPECT(mixed);
}

void TestShopParcels() {
	auto misc = ap::Table::Parse("name\tcode\tcost\tAkaraMin\tAkaraMax\tMalahMin\tMalahMax\tspawnable\r\nStandard of Heroes\tstd\t1\t\t\t\t\t1\r\n");
	EXPECT(ap::randomizer::AddShopParcels(misc) == 5);
	EXPECT(ap::randomizer::AddShopParcels(misc) == 0);
	EXPECT(misc.Rows().size() == 6);
	EXPECT(misc.Cell(1, 1) == "ap1" && misc.Cell(1, 2) == "2500" && misc.Cell(1, 3) == "1" && misc.Cell(1, 5).empty() && misc.Cell(1, 7) == "0");
	EXPECT(misc.Cell(5, 1) == "ap5" && misc.Cell(5, 2) == "40000" && misc.Cell(5, 3).empty() && misc.Cell(5, 5) == "1");
	EXPECT(ap::randomizer::AddProphecyScrolls(misc) == 8 && ap::randomizer::AddProphecyScrolls(misc) == 0);
	// Every tier at every potion seller, the price rising.
	EXPECT(misc.Cell(6, 0) == "Scroll of Prophecy" && misc.Cell(6, 1) == "aq1" && misc.Cell(6, 2) == "1000" && misc.Cell(6, 3) == "1" && misc.Cell(6, 5) == "1");
	EXPECT(misc.Cell(13, 1) == "aq8" && misc.Cell(13, 2) == "100000");
	// The Ledger is sold nowhere and worth nothing.
	EXPECT(ap::randomizer::AddLedger(misc) == 1 && ap::randomizer::AddLedger(misc) == 0);
	EXPECT(misc.Cell(14, 0) == "Archipelago Ledger" && misc.Cell(14, 1) == "apl" && misc.Cell(14, 2) == "0" && misc.Cell(14, 3).empty()
	       && misc.Cell(14, 7) == "0");
}

void TestTableEdits() {
	auto skills = ap::Table::Parse("skill\tcharclass\treqlevel\treqskill1\treqskill2\treqskill3\r\nAttack\t\t1\t\t\t\r\n"
	                               "Fire Bolt\tsor\t1\t\t\t\r\nAbyss\twar\t30\tSigil Death\tApocalypse\t\r\n");
	EXPECT(ap::randomizer::BlockClassSkills(skills) == 2);
	EXPECT(skills.Cell(0, 3).empty());
	EXPECT(skills.Cell(1, 2) == "0" && skills.Cell(1, 3) == "Fire Bolt");
	EXPECT(skills.Cell(2, 2) == "0" && skills.Cell(2, 3) == "Abyss" && skills.Cell(2, 4).empty());

	auto monlvl = ap::Table::Parse("Level\tXP\tXP(N)\tXP(H)\r\n1\t1\t100\t1000\r\n");
	ap::randomizer::ScaleExperience(monlvl, 250);
	EXPECT(monlvl.Cell(0, 1) == "2" && monlvl.Cell(0, 2) == "250" && monlvl.Cell(0, 3) == "2500");

	auto armor = ap::Table::Parse("name\treqstr\treqdex\tlevelreq\r\nCap\t0\t\t1\r\nMonarch\t156\t\t54\r\n");
	ap::randomizer::ClearRequirements(armor, true, false);
	EXPECT(armor.Cell(1, 1) == "156" && armor.Cell(1, 3) == "0");
	ap::randomizer::ClearRequirements(armor, false, true);
	EXPECT(armor.Cell(1, 1) == "0" && armor.Cell(1, 2).empty());

	auto uniques = ap::Table::Parse("index\t*ID\tversion\tspawnable\tlvl\tlvl req\tcode\tprop1\tmin1\tmax1\t*eol\r\nThe Gnasher\t0\t0\t1\t7\t5\thax\t\t\t\t0\r\n");
	ap::randomizer::AddCodexUnique(uniques, 1);
	ap::randomizer::AddCodexUnique(uniques, 1);
	EXPECT(uniques.Rows().size() == 2);
	EXPECT(uniques.Cell(1, 0) == "Archipelago Codex" && uniques.Cell(1, 1) == "1" && uniques.Cell(1, 3) == "0" && uniques.Cell(1, 5) == "1");
	EXPECT(uniques.Cell(1, 6) == "cm1" && uniques.Rows()[1].size() == 11);

	ap::randomizer::Settings settings;
	EXPECT(!settings.Any());
	settings.monsters = ap::randomizer::Shuffle::Nearby;
	EXPECT(settings.Any() && settings.Stamp().find("monsters=1") != std::string::npos);
}

} // namespace

int main() {
	TestQuestTriggers();
	TestCodexLevels();
	TestTableRoundTrip();
	TestMonsterShuffle();
	TestBalancedAnywhere();
	TestDensityAndDrops();
	TestSuperUniqueShuffle();
	TestSuperUniqueMonsters();
	TestShopParcels();
	TestSkillTreeShuffle();
	TestRunewordShuffle();
	TestTableEdits();
	TestLootScalesToLevel();
	TestOtherLoot();
	TestGoldExperienceCharms();
	TestShrinesAndTraps();
	TestClientParsesServerPackets();
	TestClientDeathLink();
	TestConfigRoundTrip();
	TestConfigDefaults();
	TestSaveStateRoundTrip();
	TestIdsMatchApworld();
	TestGameData();
	std::printf(failures == 0 ? "all tests passed\n" : "%d failure(s)\n", failures);
	return failures == 0 ? 0 : 1;
}
