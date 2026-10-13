// Unit tests for the parts of the plugin that don't need the game.

#include "ap_client.h"
#include "character.h"
#include "codex.h"
#include "command_panel.h"
#include "config.h"
#include "repairs.h"
#include "game_data.h"
#include "ids.h"
#include "loot.h"
#include "milestones.h"
#include "personalize.h"
#include "quest_triggers.h"
#include "randomizer.h"
#include "table.h"
#include "save_state.h"
#include "session_log.h"
#include "skill_table.h"
#include "sounds.h"
#include "starter_kit.h"
#include "sprite.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
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
	// After a tree shuffle, Teleport (54) sits in Bash's (126) slot: it's a Barbarian skill now, and Bash isn't.
	const auto dealt = ap::codex::Chunks(mixed, "Barbarian", 60, { { 54, 126 }, { 126, 54 } });
	EXPECT((dealt[0] == std::vector<ap::codex::Entry> { { 54, 3 }, { 132, 1 } }));

	// Tree mode: Teleport twice, Bash once and two Skill Points; Bash was unlocked and one point given before.
	const std::vector<ap::NetworkItem> received = {
		{ ap::ids::ItemBase + 54, 0, 1, 0 }, { ap::ids::ItemBase + 126, 0, 1, 0 }, { ap::ids::SkillPointItem, 0, 1, 0 },
		{ ap::ids::ItemBase + 54, 0, 1, 0 }, { ap::ids::SkillPointItem, 0, 1, 0 }, { ap::ids::EmpowermentItem, 0, 1, 0 },
	};
	const auto plan = ap::codex::PlanTreeDelivery(received, { 126 }, 1);
	EXPECT(plan.unlock == std::vector<uint32_t> { 54 } && plan.points == 2);
	EXPECT(ap::codex::PlanTreeDelivery(received, { 54, 126 }, 3).unlock.empty() && ap::codex::PlanTreeDelivery(received, { 54, 126 }, 9).points == 0);
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
	config.debugTools = true;
	config.repairOnly = true;
	EXPECT(!ap::ParseConfig("").repairOnly);
	EXPECT(!ap::ParseConfig("").debugTools);
	const ap::Config parsed = ap::ParseConfig(ap::FormatConfig(config));
	EXPECT(parsed.debugTools);
	EXPECT(parsed.repairOnly);
	EXPECT(!ap::ParseConfig("repair_only = false").repairOnly);
	EXPECT(ap::ParseConfig("").notifications);
	EXPECT(ap::ParseConfig("").statusLine && ap::ParseConfig("").statusPosition == "top-left");
	config.statusLine = false;
	config.statusPosition = "bottom-left";
	EXPECT(!ap::ParseConfig(ap::FormatConfig(config)).statusLine && ap::ParseConfig(ap::FormatConfig(config)).statusPosition == "bottom-left");
	EXPECT(ap::ParseConfig("").bannerSounds);
	config.bannerSounds = false;
	EXPECT(!ap::ParseConfig(ap::FormatConfig(config)).bannerSounds);
	EXPECT(ap::ParseConfig("").bannerVolume == 10);
	config.bannerVolume = 40;
	EXPECT(ap::ParseConfig(ap::FormatConfig(config)).bannerVolume == 40);
	EXPECT(ap::ParseConfig("banner_volume = 500").bannerVolume == 200);
	EXPECT(ap::ParseConfig("banner_volume = -5").bannerVolume == 0);
	EXPECT(ap::ParseConfig("banner_volume = 999999999999").bannerVolume == 200);
	EXPECT(ap::ParseConfig("banner_volume = 99999999999999999999").bannerVolume == 10);
	EXPECT(ap::ParseConfig("banner_volume = loud").bannerVolume == 10);
	EXPECT(ap::ParseConfig("banner_volume = 50oops").bannerVolume == 10);
	EXPECT(ap::ParseConfig("banner_volume = 0.5").bannerVolume == 10);
	EXPECT(ap::ParseConfig("banner_volume = 60 # quieter").bannerVolume == 60);
	config.notifications = false;
	EXPECT(!ap::ParseConfig(ap::FormatConfig(config)).notifications);
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

void TestConfigNumericIds() {
	const ap::Config defaults;
	const std::vector<std::pair<std::string, uint32_t ap::Config::*>> settings {
		{ "codex_unique", &ap::Config::codexUnique },
		{ "codex_prefix", &ap::Config::codexPrefix },
		{ "oskill_property", &ap::Config::oskillProperty },
	};
	for (const auto& [key, field] : settings) {
		for (const std::string value : { "abc", "-1", "", "\"\"", "4294967296", "99999999999999999999", "12oops", "1.5", "0x10", "+", "++1", "+-1", "+4294967296" }) {
			std::vector<std::string> warnings;
			const auto parsed = ap::ParseConfig("# header\n[archipelago]\n" + key + " = " + value + "\nslot = \"Still parsed\"\n", &warnings);
			EXPECT(parsed.*field == defaults.*field);
			EXPECT(parsed.slot == "Still parsed");
			EXPECT(warnings.size() == 1);
			if (!warnings.empty()) {
				EXPECT(warnings[0].find(key) != std::string::npos);
				EXPECT(warnings[0].find("line 3") != std::string::npos);
				EXPECT(warnings[0].find("keeping " + std::to_string(defaults.*field)) != std::string::npos);
			}
			// Callers that don't request diagnostics must also be safe.
			EXPECT(ap::ParseConfig(key + " = " + value).*field == defaults.*field);
		}
		for (const auto& [value, expected] : std::vector<std::pair<std::string, uint32_t>> {
			{ "0", 0 }, { "123", 123 }, { "4294967295", UINT32_MAX }, { " 265 # oskill", 265 }, { "\"500\"", 500 },
			{ "+0", 0 }, { "+500", 500 }, { "+4294967295", UINT32_MAX },
		}) {
			std::vector<std::string> warnings;
			EXPECT(ap::ParseConfig(key + " = " + value, &warnings).*field == expected);
			EXPECT(warnings.empty());
		}
		EXPECT(ap::ParseConfig(key + " = 123\n" + key + " = bad\n").*field == 123);
	}
}

void TestLoaderSettings() {
	const std::string text = ap::FormatLoaderSettings({
		{ "d2rcore.items.show_ground_sockets", "true" },
		{ "d2rcore.stash.add_shared_tabs", "50" },
		{ "d2rcore.items.floor_item_display_limit", "64" },
		// Skipped: a global loader setting, a bad table, a value that isn't a bool or number.
		{ "d2rloader.skip_title_screen", "true" },
		{ "d2rcore.it ems.x", "1" },
		{ "d2rcore.items", "1" },
		{ "d2rcore.items.display_item_levels", "\"yes\"\nevil = 1" },
	});
	EXPECT(text.find("\n[d2rcore.items]\nshow_ground_sockets = true\nfloor_item_display_limit = 64\n\n[d2rcore.stash]\nadd_shared_tabs = 50\n")
	       != std::string::npos);
	EXPECT(text.find("d2rloader]") == std::string::npos && text.find("evil") == std::string::npos && text.find("it ems") == std::string::npos);
	EXPECT(text.find("display_item_levels") == std::string::npos);
	// Nothing to override: only the header comment, no tables.
	EXPECT(ap::FormatLoaderSettings({}).find('[') == std::string::npos);
}

void TestScaleWav() {
	// A 16-bit mono WAV with an odd-sized chunk before the samples.
	const auto u16 = [](std::string& out, uint16_t value) { out.append(reinterpret_cast<const char*>(&value), 2); };
	const auto u32 = [](std::string& out, uint32_t value) { out.append(reinterpret_cast<const char*>(&value), 4); };
	const int16_t samples[] = { 1000, -1000, 30000, -30000 };
	std::string wav = "RIFF";
	u32(wav, 0);
	wav += "WAVEfmt ";
	u32(wav, 16);
	u16(wav, 1);
	u16(wav, 1);
	u32(wav, 44100);
	u32(wav, 88200);
	u16(wav, 2);
	u16(wav, 16);
	wav += "LIST";
	u32(wav, 3);
	wav += std::string("abc\0", 4);
	wav += "data";
	u32(wav, sizeof(samples));
	wav.append(reinterpret_cast<const char*>(samples), sizeof(samples));
	std::string scaled = wav;
	EXPECT(ap::sounds::ScaleWav(scaled, 150));
	int16_t out[4] = {};
	std::memcpy(out, scaled.data() + scaled.size() - sizeof(out), sizeof(out));
	EXPECT(out[0] == 1500 && out[1] == -1500 && out[2] == 32767 && out[3] == -32768);
	EXPECT(scaled.compare(0, scaled.size() - sizeof(out), wav, 0, wav.size() - sizeof(out)) == 0);
	std::string notWav = "not a wav file";
	EXPECT(!ap::sounds::ScaleWav(notWav, 50) && notWav == "not a wav file");
	std::string bits8 = wav;
	bits8[34] = 8; // bits per sample
	EXPECT(!ap::sounds::ScaleWav(bits8, 50));
	// The same samples under a WAVE_FORMAT_EXTENSIBLE header, as Media Foundation may write.
	std::string extensible = wav.substr(0, 16);
	u32(extensible, 40);
	u16(extensible, 0xFFFE);
	extensible += wav.substr(22, 14); // channels through bits per sample
	u16(extensible, 22);              // extra bytes
	u16(extensible, 16);              // valid bits
	u32(extensible, 4);               // channel mask
	extensible += std::string("\x01\x00\x00\x00\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71", 16); // PCM
	extensible += wav.substr(36);
	EXPECT(ap::sounds::ScaleWav(extensible, 50));
	std::memcpy(out, extensible.data() + extensible.size() - sizeof(out), sizeof(out));
	EXPECT(out[0] == 500 && out[1] == -500 && out[2] == 15000 && out[3] == -15000);
	extensible[44] = 3; // IEEE float sub-format
	EXPECT(!ap::sounds::ScaleWav(extensible, 50));
}

void TestSaveStateRoundTrip() {
	const auto path = std::filesystem::temp_directory_path() / "d2rl-ap-test" / "seed_slot.txt";
	ap::SaveState state;
	state.checks = { ap::ids::QuestLocation(0, 1), ap::ids::AreaLocation(0, 38), ap::ids::LevelLocation(30) };
	state.fillersDelivered = 7;
	state.goalReached = true;
	state.playerClass = 4;
	state.kills = { { 3, 120 }, { 24, 7 } };
	state.cowKills = { { 1, 64 } };
	state.foundUniques = { 11, 4000000000U };
	state.foundSets = { 5 };
	state.ownHints = { 0, 3 };
	state.otherHints = { 2425500 };
	state.prophecies = 2;
	state.ledgerGiven = true;
	state.unlockedSkills = { 126, 54 };
	state.skillPointsGiven = 3;
	state.portalTomeGiven = true;
	state.notifiedItems = 12;
	state.heldPotions = { 0x20317068, 0x20317068, 0x206C7672 };
	EXPECT(state.Save(path));
	ap::SaveState loaded;
	EXPECT(loaded.Load(path));
	EXPECT(loaded.checks == state.checks);
	EXPECT(loaded.fillersDelivered == 7);
	EXPECT(loaded.goalReached);
	EXPECT(loaded.playerClass == 4);
	EXPECT(loaded.kills == state.kills && loaded.cowKills == state.cowKills && loaded.foundUniques == state.foundUniques && loaded.foundSets == state.foundSets);
	EXPECT(loaded.ownHints == state.ownHints && loaded.otherHints == state.otherHints && loaded.prophecies == 2 && loaded.ledgerGiven
	       && loaded.unlockedSkills == state.unlockedSkills && loaded.skillPointsGiven == 3 && loaded.portalTomeGiven && loaded.notifiedItems == 12);
	EXPECT(loaded.heldPotions == state.heldPotions);
	EXPECT(loaded.commands.empty() && loaded.checkpoint == 0 && !loaded.redeliver);
	// A save that can't be written says so (the plugin pauses rewards on it): here the
	// folder it would go in is a file.
	const auto blocked = path.parent_path() / "blocked";
	std::ofstream(blocked) << "not a folder";
	EXPECT(!state.Save(blocked / "seed_slot.txt"));
	std::filesystem::remove_all(path.parent_path());
}

