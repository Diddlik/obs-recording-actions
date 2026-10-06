#pragma once
#include <obs.h>
#include <array>
#include <filesystem>
#include <memory>
#include <string>

using ObsData = std::unique_ptr<obs_data_t, decltype(&obs_data_release)>;
struct MoveTarget {
	std::string alias;
	std::filesystem::path directory;
};
struct PluginSettings {
	std::array<MoveTarget, 2> targets;
	bool enableLogs = false;
	std::string geometry;
};
std::filesystem::path configurationPath();
ObsData loadConfiguration();
PluginSettings readSettings(obs_data_t *data);
bool writeConfiguration(obs_data_t *data, const PluginSettings &settings);
