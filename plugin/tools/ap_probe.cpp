// Drives ap::Client outside the game, against a real Archipelago server.
//
//   ap_probe <server> <slot> [--password <pw>] [--check <location id>]... [--goal] [--death]
//   ap_probe --replay <file>
//
// Connects, prints the received items, sends the checks, optionally reports
// the goal or a DeathLink death (the slot needs death_link on), then prints
// every server message and exits.

#include "ap_client.h"
#include "ids.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

// Feeds a JSON file holding a list of captured server messages to the client.
int Replay(const char* path) {
	std::ifstream file(path);
	const auto messages = nlohmann::json::parse(file);
	ap::Client client([](const std::string& line) { std::printf("[log] %s\n", line.c_str()); });
	for (const auto& message : messages) {
		std::printf("replaying %s\n", message[0].value("cmd", "?").c_str());
		client.Handle(message.dump());
	}
	std::printf("state %d, %zu items, %zu locations\n", static_cast<int>(client.State()), client.Items().size(), client.LocationCount());
	return 0;
}

int main(int argc, char** argv) {
	if (argc == 3 && std::string(argv[1]) == "--replay") {
		return Replay(argv[2]);
	}
	if (argc < 3) {
		std::fprintf(stderr, "usage: ap_probe <server> <slot> [--password <pw>] [--check <id>]... [--goal] [--death]\n");
		return 2;
	}
	std::string password;
	std::vector<int64_t> checks;
	bool goal = false;
	bool death = false;
	for (int i = 3; i < argc; ++i) {
		const std::string arg = argv[i];
		if (arg == "--password" && i + 1 < argc) {
			password = argv[++i];
		} else if (arg == "--check" && i + 1 < argc) {
			checks.push_back(std::strtoll(argv[++i], nullptr, 10));
		} else if (arg == "--goal") {
			goal = true;
		} else if (arg == "--death") {
			death = true;
		}
	}

	ap::Client client([](const std::string& line) { std::printf("[log] %s\n", line.c_str()); });
	client.Connect(argv[1], argv[2], password);
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
	while (client.State() != ap::ConnectionState::Connected && client.State() != ap::ConnectionState::Refused) {
		if (std::chrono::steady_clock::now() > deadline) {
			std::printf("timed out: %s\n", client.StatusText().c_str());
			return 1;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	std::printf("status: %s\n", client.StatusText().c_str());
	if (client.State() == ap::ConnectionState::Refused) {
		return 1;
	}
	std::this_thread::sleep_for(std::chrono::seconds(2));

	const ap::SlotData slots = client.Slots();
	std::printf("seed %s, goal %d (quest %d), level checks every %d up to %d\n", client.Seed().c_str(), slots.goal, slots.goalQuest, slots.levelStep, slots.levelCap);
	std::printf("checked %zu of %zu locations\n", client.Checked().size(), client.LocationCount());
	for (const auto& item : client.Items()) {
		std::printf("item %lld (%s) from player %d at location %lld%s\n", static_cast<long long>(item.item), client.ItemName(item.item, 0).c_str(), item.player,
			static_cast<long long>(item.location), ap::ids::IsSkillItem(item.item) ? " [skill]" : "");
	}

	client.SendChecks(checks);
	if (goal) {
		client.SendGoal();
	}
	if (death) {
		if (!slots.deathLink) {
			std::puts("this slot doesn't have death_link on; no death sent");
		}
		client.SendDeath(std::string(argv[2]) + " was struck down by ap_probe.");
	}
	std::this_thread::sleep_for(std::chrono::seconds(3));
	for (const auto& message : client.TakeMessages()) {
		std::printf("message: %s\n", message.c_str());
	}
	std::printf("checked %zu of %zu locations after sending\n", client.Checked().size(), client.LocationCount());
	return 0;
}