// A save whose kill or cow counter reached a threshold without the check (the
// game stopped before the check was saved) gets it back when the seed binds.
void TestMilestoneRecovery() {
	using ap::ids::CowLocation;
	using ap::ids::KillLocation;
	auto sorted = [](std::vector<int64_t> checks) {
		std::ranges::sort(checks);
		return checks;
	};
	// Exactly at each threshold, and one below it.
	for (uint32_t index = 0; index < std::size(ap::ids::KillCounts); ++index) {
		const int threshold = static_cast<int>(ap::ids::KillCounts[index]);
		EXPECT(std::ranges::count(ap::milestones::KillChecks(0, 0, threshold), KillLocation(0, 0, index)) == 1);
		EXPECT(std::ranges::count(ap::milestones::KillChecks(0, 0, threshold - 1), KillLocation(0, 0, index)) == 0);
	}
	EXPECT(ap::milestones::CowChecks(1, static_cast<int>(ap::ids::CowKills)) == std::vector<int64_t> { CowLocation(1, 2) });
	EXPECT(ap::milestones::CowChecks(1, static_cast<int>(ap::ids::CowKills) - 1).empty());
	// Out-of-range keys from a damaged save give nothing.
	EXPECT(ap::milestones::KillChecks(3, 0, 1000).empty() && ap::milestones::KillChecks(0, 5, 1000).empty() && ap::milestones::CowChecks(-1, 1000).empty());

	ap::SaveState save;
	save.kills = {
		{ 0, 100 },  // Normal Act I at 100: the 100-kill check is missing.
		{ 12, 600 }, // Nightmare Act III past every threshold: 250 and 500 missing, 100 found.
		{ 24, 99 },  // Hell Act V below the first threshold.
		{ 3, 250 },  // Normal Act IV: both on the server already.
	};
	save.cowKills = { { 0, 100 }, { 2, 140 }, { 1, 99 } };
	save.checks = { KillLocation(1, 2, 0), KillLocation(0, 3, 0), KillLocation(0, 3, 1), CowLocation(2, 2) };
	const std::vector<int64_t> both = sorted(ap::milestones::Missing(save, true, true));
	EXPECT(both == sorted({ KillLocation(0, 0, 0), KillLocation(1, 2, 1), KillLocation(1, 2, 2), CowLocation(0, 2) }));
	// Disabled categories recover nothing.
	EXPECT(sorted(ap::milestones::Missing(save, true, false)) == sorted({ KillLocation(0, 0, 0), KillLocation(1, 2, 1), KillLocation(1, 2, 2) }));
	EXPECT(ap::milestones::Missing(save, false, true) == std::vector<int64_t> { CowLocation(0, 2) });
	EXPECT(ap::milestones::Missing(save, false, false).empty());
	// The same through a save file, as BindSession loads it; once the checks are
	// in the save nothing is missing, so they aren't sent or announced again.
	const auto path = std::filesystem::temp_directory_path() / "d2rl-ap-test-milestones" / "seed_slot.txt";
	EXPECT(save.Save(path));
	ap::SaveState loaded;
	EXPECT(loaded.Load(path));
	const std::vector<int64_t> recovered = ap::milestones::Missing(loaded, true, true);
	EXPECT(sorted(recovered) == both);
	loaded.checks.insert(recovered.begin(), recovered.end());
	EXPECT(ap::milestones::Missing(loaded, true, true).empty());
	std::filesystem::remove_all(path.parent_path());
}

// The 100 cows add up per difficulty over the whole seed (#79): across games,
// reconnects and kills made before a seed is bound, only in the Cow Level.
void TestLifetimeCows() {
	using ap::ids::CowLocation;
	using ap::milestones::CountCow;
	static constexpr int Cows = static_cast<int>(ap::data::CowLevel);
	const auto path = std::filesystem::temp_directory_path() / "d2rl-ap-test-cows" / "seed_slot.txt";
	auto kill = [](ap::SaveState& save, int difficulty, int count, int level = Cows, bool shuffled = false) {
		std::vector<int64_t> reached;
		for (int i = 0; i < count; ++i) {
			for (const int64_t location : CountCow(save, difficulty, level, shuffled).reached) {
				reached.push_back(location);
				save.checks.insert(location); // as AddChecks does
			}
		}
		return reached;
	};
	// 60 in one game, the save written as the game is left; 40 in the next game.
	{
		ap::SaveState save;
		EXPECT(kill(save, 0, 60).empty());
		EXPECT(save.Save(path));
		ap::SaveState next;
		EXPECT(next.Load(path));
		EXPECT(next.cowKills[0] == 60);
		EXPECT(kill(next, 0, 39).empty());
		EXPECT(kill(next, 0, 1) == std::vector<int64_t> { CowLocation(0, 2) });
		// Once found, more cows don't send it again.
		EXPECT(kill(next, 0, 50).empty() && next.cowKills[0] == 150);
	}
	// 60, an AP reconnect in the same game (the save is written, then reloaded by
	// the rebind), then 40.
	{
		ap::SaveState save;
		kill(save, 1, 60);
		EXPECT(save.Save(path));
		ap::SaveState rebound;
		EXPECT(rebound.Load(path));
		EXPECT(kill(rebound, 1, 40) == std::vector<int64_t> { CowLocation(1, 2) });
	}
	// 60 bound, then 40 while unbound (ap-connect to the same slot again): they join
	// the save on bind, and the check is recovered without another cow.
	{
		ap::SaveState save;
		kill(save, 2, 60);
		ap::milestones::HeldCows held;
		for (int i = 0; i < 40; ++i) {
			held.Kill(2, Cows);
		}
		held.MoveTo(save, false);
		EXPECT(save.cowKills[2] == 100);
		EXPECT(ap::milestones::Missing(save, false, true) == std::vector<int64_t> { CowLocation(2, 2) });
		// The held cows are gone once moved.
		held.MoveTo(save, false);
		EXPECT(save.cowKills[2] == 100);
	}
	// Difficulties don't share cows.
	{
		ap::SaveState save;
		EXPECT(kill(save, 0, 60).empty() && kill(save, 1, 40).empty() && kill(save, 2, 99).empty());
		EXPECT(ap::milestones::Missing(save, false, true).empty());
		ap::milestones::HeldCows held;
		held.Kill(1, Cows);
		held.Kill(3, Cows); // out of range
		held.MoveTo(save, false);
		EXPECT(save.cowKills[0] == 60 && save.cowKills[1] == 41 && save.cowKills[2] == 99 && !save.cowKills.contains(3));
		EXPECT(CountCow(save, 3, Cows, false).reached.empty() && !CountCow(save, -1, Cows, false).counted);
	}
	// Only the Cow Level's cows count, unless the seed shuffled them into another area.
	{
		EXPECT(ap::milestones::CowCounts(Cows, false) && !ap::milestones::CowCounts(2, false) && ap::milestones::CowCounts(2, true));
		ap::SaveState save;
		EXPECT(!CountCow(save, 0, 2, false).counted && !save.cowKills.contains(0));
		EXPECT(kill(save, 0, 150, 2).empty() && !save.cowKills.contains(0));
		EXPECT(kill(save, 0, 100, 2, true) == std::vector<int64_t> { CowLocation(0, 2) });
		// Held cows outside the Cow Level only count for a seed that shuffled them.
		ap::SaveState plain;
		ap::SaveState shuffled;
		ap::milestones::HeldCows held;
		for (int i = 0; i < 30; ++i) {
			held.Kill(0, Cows);
			held.Kill(0, 2);
		}
		ap::milestones::HeldCows copy = held;
		held.MoveTo(plain, false);
		copy.MoveTo(shuffled, true);
		EXPECT(plain.cowKills[0] == 30 && shuffled.cowKills[0] == 60);
	}
	std::filesystem::remove_all(path.parent_path());
}

// Reward commands wait in the save, spaces and all, and leave only once they ran (#70).
void TestSaveStatePendingCommands() {
	const auto path = std::filesystem::temp_directory_path() / "d2rl-ap-test-commands" / "seed_slot.txt";
	ap::SaveState state;
	state.fillersDelivered = 9;
	state.commands = { { 4, "gold 1200" }, { -1, "skadd Fire Bolt,1" }, { 6, "experience 5000" }, { 4, "gold 1200" } };
	state.checkpoint = 5;
	state.lostFrom = 2;
	state.lostTo = 5;
	state.redeliver = true;
	state.redeliverMark = 1;
	state.ran = { { 3, "gold 10" } };
	state.lostCommands = { { -1, "skillpoints 2" } };
	state.playerClass = 6;
	EXPECT(state.Save(path));
	ap::SaveState loaded;
	EXPECT(loaded.Load(path));
	EXPECT(loaded.commands == state.commands && loaded.ran == state.ran && loaded.lostCommands == state.lostCommands);
	EXPECT(loaded.fillersDelivered == 9 && loaded.checkpoint == 5 && loaded.lostFrom == 2 && loaded.lostTo == 5 && loaded.redeliver
	       && loaded.redeliverMark == 1 && loaded.playerClass == 6);
	// Each one that ran takes its first match and is remembered as run; the rest keep their order.
	loaded.FinishCommands({ { 4, "gold 1200" }, { -1, "skadd Fire Bolt,1" }, { 7, "gold 1" } });
	EXPECT((loaded.commands == std::vector<ap::PendingCommand> { { 6, "experience 5000" }, { 4, "gold 1200" } }));
	EXPECT((loaded.ran == std::vector<ap::PendingCommand> { { 3, "gold 10" }, { 4, "gold 1200" }, { -1, "skadd Fire Bolt,1" } }));
	// A save written by an older plugin (no command lines) still loads. Its "class" was whatever
	// character was loaded, maybe a mule, so it isn't taken as the seed's class.
	{
		std::ofstream old(path, std::ios::trunc);
		old << "fillers 3\nclass 2\ncheck 5\n";
	}
	ap::SaveState older;
	EXPECT(older.Load(path) && older.fillersDelivered == 3 && older.playerClass == -1 && older.checks.contains(5) && older.commands.empty());
	// Its rewards count as kept, not as lost in a crash.
	EXPECT(older.checkpoint == 3 && !older.DetectUncleanExit());
	std::filesystem::remove_all(path.parent_path());
}

