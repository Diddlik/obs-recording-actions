#include "settings.hpp"
#include <obs-module.h>
#include <util/bmem.h>
#include <QSaveFile>

std::filesystem::path configurationPath()
{
	std::unique_ptr<char, decltype(&bfree)> path(obs_module_config_path("settings.json"), bfree);
	return path ? std::filesystem::u8path(path.get()) : std::filesystem::path{};
}

ObsData loadConfiguration()
{
	auto path = configurationPath();
	ObsData data(path.empty() ? nullptr : obs_data_create_from_json_file_safe(path.u8string().c_str(), "bak"),
		     obs_data_release);
	if (!data)
		data.reset(obs_data_create());
	return data;
}

PluginSettings readSettings(obs_data_t *data)
{
	PluginSettings settings;
	for (size_t i = 0; i < settings.targets.size(); ++i) {
		auto key = "target" + std::to_string(i + 1);
		settings.targets[i].alias = obs_data_get_string(data, (key + "Alias").c_str());
		settings.targets[i].directory =
			std::filesystem::u8path(obs_data_get_string(data, (key + "Directory").c_str()));
	}
	settings.enableLogs = obs_data_get_bool(data, "enableLogs");
	settings.geometry = obs_data_get_string(data, "geometry");
	return settings;
}

bool writeConfiguration(obs_data_t *data, const PluginSettings &settings)
{
	for (size_t i = 0; i < settings.targets.size(); ++i) {
		auto key = "target" + std::to_string(i + 1);
		obs_data_set_string(data, (key + "Alias").c_str(), settings.targets[i].alias.c_str());
		obs_data_set_string(data, (key + "Directory").c_str(),
				    settings.targets[i].directory.u8string().c_str());
	}
	obs_data_set_bool(data, "enableLogs", settings.enableLogs);
	obs_data_set_string(data, "geometry", settings.geometry.c_str());
	const auto path = configurationPath();
	if (path.empty())
		return false;
	std::error_code error;
	std::filesystem::create_directories(path.parent_path(), error);
	if (error)
		return false;
	// QSaveFile atomically replaces the JSON without requiring ACL-copy rights,
	// unlike ReplaceFileW used by OBS's safe-save helper on Windows.
	QSaveFile file(QString::fromStdWString(path.native()));
	file.setDirectWriteFallback(false);
	if (!file.open(QIODevice::WriteOnly))
		return false;
	const QByteArray json(obs_data_get_json(data));
	return file.write(json) == json.size() && file.commit();
}