// A game never left (a crash) offers its rewards again with ap-redeliver (#70).
void TestUncleanExit() {
	ap::SaveState state;
	state.fillersDelivered = 10;
	state.checkpoint = 10;
	EXPECT(!state.DetectUncleanExit());
	// Items 10..14 went out, then D2R closed without leaving the game.
	state.fillersDelivered = 15;
	state.commands = { { 3, "gold 100" }, { 12, "gold 900" }, { -1, "skillpoints 1" }, { 14, "experience 70" } };
	// Commands that ran in that game: a tree skill, and gold from an earlier item and a lost one.
	state.ran = { { -1, "skadd Fire Bolt,1" }, { 2, "gold 50" }, { 11, "gold 400" } };
	EXPECT(state.DetectUncleanExit());
	EXPECT(state.lostFrom == 10 && state.lostTo == 15 && state.checkpoint == 15 && !state.redeliver && state.ran.empty());
	EXPECT(state.lostCommands.size() == 3);
	// Only once: the next start finds nothing new.
	EXPECT(!state.DetectUncleanExit());
	// Another crash right after, before ap-redeliver: the two are offered together.
	state.fillersDelivered = 18;
	EXPECT(state.DetectUncleanExit());
	EXPECT(state.lostFrom == 10 && state.lostTo == 18);
	// ap-redeliver drops the lost items' commands that never ran (they're made again) and runs
	// the other lost ones again: the skill and the earlier item's gold, not the lost item's.
	EXPECT(state.StartRedelivery());
	EXPECT(state.redeliver && state.redeliverMark == 10 && state.lostCommands.empty());
	EXPECT((state.commands == std::vector<ap::PendingCommand> { { 3, "gold 100" }, { -1, "skillpoints 1" }, { -1, "skadd Fire Bolt,1" }, { 2, "gold 50" } }));
	EXPECT(!state.StartRedelivery());
	// Items 10..12 are handed out again (and one's gold queued), then D2R crashes again: the
	// redelivery starts over from 10, and the queued gold goes (it's made again).
	state.lostFrom = 13;
	state.commands.push_back({ 11, "gold 420" });
	EXPECT(state.DetectUncleanExit());
	EXPECT(state.redeliver && state.lostFrom == 10 && state.lostTo == 18);
	EXPECT(std::ranges::none_of(state.commands, [](const ap::PendingCommand& command) { return command.item == 11; }));
	// It finishes; leaving the game then clears it, and moves the checkpoint, so a clean exit loses nothing.
	state.lostFrom = 18;
	state.fillersDelivered = 20;
	state.ran = { { 2, "gold 50" } };
	state.Checkpoint();
	EXPECT(state.checkpoint == 20 && state.ran.empty() && !state.redeliver && state.lostFrom == 0 && state.lostTo == 0);
	EXPECT(!state.DetectUncleanExit());
	// Only commands ran in a crashed game (no new items): still offered.
	state.ran = { { -1, "skillpoints 1" } };
	EXPECT(state.DetectUncleanExit() && state.lostFrom == state.lostTo && state.lostCommands.size() == 1);
	EXPECT(state.StartRedelivery() && !state.redeliver && state.commands.back() == (ap::PendingCommand { -1, "skillpoints 1" }));
	// A later loss that doesn't follow on starts its own window.
	ap::SaveState later;
	later.lostFrom = 1;
	later.lostTo = 3;
	later.checkpoint = 8;
	later.fillersDelivered = 11;
	EXPECT(later.DetectUncleanExit() && later.lostFrom == 8 && later.lostTo == 11);
	ap::SaveState nothing;
	EXPECT(!nothing.StartRedelivery());
}

// Rewards only go to a character of the seed's class (#70).
void TestSessionLog() {
	namespace log = ap::session_log;
	using namespace std::chrono;
	const auto start = system_clock::now();
	// Names sort in time order, and only session files are pruned, oldest first.
	EXPECT(log::FileName(start) < log::FileName(start + seconds(1)));
	EXPECT(log::FileName(start) < log::FileName(start + milliseconds(1)));
	const std::vector<std::filesystem::path> files {
		"session-2026-10-13_10-00-00-000.log", "notes.txt", "session-2026-10-12_10-00-00-000.log", "session-2026-10-13_09-00-00-000.log" };
	EXPECT((log::Stale(files, 1) == std::vector<std::filesystem::path> { files[2], files[3] }));
	EXPECT(log::Stale(files, 3).empty());

	const auto folder = std::filesystem::temp_directory_path() / "d2rl-ap-test-logs";
	std::filesystem::remove_all(folder);
	auto opened = log::Open(folder, 3, start);
	EXPECT(!opened.path.empty() && opened.previous.empty() && !opened.previousUnclean);
	log::Write("INFO", "first session");
	log::Close();
	log::Write("INFO", "after closing: dropped");
	// A closed session is clean; one without an end line (a crash) isn't.
	opened = log::Open(folder, 3, start + seconds(1));
	EXPECT(opened.previous.filename() == log::FileName(start) && !opened.previousUnclean);
	log::Write("INFO", "crashed session");
	opened = log::Open(folder, 3, start + seconds(2));
	EXPECT(opened.previous.filename() == log::FileName(start + seconds(1)) && opened.previousUnclean);
	log::Write("INFO", "Archipelago: D2R is closing.");
	opened = log::Open(folder, 3, start + seconds(3));
	EXPECT(!opened.previousUnclean);
	log::Close();
	// Keep counts the new file: the oldest went.
	size_t count = 0;
	for (const auto& entry : std::filesystem::directory_iterator(folder)) {
		EXPECT(entry.path().filename() != log::FileName(start));
		++count;
	}
	EXPECT(count == 3);
	std::ifstream first(folder / log::FileName(start + seconds(1)));
	const std::string text((std::istreambuf_iterator<char>(first)), std::istreambuf_iterator<char>());
	EXPECT(text.find("INFO crashed session") != std::string::npos && text.find("dropped") == std::string::npos);
	first.close();
	std::filesystem::remove_all(folder);
}

void TestSkillTablePrerequisite() {
	namespace st = ap::skill_table;
	constexpr size_t RowSize = 40, Rows = 300, IdAt = 0, ReqAt = 20;
	std::vector<uint8_t> rows(RowSize * Rows, 0);
	const auto put = [&](size_t row, size_t offset, uint16_t value) { std::memcpy(rows.data() + row * RowSize + offset, &value, 2); };
	std::vector<uint16_t> classSkills;
	for (uint16_t skill = 6; skill < 280; skill += 3) {
		classSkills.push_back(skill);
	}
	for (size_t row = 0; row < Rows; ++row) {
		put(row, IdAt, static_cast<uint16_t>(row)); // the id names its own row in every row
		put(row, ReqAt, st::NoSkill);
		put(row, ReqAt + 2, st::NoSkill);
		put(row, ReqAt + 4, st::NoSkill);
	}
	for (const uint16_t skill : classSkills) {
		put(skill, ReqAt, skill); // locked by d2r-ap-setup
	}
	EXPECT(st::FindSelfPrerequisite(rows.data(), Rows, RowSize, classSkills) == ReqAt);
	EXPECT(st::IsSelfLocked(rows.data(), Rows, RowSize, ReqAt, 9) && !st::IsSelfLocked(rows.data(), Rows, RowSize, ReqAt, 10));
	// Still found with a third of the skills unlocked already.
	for (size_t i = 0; i < classSkills.size() / 3; ++i) {
		put(classSkills[i], ReqAt, st::NoSkill);
	}
	EXPECT(st::FindSelfPrerequisite(rows.data(), Rows, RowSize, classSkills) == ReqAt);
	EXPECT(!st::IsSelfLocked(rows.data(), Rows, RowSize, ReqAt, classSkills[0]));
	// A mod without the lock: nothing to find.
	for (const uint16_t skill : classSkills) {
		put(skill, ReqAt, st::NoSkill);
	}
	EXPECT(!st::FindSelfPrerequisite(rows.data(), Rows, RowSize, classSkills));
}

void TestStarterKit() {
	int cells = 0;
	for (const auto& entry : ap::starter_kit::All()) {
		const size_t length = std::strlen(entry.code);
		EXPECT(length >= 3 && length <= 4);
		EXPECT(entry.count > 0 && std::strlen(entry.name) > 0);
		cells += entry.count;
	}
	// Small items, so the kit fits in a fresh character's inventory (40 cells) next to its starting gear.
	EXPECT(cells <= 12);
	EXPECT(ap::panel::Find(ap::panel::Debug(), "debug-kit") != nullptr);
}

// ap-debug kills and cows: one short of the first check the save hasn't got.
void TestDebugCounts() {
	using ap::ids::CowLocation;
	using ap::ids::KillLocation;
	using ap::milestones::CowsBeforeNext;
	using ap::milestones::KillsBeforeNext;
	ap::SaveState save;
	EXPECT(KillsBeforeNext(save, 0, 0) == 99);
	save.checks = { KillLocation(1, 2, 0) };
	EXPECT(KillsBeforeNext(save, 1, 2) == 249);
	// Only that act and difficulty.
	EXPECT(KillsBeforeNext(save, 0, 2) == 99 && KillsBeforeNext(save, 1, 3) == 99);
	// A gap: the 250 check is missing though 500 is in.
	save.checks = { KillLocation(0, 0, 0), KillLocation(0, 0, 2) };
	EXPECT(KillsBeforeNext(save, 0, 0) == 249);
	save.checks.insert(KillLocation(0, 0, 1));
	EXPECT(KillsBeforeNext(save, 0, 0) == -1);
	EXPECT(KillsBeforeNext(save, 3, 0) == -1 && KillsBeforeNext(save, 0, 5) == -1 && KillsBeforeNext(save, -1, 0) == -1);
	// The next kill at that count reaches the check.
	save.checks.clear();
	const int kills = KillsBeforeNext(save, 2, 4);
	EXPECT(ap::milestones::KillChecks(2, 4, kills).empty() && ap::milestones::KillChecks(2, 4, kills + 1) == std::vector<int64_t> { KillLocation(2, 4, 0) });

	EXPECT(CowsBeforeNext(save, 1) == static_cast<int>(ap::ids::CowKills) - 1);
	save.checks = { CowLocation(1, 2) };
	EXPECT(CowsBeforeNext(save, 1) == -1 && CowsBeforeNext(save, 0) == 99);
	EXPECT(CowsBeforeNext(save, 3) == -1);
	// ap-debug kills 260 sets a count past two thresholds, both recovered on the next bind.
	save.checks.clear();
	save.kills[0] = 260;
	EXPECT(ap::milestones::Missing(save, true, false).size() == 2);
	EXPECT(ap::panel::Find(ap::panel::Debug(), "debug-kills-260")->action == "ap-debug kills 260");
}

void TestCharacterCheck() {
	using ap::character::Verdict;
	EXPECT(ap::character::Check(4, -1) == Verdict::Unknown);
	EXPECT(ap::character::Check(-1, -1) == Verdict::Unknown);
	EXPECT(ap::character::Check(4, 4) == Verdict::Match);
	EXPECT(ap::character::Check(4, 1) == Verdict::Mismatch);
	// A seed with no class yet goes to the first character that shows its class.
	EXPECT(ap::character::Check(-1, 7) == Verdict::Match);
}

// Mirrors apworld/d2r_archipelago/test/test_generation.py.
void TestIdsMatchApworld() {
	EXPECT(ap::ids::QuestLocation(0, 1) == 2'420'001);
	EXPECT(ap::ids::QuestLocation(2, 40) == 2'420'240);
	EXPECT(ap::ids::AreaLocation(0, 38) == 2'421'038);
	EXPECT(ap::ids::AreaLocation(2, 132) == 2'421'532);
	EXPECT(ap::ids::LevelLocation(30) == 2'422'030);
	// Level milestones' acts, as the apworld places them: Normal to Diablo (4 acts, cap 30), all of Hell (15, 90).
	EXPECT(ap::ids::LevelAct(5, 4, 30) == 0 && ap::ids::LevelAct(10, 4, 30) == 1 && ap::ids::LevelAct(30, 4, 30) == 3);
	EXPECT(ap::ids::LevelAct(80, 15, 90) == 13 && ap::ids::LevelAct(90, 15, 90) == 14);
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
	EXPECT(std::size(ap::data::SuperUniques) == 31);
	EXPECT(std::size(ap::data::Gems) == 35 && std::size(ap::data::Sets) == 32 && std::size(ap::data::Specials) == 13);
	EXPECT(std::string_view(ap::data::Sets[0].pieces[0]) == "Civerb's Ward" && ap::data::Sets[0].pieces[3] == nullptr);
	EXPECT(ap::data::Experience[0] == 0 && ap::data::Experience[1] == 500 && std::size(ap::data::Experience) == 99);
	EXPECT(std::size(ap::data::QuestItems) == 15 && std::size(ap::data::Ubers) == 6);
	EXPECT(ap::data::LevelActs[1] == 0 && ap::data::LevelActs[40] == 1 && ap::data::LevelActs[75] == 2 && ap::data::LevelActs[103] == 3
	       && ap::data::LevelActs[109] == 4);
	for (const auto& superUnique : ap::data::SuperUniques) {
		EXPECT(superUnique.id < 100 && superUnique.act < 5);
	}
	EXPECT(std::string_view(ap::data::SuperUniques[1].name) == "Bonebreak"
	       && std::string_view(ap::data::SuperUniques[1].label) == "Bonebreaker (Crypt)");
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
		"level_step":5,"level_cap":90,"loader_settings":{"d2rcore.stash.add_shared_tabs":50,"d2rcore.items.show_ground_sockets":true,
		"d2rcore.items.odd":"text"}},"slot_info":{"1":{"name":"Snrn","game":"Diablo II Resurrected","type":1,"group_members":[],"class":"NetworkSlot"}},
		"hint_points":0}])json");
	client.Handle(R"json([{"cmd":"ReceivedItems","index":0,"items":[{"item":2430054,"location":-2,"player":1,"flags":1,"class":"NetworkItem"}]}])json");
	EXPECT(client.State() == ap::ConnectionState::Connected);
	EXPECT(client.LocationCount() == 2);
	EXPECT(client.Checked().contains(2420001));
	EXPECT(client.Missing() == std::vector<int64_t> { 2420002 });
	client.Handle(R"json([{"cmd":"RoomUpdate","checked_locations":[2420002]}])json");
	EXPECT(client.Missing().empty() && client.LocationCount() == 2);
	EXPECT(client.Slots().goal == 2 && !client.Slots().areaChecks && client.Slots().levelCap == 90 && client.Slots().sideAreas.empty());
	// Strings aren't D2RLoader values we write, so they're dropped.
	EXPECT(client.Slots().loaderSettings.size() == 2);
	EXPECT(std::ranges::count(client.Slots().loaderSettings, std::pair<std::string, std::string>("d2rcore.stash.add_shared_tabs", "50")) == 1);
	EXPECT(std::ranges::count(client.Slots().loaderSettings, std::pair<std::string, std::string>("d2rcore.items.show_ground_sockets", "true")) == 1);
	EXPECT(client.Items().size() == 1 && client.Items()[0].item == 2430054);
	EXPECT(client.ItemName(2430054, 1) == "Teleport (Sorceress)");
	// A gap in the item index must not append anything.
	client.Handle(R"json([{"cmd":"ReceivedItems","index":5,"items":[{"item":2431000,"location":3,"player":1,"flags":0}]}])json");
	EXPECT(client.Items().size() == 1);
	EXPECT(!client.Slots().deathLink && !client.Slots().superUniqueChecks && !client.Slots().locks.cube);
	// DeathLink is off for this seed, so deaths are ignored.
	client.Handle(R"json([{"cmd":"Bounced","tags":["DeathLink"],"data":{"time":1.0,"source":"Other","cause":"Other fell."}}])json");
	EXPECT(client.TakeDeaths().empty());
	EXPECT(client.ItemsSynced());
	// A new session (ap-connect to another slot) must not keep the previous slot's items or players:
	// the server sends ReceivedItems index 0 only when the new slot has items.
	const uint64_t generation = client.ItemsGeneration();
	client.Handle(R"json([{"cmd":"Connected","team":0,"slot":2,"players":[{"team":0,"slot":2,"alias":"Other","name":"Other","class":"NetworkPlayer"}],
		"missing_locations":[2420001],"checked_locations":[],"slot_data":{},"slot_info":{"2":{"name":"Other","game":"Diablo II Resurrected","type":1,"group_members":[],"class":"NetworkSlot"}},
		"hint_points":0}])json");
	EXPECT(client.Items().empty() && !client.ItemsSynced() && client.ItemsGeneration() != generation);
	const ap::ItemSnapshot snapshot = client.Snapshot();
	EXPECT(snapshot.items.empty() && !snapshot.synced && snapshot.session == client.SessionGeneration() && snapshot.generation == client.ItemsGeneration());
	EXPECT(client.PlayerName(1) == "player 1" && client.PlayerName(2) == "Other");
	client.Handle(R"json([{"cmd":"ReceivedItems","index":0,"items":[]}])json");
	EXPECT(client.Items().empty() && client.ItemsSynced());
}

void TestClientSurvivesBadPackets() {
	ap::Client client([](const std::string&) {});
	// Fields of the wrong type fall back to their defaults instead of failing the whole Connected,
	// and the ReceivedItems after it in the same message still arrives.
	client.Handle(R"json([{"cmd":"Connected","team":0,"slot":1,"players":[{"team":0,"slot":1,"alias":7,"name":"Snrn"}],
		"missing_locations":[2420002,"x"],"checked_locations":"none","slot_data":{"goal":"two","level_cap":90,"locks":true,
		"hints":[[1,"2",3],[4,1,5]],"randomizer":{"skill_tree":[[1,-2]],"xp_multiplier":"lots"}},
		"slot_info":{"one":{"game":"Diablo II Resurrected"},"1":{"game":5}}},
		{"cmd":"ReceivedItems","index":0,"items":[{"item":2430054,"location":-2,"player":1,"flags":1}]}])json");
	EXPECT(client.State() == ap::ConnectionState::Connected);
	EXPECT(client.Missing() == std::vector<int64_t> { 2420002 } && client.Checked().empty());
	EXPECT(client.Slots().goal == 0 && client.Slots().levelCap == 90 && !client.Slots().locks.cube);
	EXPECT(client.Slots().hints.size() == 1 && client.Slots().hints[0].location == 4);
	EXPECT(client.Slots().randomizer.skillTree.empty() && client.Slots().randomizer.xpPercent[2] == 100);
	EXPECT(client.PlayerName(1) == "Snrn");
	EXPECT(client.Items().size() == 1 && client.ItemsSynced());
	client.TakeMessages();
	// Bad fields in one packet don't take the rest of the message with them; ids that aren't numbers print as text.
	client.Handle(R"json([{"cmd":"DataPackage","data":{"games":{"Diablo II Resurrected":{"item_name_to_id":{"Bad":"1"}}}}},
		{"cmd":"PrintJSON","data":[{"type":"player_id","text":"abc"},{"text":" found "},{"type":"item_id","text":"2430054","player":1}]},
		{"cmd":"RoomUpdate","checked_locations":[2420002]}])json");
	EXPECT(client.Missing().empty());
	const auto messages = client.TakeMessages();
	EXPECT(messages.size() == 1 && messages[0] == "abc found item 2430054");
	// ap-disconnect stops claiming Connected right away, so nothing is sent into the closed socket.
	client.Disconnect();
	EXPECT(client.State() == ap::ConnectionState::Idle && client.StatusText().starts_with("Disconnected."));
}

void TestPanelConnectionTarget() {
	ap::Client client([](const std::string&) {});
	client.Handle(R"json([{"cmd":"Connected","checked_locations":[2420001],"missing_locations":[2420002]},
		{"cmd":"ReceivedItems","index":0,"items":[{"item":2430054,"location":-2,"player":1}]}])json");
	// A pending dialog must become stale as soon as another connection is requested,
	// while the previous accepted session generation still matches.
	EXPECT(client.HasCurrentSession());
	const auto accepted = client.SessionGeneration();
	const auto target = client.TargetGeneration();
	client.Connect("localhost:1", "OtherSlot", "");
	EXPECT(client.TargetGeneration() == target + 1);
	EXPECT(!client.HasCurrentSession());
	EXPECT(client.SessionGeneration() == accepted);
	client.Disconnect();
	EXPECT(client.TargetGeneration() == target + 2);
	EXPECT(client.SessionGeneration() == accepted);
	EXPECT(!client.HasCurrentSession());
	// A late Connected from the old socket must not count as acceptance of the
	// newly requested target. Feed explicit connection generations deterministically.
	client.Handle(R"json([{"cmd":"Connected"}])json", target);
	EXPECT(!client.HasCurrentSession());
	client.Handle(R"json([{"cmd":"Connected"}])json", client.TargetGeneration());
	EXPECT(client.HasCurrentSession());
	client.Handle(R"json([{"cmd":"ConnectionRefused","errors":["InvalidSlot"]}])json", client.TargetGeneration());
	EXPECT(!client.HasCurrentSession());
}

void TestClientDeathLink() {
	ap::Client client([](const std::string&) {});
	client.Connect("localhost", "Snrn", "");
	client.Disconnect();
	client.Handle(R"json([{"cmd":"Connected","team":0,"slot":1,"players":[],"missing_locations":[],"checked_locations":[],
		"slot_data":{"death_link":true,"superunique_checks":true,"shop_checks":3,"enforce_act_keys":true,"side_areas":[2421019,2421325],
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
	EXPECT(client.Slots().sideAreas == std::set<int64_t>({ ap::ids::AreaLocation(0, 19), ap::ids::AreaLocation(1, 125) }));
	client.Handle(R"json([{"cmd":"Bounced","tags":["DeathLink"],"data":{"time":1.0,"source":"Other","cause":"Other was slain by a cow."}}])json");
	// Our own death comes back from the server too; it's not a new one.
	client.Handle(R"json([{"cmd":"Bounced","tags":["DeathLink"],"data":{"time":2.0,"source":"Snrn","cause":"Snrn died."}}])json");
	client.Handle(R"json([{"cmd":"Bounced","tags":["Other"],"data":{"source":"Other"}}])json");
	client.Handle(R"json([{"cmd":"Bounced","tags":["DeathLink"],"data":{"time":3.0,"source":"Quiet"}}])json");
	const auto deaths = client.TakeDeaths();
	EXPECT(deaths.size() == 2 && deaths[0] == "Other was slain by a cow." && deaths[1] == "Quiet died.");
	EXPECT(client.TakeDeaths().empty());
}

// Hints found in game are sent again on every bind (issue #71): each own hint is its own CreateHints, so one
// the server rejects can't take the others with it, and other players' items go out as one LocationScouts.
void TestHintPackets() {
	EXPECT(ap::HintPackets({}, {}).empty());
	const auto packets = nlohmann::json::parse(ap::HintPackets({ { 77, 2, 2430951 }, { 2420001, 1, 2430054 } }, { 2420002, 2420003 }));
	EXPECT(packets.is_array() && packets.size() == 3);
	EXPECT(packets[0]["cmd"] == "CreateHints" && packets[0]["locations"] == nlohmann::json::array({ 77 }) && packets[0]["player"] == 2);
	EXPECT(packets[1]["cmd"] == "CreateHints" && packets[1]["locations"] == nlohmann::json::array({ 2420001 }) && packets[1]["player"] == 1);
	EXPECT(packets[2]["cmd"] == "LocationScouts" && packets[2]["locations"] == nlohmann::json::array({ 2420002, 2420003 })
	       && packets[2]["create_as_hint"] == 2);
	const auto others = nlohmann::json::parse(ap::HintPackets({}, { 5 }));
	EXPECT(others.size() == 1 && others[0]["cmd"] == "LocationScouts");
	const auto own = nlohmann::json::parse(ap::HintPackets({ { 9, 1, 3 } }, {}));
	EXPECT(own.size() == 1 && own[0]["cmd"] == "CreateHints");
}

void TestClientReconnect() {
	ap::Client client([](const std::string&) {});
	client.Handle(R"json([{"cmd":"RoomInfo","seed_name":"S1","games":["Diablo II Resurrected","Other Game"],
		"datapackage_checksums":{"Diablo II Resurrected":"a","Other Game":"b"}}])json");
	client.Handle(R"json([{"cmd":"Connected","team":0,"slot":1,"players":[],"missing_locations":[],"checked_locations":[],"slot_data":{},
		"slot_info":{"1":{"game":"Diablo II Resurrected"}}}])json");
	EXPECT(client.Seed() == "S1" && client.ConnectedSession() != 0 && client.ConnectedSession() == client.SessionGeneration());
	// ap-connect: the old session is over at once, so the plugin can't bind it (or its seed) again
	// before the new target's Connected.
	client.Connect("localhost:1", "Other", "");
	EXPECT(client.State() == ap::ConnectionState::Connecting && client.ConnectedSession() == 0 && client.Seed().empty());
	// The old socket's RoomInfo and Connected (target 0), read just before the ap-connect, are dropped.
	client.Handle(R"json([{"cmd":"RoomInfo","seed_name":"S1"},{"cmd":"Connected","team":0,"slot":1,"players":[],"missing_locations":[],
		"checked_locations":[],"slot_data":{},"slot_info":{}},{"cmd":"ConnectionRefused","errors":["InvalidSlot"]}])json", 0);
	EXPECT(client.State() == ap::ConnectionState::Connecting && client.ConnectedSession() == 0 && client.Seed().empty());
	client.Disconnect();
	EXPECT(client.ConnectedSession() == 0);
	// Names are only asked for when missing or changed, and only for this multiworld's games.
	using Games = std::vector<std::string>;
	const std::map<std::string, std::string> room { { "Diablo II Resurrected", "a" }, { "Other Game", "b" } };
	EXPECT((ap::DataPackageGames({ "Diablo II Resurrected", "Other Game" }, room, {}) == Games { "Diablo II Resurrected", "Other Game" }));
	EXPECT((ap::DataPackageGames({ "Diablo II Resurrected", "Other Game" }, room, { { "Diablo II Resurrected", "a" }, { "Other Game", "old" } })
	        == Games { "Other Game" }));
	EXPECT(ap::DataPackageGames({ "Diablo II Resurrected" }, room, { { "Diablo II Resurrected", "a" }, { "Unrelated", "c" } }).empty());
	// No checksum in the room: what's held stays.
	EXPECT(ap::DataPackageGames({ "Old World" }, room, { { "Old World", "" } }).empty());
	// A data package replaces that game's names.
	client.Handle(R"json([{"cmd":"DataPackage","data":{"games":{"Diablo II Resurrected":{"item_name_to_id":{"Teleport (Sorceress)":2430054},
		"location_name_to_id":{},"checksum":"a"}}}}])json");
	EXPECT(client.ItemName(2430054, 1) == "Teleport (Sorceress)");
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

void TestSocketedBases() {
	// The generated pool includes early two-socket weapons and armor, and
	// respects item-type caps (orbs have gemsockets=4 but a type cap of 3).
	for (const auto& base : ap::data::SocketBases) {
		EXPECT(base.sockets >= 2 && base.sockets <= 6);
	}
	const auto find = [](uint32_t code) -> const ap::data::Base* {
		for (const auto& base : ap::data::SocketBases) {
			if (ap::loot::CodeFromText(base.code) == code) return &base;
		}
		return nullptr;
	};
	for (const char* code : { "hax", "ssd", "qui", "lea" }) {
		const auto* base = find(ap::loot::CodeFromText(code));
		EXPECT(base && base->sockets == 2 && base->reqLevel == 0);
	}
	const auto* orb = find(ap::loot::CodeFromText("ob1"));
	EXPECT(orb && orb->sockets == 3);
	std::mt19937 random(61);
	bool sawTwoWeapon = false, sawTwoArmor = false, sawSixLate = false;
	for (int playerClass = -1; playerClass < 8; ++playerClass) {
		for (int level : { 1, 15, 30, 45, 60, 75, 90, 99 }) {
			for (int i = 0; i < 500; ++i) {
				const auto pick = ap::loot::Roll(ap::loot::Loot::SocketedBase, level, random, playerClass);
				const auto* base = find(pick.code);
				EXPECT(base != nullptr);
				if (!base) continue;
				EXPECT(pick.sockets >= 2 && pick.sockets <= base->sockets);
				EXPECT(base->cls < 0 || playerClass < 0 || base->cls == playerClass);
				EXPECT(base->reqLevel <= level + 2);
				EXPECT(pick.quality == D2RL::Items::Quality::Normal && pick.itemLevel == 90);
				if (base->sockets == 2) EXPECT(pick.sockets == 2);
				sawTwoWeapon |= level == 1 && pick.sockets == 2 && base->kind == ap::data::Weapon;
				sawTwoArmor |= level == 1 && pick.sockets == 2 && base->kind == ap::data::Armor;
				sawSixLate |= level >= 60 && pick.sockets == 6;
			}
		}
	}
	EXPECT(sawTwoWeapon && sawTwoArmor && sawSixLate);
}

void TestLootFitsClass() {
	std::mt19937 random(5);
	const auto baseClass = [](uint32_t code) {
		for (const auto& base : ap::data::SocketBases) {
			if (ap::loot::CodeFromText(base.code) == code) {
				return static_cast<int>(base.cls);
			}
		}
		return -2;
	};
	// A Sorceress (1) never gets another class's uniques, sets or bases.
	bool sawSorceressItem = false;
	for (int level : { 10, 42, 60, 85 }) {
		for (int i = 0; i < 300; ++i) {
			const auto* unique = FindRecord(ap::data::Uniques, ap::loot::Roll(ap::loot::Loot::RandomUnique, level, random, 1).record);
			const auto* set = FindRecord(ap::data::SetItems, ap::loot::Roll(ap::loot::Loot::SetItem, level, random, 1).record);
			EXPECT(unique != nullptr && (unique->cls == -1 || unique->cls == 1));
			EXPECT(set != nullptr && (set->cls == -1 || set->cls == 1));
			sawSorceressItem = sawSorceressItem || unique->cls == 1 || set->cls == 1;
			const int base = baseClass(ap::loot::Roll(ap::loot::Loot::SocketedBase, level, random, 1).code);
			EXPECT(base == -1 || base == 1);
		}
	}
	EXPECT(sawSorceressItem);
	// With the class unknown, class items still turn up.
	bool sawClassItem = false;
	for (int i = 0; i < 500 && !sawClassItem; ++i) {
		sawClassItem = FindRecord(ap::data::Uniques, ap::loot::Roll(ap::loot::Loot::RandomUnique, 42, random).record)->cls >= 0;
	}
	EXPECT(sawClassItem);
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
	// The seed's trap options: ambush size, the area's own monsters, tax per level.
	const ap::loot::TrapSettings tuning { .ambushSize = 12, .taxPerLevel = 250 };
	const std::string area[] = { "zombie1", "quillrat1" };
	for (int i = 0; i < 10; ++i) {
		const auto big = ap::loot::CommandsFor(ap::loot::Loot::AmbushTrap, 30, 4, random, tuning, area);
		EXPECT(big.commands.back() == "mspawn zombie1,12" || big.commands.back() == "mspawn quillrat1,12");
	}
	EXPECT(ap::loot::CommandsFor(ap::loot::Loot::AmbushTrap, 30, 4, random, tuning).commands.back().ends_with(",12"));
	EXPECT(ap::loot::CommandsFor(ap::loot::Loot::TaxCollectorTrap, 30, 0, random, tuning).commands[0] == "gold -7500");
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
		EXPECT(base.sockets >= 2 && base.sockets <= 6 && base.quality == D2RL::Items::Quality::Normal);
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

	// With the cow shuffle on, the Cow Level trades its cows too.
	bool cowsMoved = false;
	for (uint32_t seed = 0; seed < 20 && !cowsMoved; ++seed) {
		auto cows = MakeLevels();
		std::mt19937 r(seed);
		ap::randomizer::ShuffleMonsters(cows, noFamilies, ap::randomizer::Shuffle::Anywhere, r, 3, true);
		cowsMoved = lineup(cows, 7) != lineup(original, 7);
	}
	EXPECT(cowsMoved);
}

void TestMonsterShuffleProtectedAreas() {
	const auto original = ap::Table::Parse(
		"Name\tId\tAct\tMonLvlEx\tMonDen\tMonDen(N)\tMonDen(H)\tmon1\tmon2\tmon3\tnmon1\tumon1\r\n"
		"Summit\t120\t4\t30\t0\t100\t100\tancientbarb1\tancientbarb2\tancientbarb3\tancientbarb1\tancientbarb1\r\n"
		"Scripted\t121\t4\t30\t0\t0\t0\tscripted\t\t\tscripted\tscripted\r\n"
		"Empty\t122\t4\t30\t\t\t\tempty\t\t\tempty\tempty\r\n"
		"Normal\t123\t4\t30\t100\t0\t0\tnormal\t\t\tnormal\tnormal\r\n"
		"Nightmare\t90\t4\t30\t0\t100\t0\tnightmare\t\t\tnightmare\tnightmare\r\n"
		"Hell\t91\t4\t30\t0\t0\t100\thell\t\t\thell\thell\r\n");
	const auto noFamilies = ap::Table::Parse("Id\tBaseId\tLevel\r\n");
	for (const auto mode : { ap::randomizer::Shuffle::Nearby, ap::randomizer::Shuffle::Anywhere }) {
		std::set<std::string> seen;
		for (uint32_t seed = 0; seed < 20; ++seed) {
			auto levels = original;
			std::mt19937 random(seed);
			ap::randomizer::ShuffleMonsters(levels, noFamilies, mode, random);
			for (size_t row = 0; row < 3; ++row) {
				EXPECT(levels.Rows()[row] == original.Rows()[row]);
			}
			std::multiset<std::string> lineups;
			seen.insert(levels.Cell(3, 7));
			for (size_t row = 3; row < 6; ++row) {
				lineups.insert(levels.Cell(row, 7));
				EXPECT(levels.Cell(row, 7) == levels.Cell(row, 10));
				EXPECT(levels.Cell(row, 7) == levels.Cell(row, 11));
			}
			EXPECT((lineups == std::multiset<std::string> { "normal", "nightmare", "hell" }));
		}
		EXPECT(seen.size() == 3);
	}
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
	ap::randomizer::ScaleDensity(levels, { 200, 50, 100 }, 300);
	EXPECT(levels.Cell(0, 1) == "0" && levels.Cell(1, 1) == "1600" && levels.Cell(1, 2) == "750");
	EXPECT(levels.Cell(1, 3) == "3" && levels.Cell(1, 4) == "9");
	auto ratio = ap::Table::Parse("Function\tUnique\tUniqueDivisor\tUniqueMin\tRare\tSet\tMagic\tMagicMin\r\nNormal\t400\t2\t6400\t160\t125\t30\t192\r\n");
	ap::randomizer::BoostDrops(ratio, 200);
	EXPECT(ratio.Cell(0, 1) == "200" && ratio.Cell(0, 2) == "2" && ratio.Cell(0, 3) == "3200" && ratio.Cell(0, 4) == "80");
	EXPECT(ratio.Cell(0, 6) == "30" && ratio.Cell(0, 7) == "192");
	// Below 100 makes them rarer; magic items have their own percent.
	ap::randomizer::BoostDrops(ratio, 50, 300);
	EXPECT(ratio.Cell(0, 1) == "400" && ratio.Cell(0, 6) == "10" && ratio.Cell(0, 7) == "64");
}

void TestSuperUniqueMinions() {
	const auto before = ap::Table::Parse("Superunique\tClass\tMinGrp\tMaxGrp\r\nRakanishu\tfallen2\t8\t8\r\nBonebreak\tskeleton1\t5\t5\r\n"
	                                     "Bishibosh\tfallenshaman1\t2\t2\r\nRadament\tradament\t3\t3\r\n");
	auto table = before;
	// Rakanishu (main path) and Bonebreak (optional) got new monsters; Bishibosh kept his.
	table.Set(0, 1, "cr_archer1");
	table.Set(1, 1, "fallen2");
	EXPECT(ap::randomizer::ScaleSuperUniqueMinions(table, before, 100, 4) == 1);
	EXPECT(table.Cell(0, 2) == "4" && table.Cell(0, 3) == "4" && table.Cell(1, 2) == "5" && table.Cell(2, 2) == "2");
	auto scaled = before;
	EXPECT(ap::randomizer::ScaleSuperUniqueMinions(scaled, before, 150, -1) == 3);
	// Quest bosses keep theirs.
	EXPECT(scaled.Cell(0, 2) == "12" && scaled.Cell(1, 3) == "8" && scaled.Cell(2, 2) == "3" && scaled.Cell(3, 2) == "3");
	auto none = before;
	ap::randomizer::ScaleSuperUniqueMinions(none, before, 0, -1);
	EXPECT(none.Cell(0, 2) == "0" && none.Cell(0, 3) == "0");
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
	// The main-path cap: Bishibosh and Griswold never get a stronger monster than
	// their own, so Leatherarm's mummy goes to an optional slot (Coldcrow or Leatherarm).
	const auto capped = ap::Table::Parse("Id\tLevel\r\nfallenshaman1\t2\r\ncr_archer1\t4\r\ngriswold\t5\r\nmummy2\t15\r\n");
	bool moved = false;
	for (uint32_t seed = 0; seed < 50; ++seed) {
		auto table = ap::Table::Parse("Superunique\tName\tClass\r\nBishibosh\tBishibosh\tfallenshaman1\r\nColdcrow\tColdcrow\tcr_archer1\r\n"
		                              "Griswold\tGriswold\tgriswold\r\nLeatherarm\tLeatherarm\tmummy2\r\n");
		std::mt19937 seeded(seed);
		ap::randomizer::ShuffleSuperUniques(table, capped, ap::randomizer::Shuffle::Anywhere, seeded, 0);
		EXPECT(table.Cell(0, 2) == "fallenshaman1");
		EXPECT(table.Cell(2, 2) != "mummy2");
		moved = moved || table.Cell(2, 2) != "griswold";
		std::multiset<std::string> all { table.Cell(0, 2), table.Cell(1, 2), table.Cell(2, 2), table.Cell(3, 2) };
		EXPECT((all == std::multiset<std::string> { "fallenshaman1", "cr_archer1", "griswold", "mummy2" }));
	}
	// Weaker swaps still happen.
	EXPECT(moved);
	ap::randomizer::Settings settings;
	settings.superUniques = ap::randomizer::Shuffle::Nearby;
	EXPECT(settings.Stamp().find("superunique_cap") == std::string::npos);
	settings.mainPathSuperUniqueCap = 2;
	EXPECT(settings.Stamp().find("superunique_cap=2") != std::string::npos);
	settings.mainPathSuperUniqueMinions = 4;
	settings.superUniqueMinions = 50;
	settings.xpPercent = { 100, 200, 300 };
	EXPECT(settings.Stamp().find("superunique_minion_cap=4") != std::string::npos && settings.Stamp().find("superunique_minions=50") != std::string::npos);
	EXPECT(settings.Stamp().find("xp=100/200/300") != std::string::npos);
	settings.xpPercent = { 150, 150, 150 };
	EXPECT(settings.Stamp().find("xp=150 ") != std::string::npos);
	// Tuning for a shuffle that's off leaves the stamp alone.
	ap::randomizer::Settings quiet;
	quiet.shuffleTolerance = 9;
	quiet.runeSpread = 5;
	EXPECT(quiet.Stamp() == ap::randomizer::Settings {}.Stamp() && !quiet.Any());
}

void TestRunewordShuffle() {
	auto runes = ap::Table::Parse("Name\t*Rune Name\tcomplete\tRune1\tRune2\tRune3\tRune4\tRune5\tRune6\r\n"
	                              "Runeword1\tSpirit\t1\tr07\tr10\tr11\tr09\t\t\r\nRuneword2\tOff\t0\tr01\t\t\t\t\t\r\nRuneword3\tZod\t1\tr33\t\t\t\t\t\r\n");
	std::mt19937 random(9);
	const auto recipes = ap::randomizer::ShuffleRunewords(runes, random);
	EXPECT(recipes.size() == 2 && recipes[0].rfind("Spirit:", 0) == 0);
	// Pinned: the roll uses only std::mt19937's raw numbers, which every standard library gives
	// alike, so a setup tool built anywhere deals a seed the same way.
	EXPECT(recipes[0] == "Spirit: Eth Amn Shael Ort" && recipes[1] == "Zod: Zod");
	const int original[] = { 7, 10, 11, 9 };
	for (int i = 0; i < 4; ++i) {
		const int tier = std::stoi(runes.Cell(0, 3 + i).substr(1));
		EXPECT(tier >= original[i] - 2 && tier <= original[i] + 2);
	}
	EXPECT(runes.Cell(1, 3) == "r01");
	EXPECT(std::stoi(runes.Cell(2, 3).substr(1)) >= 31);
	// A spread of 0 keeps every recipe.
	std::mt19937 still(9);
	const auto before = runes.Format();
	ap::randomizer::ShuffleRunewords(runes, still, 0);
	EXPECT(runes.Format() == before);
	// Words that start alike never end with one recipe (the game would only make the first): five
	// one-rune words around Ral, with five runes in reach, get five different runes.
	std::string text = "Name\t*Rune Name\tcomplete\tRune1\tRune2\tRune3\tRune4\tRune5\tRune6\r\n";
	for (int i = 0; i < 5; ++i) {
		text += "Runeword" + std::to_string(i) + "\tWord" + std::to_string(i) + "\t1\tr08\t\t\t\t\t\r\n";
	}
	for (uint32_t seed = 0; seed < 20; ++seed) {
		auto same = ap::Table::Parse(text);
		std::mt19937 rolls(seed);
		ap::randomizer::ShuffleRunewords(same, rolls);
		std::set<std::string> distinct;
		for (size_t row = 0; row < 5; ++row) {
			distinct.insert(same.Cell(row, 3));
		}
		EXPECT(distinct.size() == 5);
	}
	// Two banks with different rows (RotW has words Classic/LoD lacks, and the rows sit elsewhere)
	// give every word they share one recipe. Four shared one-rune words around Ral and one only
	// Classic/LoD has fill the five runes in reach, so that one must dodge the shared ones too.
	const std::string header = "Name\t*Rune Name\tcomplete\tRune1\tRune2\tRune3\tRune4\tRune5\tRune6\r\n";
	std::string rotwText = header + "Runeword1\tSpirit\t1\tr07\tr10\tr11\tr09\t\t\r\nRuneword2\tSteel\t1\tr03\tr01\t\t\t\t\r\n"
	                     + "Runeword3\tCoven\t1\tr12\tr04\tr05\t\t\t\r\nRuneword4\tChanged\t1\tr15\tr16\t\t\t\t\r\n";
	std::string baseText = header + "Runeword2\tSteel\t1\tr03\tr01\t\t\t\t\r\nRuneword9\tOld\t1\tr08\t\t\t\t\t\r\n"
	                     + "Runeword4\tChanged\t1\tr20\tr21\t\t\t\t\r\nRuneword1\tSpirit\t1\tr07\tr10\tr11\tr09\t\t\r\n";
	for (int i = 0; i < 4; ++i) {
		const std::string row = "Word" + std::to_string(i) + "\tWord" + std::to_string(i) + "\t1\tr08\t\t\t\t\t\r\n";
		rotwText += row;
		baseText = header + row + baseText.substr(header.size());
	}
	const auto sequence = [](const ap::Table& table, const std::string& name) {
		for (size_t row = 0; row < table.Rows().size(); ++row) {
			if (table.Cell(row, 0) == name) {
				std::string runes;
				for (int slot = 3; slot < 9; ++slot) {
					runes += table.Cell(row, slot);
				}
				return runes;
			}
		}
		return std::string();
	};
	std::vector<std::string> pinned;
	for (uint32_t seed = 0; seed < 20; ++seed) {
		auto rotw = ap::Table::Parse(rotwText);
		auto base = ap::Table::Parse(baseText);
		std::mt19937 rolls(seed);
		const auto dealt = ap::randomizer::ShuffleRunewords(std::vector<ap::Table*> { &rotw, &base }, rolls);
		for (const char* name : { "Runeword1", "Runeword2", "Word0", "Word1", "Word2", "Word3" }) {
			EXPECT(!sequence(rotw, name).empty() && sequence(rotw, name) == sequence(base, name));
		}
		for (const ap::Table* table : { &rotw, &base }) {
			std::set<std::string> distinct;
			for (size_t row = 0; row < table->Rows().size(); ++row) {
				distinct.insert(sequence(*table, table->Cell(row, 0)));
			}
			EXPECT(distinct.size() == table->Rows().size());
		}
		// Shared words list once for both banks; Coven (RotW only), Old (Classic/LoD only) and
		// Changed (other runes in each bank) list per bank.
		std::map<std::string, std::vector<std::vector<size_t>>> banks;
		for (const auto& recipe : dealt) {
			banks[recipe.text.substr(0, recipe.text.find(':'))].push_back(recipe.banks);
		}
		EXPECT(dealt.size() == 10);
		EXPECT((banks["Spirit"] == std::vector<std::vector<size_t>> { { 0, 1 } } && banks["Word3"] == std::vector<std::vector<size_t>> { { 0, 1 } }));
		EXPECT((banks["Coven"] == std::vector<std::vector<size_t>> { { 0 } } && banks["Old"] == std::vector<std::vector<size_t>> { { 1 } }));
		EXPECT((banks["Changed"] == std::vector<std::vector<size_t>> { { 0 }, { 1 } }));
		EXPECT(std::stoi(sequence(base, "Runeword4").substr(1, 2)) >= 18);
		// The same seed deals the same recipes.
		auto rotwAgain = ap::Table::Parse(rotwText);
		auto baseAgain = ap::Table::Parse(baseText);
		std::mt19937 again(seed);
		const auto repeat = ap::randomizer::ShuffleRunewords(std::vector<ap::Table*> { &rotwAgain, &baseAgain }, again);
		EXPECT(rotwAgain.Format() == rotw.Format() && baseAgain.Format() == base.Format());
		EXPECT(repeat.size() == dealt.size() && std::ranges::equal(repeat, dealt, {}, &ap::randomizer::RunewordRecipe::text, &ap::randomizer::RunewordRecipe::text));
		pinned.push_back(rotw.Format() + base.Format());
	}
	// Different seeds deal differently.
	EXPECT(std::set<std::string>(pinned.begin(), pinned.end()).size() > 1);
}

void TestSuperUniqueMonsters() {
	auto monstats = ap::Table::Parse("Id\t*hcIdx\tminion1\tLevel\r\nfallenshaman1\t0\tfallen1\t2\r\nzombie1\t1\t\t1\r\nExpansion\t\t\t\r\n"
	                                 "snowyeti4\t2\t\t40\r\n");
	auto superUniques = ap::Table::Parse("Superunique\tName\tClass\r\nBishibosh\tBishibosh\tfallenshaman1\r\nRadament\tRadament\tradament\r\n"
	                                     "Corpsefire\tCorpsefire\tzombie1\r\n");
	const size_t before = monstats.Rows().size();
	std::vector<std::pair<std::string, std::string>> copies;
	EXPECT(ap::randomizer::AddSuperUniqueMonsters(monstats, superUniques, &copies) == 2);
	// The linked copies and the monsters they copy, for their HD models.
	EXPECT(copies.size() == 2 && copies[0].first == "ap_superunique_0" && copies[0].second == "fallenshaman1");
	EXPECT(copies.size() == 2 && copies[1].first == "ap_superunique_40" && copies[1].second == "zombie1");
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
	EXPECT(settings.Any() && settings.Stamp().find("superunique_checks=31") != std::string::npos);
	EXPECT(ap::randomizer::Settings {}.Stamp().find("superunique_checks") == std::string::npos);
}

void TestMainPathSuperUniqueTuning() {
	// Griswold (main path) became a mummy; Bonebreak (optional) a fallen.
	auto monstats = ap::Table::Parse("Id\tminion1\tLevel\tminHP\tmaxHP\tMinHP(N)\tMaxHP(H)\tA1MinD\tA1MaxD(H)\tEl1MaxD\r\n"
	                                 "griswold\t\t5\t40\t50\t300\t900\t10\t60\t\r\nmummy2\t\t15\t100\t120\t500\t1500\t20\t80\t8\r\n"
	                                 "skeleton1\t\t2\t10\t12\t80\t240\t2\t9\t\r\nfallen2\t\t5\t8\t10\t60\t180\t2\t8\t\r\n");
	const auto before = ap::Table::Parse("Superunique\tClass\r\nGriswold\tgriswold\r\nBonebreak\tskeleton1\r\n");
	auto superUniques = ap::Table::Parse("Superunique\tClass\r\nGriswold\tmummy2\r\nBonebreak\tfallen2\r\n");
	ap::randomizer::AddSuperUniqueMonsters(monstats, superUniques);
	auto rowOf = [&](const std::string& id) {
		for (size_t row = 0; row < monstats.Rows().size(); ++row) {
			if (monstats.Cell(row, 0) == id) {
				return row;
			}
		}
		return size_t { 0 };
	};
	EXPECT(ap::randomizer::TuneMainPathSuperUniques(monstats, before, 50, 1) >= 1);
	const size_t griswold = rowOf("ap_superunique_5"), bonebreak = rowOf("ap_superunique_1");
	// Griswold: his own level (5) plus 1, half the mummy's HP and damage on every difficulty.
	EXPECT(monstats.Cell(griswold, 2) == "6" && monstats.Cell(griswold, 3) == "50" && monstats.Cell(griswold, 4) == "60");
	EXPECT(monstats.Cell(griswold, 5) == "250" && monstats.Cell(griswold, 6) == "750");
	EXPECT(monstats.Cell(griswold, 7) == "10" && monstats.Cell(griswold, 8) == "40" && monstats.Cell(griswold, 9) == "4");
	// Bonebreak is optional, and the plain monsters are untouched.
	EXPECT(monstats.Cell(bonebreak, 2) == "5" && monstats.Cell(bonebreak, 3) == "8");
	EXPECT(monstats.Cell(rowOf("mummy2"), 2) == "15" && monstats.Cell(rowOf("mummy2"), 3) == "100");

	// Without a level, Normal keeps the new monster's.
	auto kept = ap::Table::Parse("Id\tminion1\tLevel\tminHP\r\ngriswold\t\t5\t40\r\nmummy2\t\t15\t100\r\n");
	auto table = ap::Table::Parse("Superunique\tClass\r\nGriswold\tmummy2\r\n");
	ap::randomizer::AddSuperUniqueMonsters(kept, table);
	ap::randomizer::TuneMainPathSuperUniques(kept, before, 100, std::nullopt);
	bool keptLevel = false;
	for (size_t row = 0; row < kept.Rows().size(); ++row) {
		keptLevel = keptLevel || (kept.Cell(row, 0) == "ap_superunique_5" && kept.Cell(row, 2) == "15");
	}
	EXPECT(keptLevel);

	ap::randomizer::Settings settings;
	EXPECT(!settings.SuperUniqueCopies());
	settings.mainPathSuperUniqueLevel = 0;
	// Level 0 without a shuffle changes nothing, so no copies.
	EXPECT(!settings.SuperUniqueCopies());
	settings.superUniques = ap::randomizer::Shuffle::Nearby;
	EXPECT(settings.SuperUniqueCopies() && settings.Stamp().find("superunique_level=0") != std::string::npos);
	ap::randomizer::Settings strong;
	strong.mainPathSuperUniqueStrength = 70;
	EXPECT(strong.SuperUniqueCopies() && strong.Any() && strong.Stamp().find("superunique_strength=70") != std::string::npos);
}

void TestSprite() {
	// Two 2x2 frames: the first all 1s, the second all 2s.
	auto make = [](uint8_t first, uint8_t second) {
		std::string bytes(40, '\0');
		bytes.replace(0, 4, "SpA1");
		const uint32_t header[] = { 4, 2, 0, 2 };
		std::memcpy(bytes.data() + 8, header, sizeof(header));
		for (int y = 0; y < 2; ++y) {
			bytes += std::string(8, static_cast<char>(first)) + std::string(8, static_cast<char>(second));
		}
		return bytes;
	};
	const auto source = ap::sprite::Parse(make(1, 2));
	auto target = ap::sprite::Parse(make(7, 8));
	EXPECT(source && target && source->FrameWidth() == 2 && source->height == 2);
	EXPECT(ap::sprite::CopyFrame(*source, 1, *target, 0));
	EXPECT(target->bytes.substr(40, 16) == std::string(8, '\2') + std::string(8, '\b'));
	EXPECT(target->bytes.substr(56, 16) == std::string(8, '\2') + std::string(8, '\b'));
	EXPECT(!ap::sprite::CopyFrame(*source, 2, *target, 0) && !ap::sprite::Parse("SpA0"));

	// A 40x40 sheet: a header line, a 10x10 box and a 2 pixel wide "arrow".
	std::string art(40, '\0');
	art.replace(0, 4, "SpA1");
	const uint32_t size[] = { 40, 40, 0, 1 };
	std::memcpy(art.data() + 8, size, sizeof(size));
	art += std::string(40 * 40 * 4, '\0');
	auto paint = [&](int x, int y) { art[40 + static_cast<size_t>(y * 40 + x) * 4 + 3] = '\x7f'; };
	for (int x = 0; x < 30; ++x) {
		paint(x, 1);
	}
	for (int y = 10; y < 20; ++y) {
		for (int x = 5; x < 15; ++x) {
			paint(x, y);
		}
	}
	for (int y = 10; y < 35; ++y) {
		paint(25, y);
		paint(26, y);
	}
	auto sheet = ap::sprite::Parse(art);
	ap::sprite::RemoveThinShapes(*sheet, 5, 6);
	auto at = [&](int x, int y) { return sheet->bytes[40 + static_cast<size_t>(y * 40 + x) * 4 + 3]; };
	EXPECT(at(3, 1) != 0 && at(5, 10) != 0 && at(14, 19) != 0 && at(10, 15) != 0);
	EXPECT(at(25, 15) == 0 && at(26, 30) == 0);
}

void TestSkillTreeShuffle() {
	// Warmth has a synergy with Fire Bolt.
	const auto skills = ap::Table::Parse("skill\t*Id\tcharclass\tskilldesc\treqlevel\tcalc1\r\nAttack\t0\t\tattack\t\t\r\n"
	                                     "Fire Bolt\t36\tsor\tfire bolt\t1\t\r\nWarmth\t37\tsor\twarmth\t1\tskill('Fire Bolt'.blvl)*par8\r\n"
	                                     "Inferno\t41\tsor\tinferno\t6\t\r\nBash\t126\tbar\tbash\t1\t\r\nHowl\t130\tbar\thowl\t1\t\r\n"
	                                     "Leap\t132\tbar\tleap\t6\t\r\n");
	const auto desc = ap::Table::Parse("skilldesc\tSkillPage\tSkillRow\tSkillColumn\tListRow\tIconCel\tstr name\r\nattack\t0\t0\t0\t0\t0\t\r\n"
	                                   "fire bolt\t1\t1\t2\t1\t0\tskillname36\r\nwarmth\t1\t1\t3\t1\t2\tskillname37\r\n"
	                                   "inferno\t1\t2\t1\t1\t4\tskillname41\r\nbash\t1\t1\t2\t1\t0\tskillname126\r\n"
	                                   "howl\t3\t1\t1\t3\t2\tskillname130\r\nleap\t1\t2\t1\t1\t4\tskillname132\r\n");
	auto s = skills;
	auto d = desc;
	// Warmth and Bash trade slots, and so do Inferno and Leap.
	const ap::randomizer::SkillTree tree = { { 37, 126 }, { 126, 37 }, { 41, 132 }, { 132, 41 } };
	const auto shuffle = ap::randomizer::ApplySkillTrees(s, d, tree);
	EXPECT(shuffle.moved == 4 && shuffle.icons.size() == 4);
	// Warmth now sits in Bash's slot: Barbarian, tree 1, row 1, column 2, icon frame 0.
	EXPECT(s.Cell(2, 2) == "bar" && s.Cell(2, 4) == "1" && d.Cell(2, 1) == "1" && d.Cell(2, 3) == "2" && d.Cell(2, 5) == "0");
	EXPECT(s.Cell(4, 2) == "sor" && d.Cell(4, 3) == "3" && d.Cell(4, 5) == "2");
	EXPECT(s.Cell(3, 2) == "bar" && s.Cell(3, 4) == "6" && s.Cell(6, 2) == "sor");
	// Fire Bolt and Howl stay; Warmth's icon comes from the Sorceress sheet's frame 2.
	EXPECT(s.Cell(1, 2) == "sor" && s.Cell(5, 2) == "bar" && s.Cell(0, 2).empty());
	const auto warmth = std::ranges::find_if(shuffle.icons, [](const auto& icon) { return icon.toClass == "bar" && icon.toCel == 0; });
	EXPECT(warmth != shuffle.icons.end() && warmth->fromClass == "sor" && warmth->fromCel == 2);
	// Fire Bolt is out of Warmth's new tree, so the synergy moves to the Barbarian skill nearest
	// Fire Bolt's old place on Warmth's tab: Inferno, now in Leap's slot.
	EXPECT(s.Cell(2, 5) == "skill('Inferno'.blvl)*par8");
	// No deal, no change.
	auto unchanged = skills;
	auto unchangedDesc = desc;
	EXPECT(ap::randomizer::ApplySkillTrees(unchanged, unchangedDesc, {}).moved == 0 && unchanged.Format() == skills.Format());
}

void TestShopParcels() {
	auto misc = ap::Table::Parse("name\tcode\tcost\tAkaraMin\tAkaraMax\tMalahMin\tMalahMax\tspawnable\r\nStandard of Heroes\tstd\t1\t\t\t\t\t1\r\n");
	EXPECT(ap::randomizer::AddShopParcels(misc) == 5);
	EXPECT(ap::randomizer::AddShopParcels(misc) == 0);
	EXPECT(misc.Rows().size() == 6);
	EXPECT(misc.Cell(1, 1) == "ap1" && misc.Cell(1, 2) == "2500" && misc.Cell(1, 3) == "1" && misc.Cell(1, 5).empty() && misc.Cell(1, 7) == "1");
	EXPECT(misc.Cell(5, 1) == "ap5" && misc.Cell(5, 2) == "40000" && misc.Cell(5, 3).empty() && misc.Cell(5, 5) == "1");
	EXPECT(ap::randomizer::AddProphecyScrolls(misc) == 8 && ap::randomizer::AddProphecyScrolls(misc) == 0);
	// The Writ of Absolution: a Token of Absolution copy (same use), at every potion seller.
	auto writ = ap::Table::Parse("name\tcode\ttype\tcost\tpSpell\tAkaraMin\tMalahMax\tspawnable\r\nToken of Absolution\ttoa\tques\t99999\t13\t\t\t0\r\n");
	EXPECT(ap::randomizer::AddRespecWrit(writ) == 1 && ap::randomizer::AddRespecWrit(writ) == 0);
	EXPECT(writ.Cell(1, 0) == "Writ of Absolution" && writ.Cell(1, 1) == "apr" && writ.Cell(1, 2) == ap::randomizer::ShopItemType && writ.Cell(1, 3) == "1000"
	       && writ.Cell(1, 4) == "13" && writ.Cell(1, 5) == "1" && writ.Cell(1, 6) == "1" && writ.Cell(1, 7) == "1");
	// The real token is untouched, so finding one stays a check.
	EXPECT(writ.Cell(0, 2) == "ques" && writ.Cell(0, 3) == "99999" && writ.Cell(0, 7) == "0");
	// Every tier at every potion seller, the price rising.
	EXPECT(misc.Cell(6, 0) == "Scroll of Prophecy" && misc.Cell(6, 1) == "aq1" && misc.Cell(6, 2) == "1000" && misc.Cell(6, 3) == "1" && misc.Cell(6, 5) == "1");
	EXPECT(misc.Cell(13, 1) == "aq8" && misc.Cell(13, 2) == "100000");
	// Scrolls of Prophecy take the Scroll of Inifuss' size and picture, to match its HD picture.
	auto sized = ap::Table::Parse("name\tcode\tinvwidth\tinvheight\tinvfile\r\nStandard of Heroes\tstd\t1\t1\tinvflag\r\n"
	                              "Scroll of Inifuss\tbks\t2\t2\tinvscb\r\n");
	EXPECT(ap::randomizer::AddProphecyScrolls(sized) == 8);
	EXPECT(sized.Cell(2, 1) == "aq1" && sized.Cell(2, 2) == "2" && sized.Cell(2, 3) == "2" && sized.Cell(2, 4) == "invscb");
	// The shop items get a quest-like type that vendors stock on the Misc page.
	auto types = ap::Table::Parse(std::string("ItemType\tCode\tStorePage\r\nQuest\tques\t\r\n"));
	EXPECT(ap::randomizer::AddShopItemType(types) == 1 && ap::randomizer::AddShopItemType(types) == 0);
	EXPECT(types.Cell(1, 1) == ap::randomizer::ShopItemType && types.Cell(1, 2) == "misc");
	// Classic/LoD gets a placeholder for each RotW item it lacks, before the added items.
	auto lod = ap::Table::Parse("name\tcode\tspawnable\r\nStandard of Heroes\tstd\t1\r\nScroll of Identify\tisc\t1\r\n");
	EXPECT(ap::randomizer::PadMiscRows(lod, { "std", "isc", "cs2" }) == 1 && ap::randomizer::PadMiscRows(lod, { "std", "isc", "cs2" }) == 0);
	EXPECT(lod.Rows().size() == 3 && lod.Cell(2, 1) == "cs2" && lod.Cell(2, 2) == "0");
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
	ap::randomizer::ScaleExperience(monlvl, { 250, 250, 250 });
	EXPECT(monlvl.Cell(0, 1) == "2" && monlvl.Cell(0, 2) == "250" && monlvl.Cell(0, 3) == "2500");
	ap::randomizer::ScaleExperience(monlvl, { 100, 200, 50 });
	EXPECT(monlvl.Cell(0, 1) == "2" && monlvl.Cell(0, 2) == "500" && monlvl.Cell(0, 3) == "1250");

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

void TestPersonalization() {
	using namespace ap::personal;
	EXPECT(Name("Bob") == "Bob");
	EXPECT(Name("  Sir Bob the 3rd!  ") == "Sir Bob the 3rd");
	EXPECT(Name("Zoë_42") == "Zo_42");
	EXPECT(Name("AVeryLongPlayerNameIndeed") == "AVeryLongPlayer");
	EXPECT(Name("Bob  -  X") == "Bob X");
	EXPECT(Name("日本").empty());

	std::vector<uint8_t> data(DataSize, 0);
	const auto put32 = [&](size_t offset, uint32_t value) { std::memcpy(data.data() + offset, &value, sizeof(value)); };
	put32(DataQuality, 7);
	put32(DataFlags, FlagIdentified | 0x00200000U);
	EXPECT(CheckLayout(data, 7, true, false).empty());
	EXPECT(!CheckLayout(data, 5, true, false).empty());
	EXPECT(!CheckLayout(data, 7, false, false).empty());
	EXPECT(!CheckLayout(data, 7, true, true).empty());
	data[DataName + 3] = 0x41;
	EXPECT(!CheckLayout(data, 7, true, false).empty());
	std::memcpy(data.data() + DataName, "Bob", 4);
	put32(DataFlags, FlagIdentified | FlagPersonalized);
	EXPECT(CheckLayout(data, 7, true, false).empty());
	std::memset(data.data() + DataName, 'x', NameSize);
	EXPECT(!CheckLayout(data, 7, true, false).empty());
	EXPECT(!CheckLayout(std::span(data).first(0x20), 7, true, false).empty());
	EXPECT(HexDump(std::span(data).first(18)).starts_with("0000: 07 00 00 00"));
	EXPECT(HexDump(std::span(data).first(18)).find("\n0010: ") != std::string::npos);
}

void TestDeferredPanelScope() {
	const ap::panel::SessionScope confirmed { 1, 2, 3, 0, 0, true };
	EXPECT(ap::panel::SameReadyScope(confirmed, confirmed));
	for (int change = 0; change < 6; ++change) {
		auto current = confirmed;
		switch (change) {
			case 0: ++current.target; break;
			case 1: ++current.session; break;
			case 2: ++current.game; break;
			case 3: ++current.playerClass; break;
			case 4: ++current.seedClass; break;
			case 5: current.ready = false; break;
		}
		EXPECT(!ap::panel::SameReadyScope(confirmed, current));
	}
	auto disconnected = confirmed;
	disconnected.ready = false;
	EXPECT(!ap::panel::SameReadyScope(disconnected, disconnected));
}

void TestCommandPanel() {
	std::set<std::string> ids;
	std::vector<const ap::panel::Panel*> panels { &ap::panel::Main(), &ap::panel::Debug(), &ap::panel::Repairs() };
	for (const auto& dialog : ap::panel::Confirmations()) {
		panels.push_back(&dialog);
	}
	for (const auto* panel : panels) {
		const auto layout = nlohmann::json::parse(ap::panel::Layout(*panel));
		EXPECT(layout["type"] == "Panel" && layout["name"] == panel->name);
		if (panel->confirmation) {
			EXPECT(layout["fields"]["defaultWidget"] == "Close");
		}
		std::set<std::string> names;
		std::set<std::string> messages;
		for (const auto& child : layout["children"]) {
			EXPECT(names.insert(child["name"].get<std::string>()).second);
			if (child["type"] == "ButtonWidget") {
				messages.insert(child["fields"]["onClickMessage"].get<std::string>());
			}
		}
		// Every button runs through Find, ids are unique across the panels, and Close closes the panel.
		size_t count = 0;
		for (const auto& column : panel->columns) {
			for (const auto& button : column.buttons) {
				++count;
				EXPECT(ids.insert(std::string(button.id)).second);
				EXPECT(ap::panel::Find(*panel, button.id) == &button);
				if (button.confirm) {
					const auto* dialog = ap::panel::ConfirmationFor(button.id);
					EXPECT(dialog != nullptr && dialog->confirmation);
					EXPECT(dialog->columns[0].buttons[0].action == button.action);
				}
				EXPECT(messages.contains(ap::panel::ClickMessage(button.id)));
				EXPECT(ap::panel::ClickedButton("PanelManager", "OpenPanel", ap::panel::ClickMessage(button.id).substr(23)) == button.id);
				const auto parts = ap::panel::SplitAction(button.action);
				EXPECT(!parts.empty());
				for (const auto part : parts) {
					EXPECT(part.starts_with('!') || (part.starts_with("ap-") && parts.size() == 1));
				}
			}
		}
		EXPECT(messages.contains(std::string("PanelManager:ClosePanel:") + panel->name));
		EXPECT(messages.size() == count + 1);
		EXPECT(ap::panel::Find(*panel, "nothing") == nullptr);
		EXPECT(std::string(panel->name) == std::string("archipelago/") + panel->localId);
	}
	// The debug panel fits on screen at five columns of twelve; more need checking in game.
	EXPECT(ap::panel::Debug().columns.size() <= 5);
	for (const auto& column : ap::panel::Debug().columns) {
		EXPECT(column.buttons.size() <= 12);
	}
	// Confirmation IDs cannot resolve as ordinary commands on the main panel.
	for (const auto& dialog : ap::panel::Confirmations()) {
		EXPECT(ap::panel::Find(ap::panel::Main(), dialog.columns[0].buttons[0].id) == nullptr);
	}
	EXPECT(ap::panel::ConfirmationFor("status") == nullptr);
	EXPECT(ap::panel::ConfirmationFor("confirm-collect") == nullptr);
	// Debug buttons only run from the debug panel.
	EXPECT(ap::panel::Find(ap::panel::Main(), "debug-god") == nullptr);
	EXPECT(ap::panel::Find(ap::panel::Debug(), "debug-god") != nullptr);
	EXPECT(ap::panel::ClickedButton("PanelManager", "OpenPanel", "\"archipelago-run/status\"") == "status");
	EXPECT(ap::panel::ClickedButton("PanelManager", "OpenPanel", "BankPanel").empty());
	EXPECT(ap::panel::ClickedButton("PanelManager", "ClosePanel", "archipelago-run/status").empty());
	EXPECT((ap::panel::SplitAction("!a; !b c") == std::vector<std::string_view> { "!a", "!b c" }));
}

void TestRepairOffers() {
	using namespace std::chrono;
	using ap::repairs::Offer;
	const auto now = Offer::Clock::time_point {} + seconds(100);
	Offer offer;
	EXPECT(!offer.Take(true, now));
	offer.Begin(now);
	EXPECT(!offer.Ready(false, now));
	EXPECT(offer.Take(true, now + seconds(1)));
	EXPECT(!offer.Take(true, now + seconds(2))); // Double clicks cannot charge twice.
	offer.Begin(now);
	EXPECT(!offer.Take(true, now + seconds(30))); // Expired NPC interaction.
	offer.Begin(now);
	EXPECT(!offer.Take(false, now)); // Death, leaving town, disabling or unlocking.
	EXPECT(!offer.Take(true, now));
	offer.Begin(now);
	offer.Cancel(); // Game/slot changes and closing the modal invalidate it.
	EXPECT(!offer.Ready(true, now));
	EXPECT(ap::repairs::IsVendorPanel("\"VendorPanelLayout\""));
	EXPECT(ap::repairs::IsVendorPanel("VendorPanelLayoutHD"));
	EXPECT(!ap::repairs::IsVendorPanel("archipelago/RepairPanel"));
	EXPECT(!ap::repairs::IsVendorPanel("PlayerInventoryPanel"));
}

int main() {
	TestCommandPanel();
	TestDeferredPanelScope();
	TestRepairOffers();
	TestQuestTriggers();
	TestCodexLevels();
	TestTableRoundTrip();
	TestMonsterShuffle();
	TestMonsterShuffleProtectedAreas();
	TestBalancedAnywhere();
	TestDensityAndDrops();
	TestSuperUniqueMinions();
	TestSuperUniqueShuffle();
	TestSuperUniqueMonsters();
	TestMainPathSuperUniqueTuning();
	TestShopParcels();
	TestSkillTreeShuffle();
	TestSprite();
	TestRunewordShuffle();
	TestTableEdits();
	TestLootScalesToLevel();
	TestOtherLoot();
	TestLootFitsClass();
	TestSocketedBases();
	TestGoldExperienceCharms();
	TestShrinesAndTraps();
	TestClientParsesServerPackets();
	TestPanelConnectionTarget();
	TestClientSurvivesBadPackets();
	TestClientDeathLink();
	TestHintPackets();
	TestClientReconnect();
	TestConfigRoundTrip();
	TestConfigDefaults();
	TestConfigNumericIds();
	TestLoaderSettings();
	TestScaleWav();
	TestSaveStateRoundTrip();
	TestMilestoneRecovery();
	TestLifetimeCows();
	TestSaveStatePendingCommands();
	TestUncleanExit();
	TestSessionLog();
	TestStarterKit();
	TestDebugCounts();
	TestSkillTablePrerequisite();
	TestCharacterCheck();
	TestIdsMatchApworld();
	TestGameData();
	TestPersonalization();
	std::printf(failures == 0 ? "all tests passed\n" : "%d failure(s)\n", failures);
	return failures == 0 ? 0 : 1;
}
