#include "recording-actions.hpp"
#include "updates.hpp"
#include <QCryptographicHash>
#include <QSslSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <obs-module.h>
#include <util/base.h>
#include <util/bmem.h>
#include <util/config-file.h>
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QThread>
#include <QWidget>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
fs::path root;
QWidget *mainWindow = nullptr;
QAction *toolsAction = nullptr;
config_t *profile = nullptr;
obs_frontend_event_cb eventHandler = nullptr;
void *eventContext = nullptr;
bool recording = false;
unsigned stopRequests = 0, pluginLogs = 0, checks = 0;
std::string lastRecording;
std::map<std::string, std::pair<obs_hotkey_id, std::string>> registeredKeys;
void require(bool value, const char *message)
{
	++checks;
	if (!value)
		throw std::runtime_error(message);
}
void pump(unsigned milliseconds = 60)
{
	for (unsigned i = 0; i < milliseconds / 10; ++i) {
		QApplication::processEvents();
		QThread::msleep(10);
	}
}
void keys()
{
	registeredKeys.clear();
	obs_enum_hotkeys(
		[](void *, obs_hotkey_id id, obs_hotkey_t *key) {
			registeredKeys[obs_hotkey_get_name(key)] = {id, obs_hotkey_get_description(key)};
			return true;
		},
		nullptr);
}
void press(const char *name)
{
	obs_hotkey_trigger_routed_callback(registeredKeys.at(name).first, true);
	obs_hotkey_trigger_routed_callback(registeredKeys.at(name).first, false);
	pump();
}
void stopped(const fs::path &source)
{
	lastRecording = source.u8string();
	recording = false;
	eventHandler(OBS_FRONTEND_EVENT_RECORDING_STOPPED, eventContext);
	pump(200);
}
void write(const fs::path &source)
{
	fs::create_directories(source.parent_path());
	std::ofstream(source) << "fixture";
}
} // namespace

// Simulate only the frontend boundary; persistence, hotkeys, Qt widgets and filesystem
// operations use the real implementation and real libobs in an isolated directory.
extern "C" {
obs_module_t *obs_current_module(void)
{
	return nullptr;
}
const char *obs_module_text(const char *key)
{
	if (std::string(key) == "Hotkey.Move")
		return "Recording Actions: Stop + Move to %1";
	if (std::string(key) == "Target1")
		return "Target 1";
	if (std::string(key) == "Target2")
		return "Target 2";
	return key;
}
char *obs_module_get_config_path(obs_module_t *, const char *file)
{
	return bstrdup((root / file).u8string().c_str());
}
void *obs_frontend_get_main_window(void)
{
	return mainWindow;
}
void *obs_frontend_add_tools_menu_qaction(const char *name)
{
	toolsAction = new QAction(QString::fromUtf8(name), mainWindow);
	return toolsAction;
}
void obs_frontend_add_event_callback(obs_frontend_event_cb callback, void *context)
{
	eventHandler = callback;
	eventContext = context;
}
void obs_frontend_remove_event_callback(obs_frontend_event_cb, void *)
{
	eventHandler = nullptr;
}
bool obs_frontend_recording_active(void)
{
	return recording;
}
void obs_frontend_recording_stop(void)
{
	++stopRequests;
}
char *obs_frontend_get_last_recording(void)
{
	return bstrdup(lastRecording.c_str());
}
config_t *obs_frontend_get_profile_config(void)
{
	return profile;
}
}

int main(int argc, char **argv)
{
	qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &message) {
		std::cerr << message.toStdString() << '\n';
	});
	QApplication application(argc, argv);
	if (application.arguments().contains("--https-smoke")) {
		if (!QSslSocket::supportsSsl()) {
			std::cerr << "TLS backend unavailable\n";
			return 1;
		}
		QNetworkAccessManager network;
		QNetworkRequest request(QUrl("https://api.github.com/repos/Diddlik/obs-recording-actions"));
		request.setHeader(QNetworkRequest::UserAgentHeader, "Recording-Actions-runtime-smoke");
		request.setTransferTimeout(15000);
		auto *reply = network.get(request);
		QEventLoop loop;
		QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
		loop.exec();
		std::cout << "TLS backend: " << QSslSocket::activeBackend().toStdString()
			  << "; HTTPS status: " << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
			  << "; " << reply->errorString().toStdString() << '\n';
		return reply->error() == QNetworkReply::NoError ? 0 : 1;
	}

	QWidget window;
	window.setAttribute(Qt::WA_DontShowOnScreen);
	mainWindow = &window;
	root = fs::current_path() / ("integration-fixtures-" + std::to_string(QCoreApplication::applicationPid()));
	fs::create_directories(root);
	base_set_log_handler(
		[](int, const char *message, va_list, void *) {
			if (std::string(message).find("[Recording Actions]") != std::string::npos)
				++pluginLogs;
		},
		nullptr);
	bool started = obs_startup("en-US", root.u8string().c_str(), nullptr);
	try {
		require(started, "libobs startup");
		require(QSslSocket::supportsSsl(), "HTTPS TLS backend available");
		auto digest = QCryptographicHash::hash("installer fixture", QCryptographicHash::Sha256).toHex();
		auto manifest = digest + "  setup.exe\n";
		require(installerChecksum(manifest, "setup.exe") == digest, "Exact installer checksum selected");
		require(installerChecksum(manifest, "other.exe").isEmpty(), "Missing installer checksum rejected");
		require(installerChecksum(manifest + manifest, "setup.exe").isEmpty(), "Duplicate checksum rejected");
		require(installerChecksum(QByteArray(64, 'z') + "  setup.exe", "setup.exe").isEmpty(),
			"Malformed hash rejected");
		require(installerChecksum("abc  setup.exe", "setup.exe").isEmpty(), "Truncated hash rejected");
		obs_hotkey_enable_callback_rerouting(true);
		require(config_open(&profile, (root / "profile.ini").u8string().c_str(), CONFIG_OPEN_ALWAYS) ==
				CONFIG_SUCCESS,
			"Profile fixture");
		auto config = loadConfiguration();
		PluginSettings initial;
		initial.autoUpdates = false;
		initial.targets[0] = {"Client A", root / "target1"};
		initial.targets[1] = {"", root / "target2"};
		require(writeConfiguration(config.get(), initial), "Initial settings saved");
		{
			RecordingActions manager;
			manager.initialize();
			keys();
			require(registeredKeys.size() == 3, "Exactly three hotkeys");
			require(registeredKeys.at("recording-actions.move1").second.find("Client A") !=
					std::string::npos,
				"Dynamic alias description");
			require(registeredKeys.at("recording-actions.move2").second.find("Target 2") !=
					std::string::npos,
				"Empty alias fallback");
			press("recording-actions.delete");
			require(stopRequests == 0, "No recording does nothing");
			obs_key_combination_t binding{INTERACT_CONTROL_KEY, OBS_KEY_F10};
			obs_hotkey_load_bindings(registeredKeys.at("recording-actions.move1").first, &binding, 1);
			pump();
			auto saved = loadConfiguration();
			std::unique_ptr<obs_data_array_t, decltype(&obs_data_array_release)> array(
				obs_data_get_array(saved.get(), "recording-actions.move1"), obs_data_array_release);
			require(array && obs_data_array_count(array.get()) == 1, "Hotkeys persisted immediately");

			auto source = root / "source" / "sample.mkv";
			write(source);
			recording = true;
			press("recording-actions.move1");
			press("recording-actions.delete");
			require(stopRequests == 1 && fs::exists(source),
				"Double press ignored and file waits for STOPPED");
			stopped(source);
			require(!fs::exists(source) && fs::exists(root / "target1" / source.filename()),
				"Target 1 lifecycle");
			write(source);
			recording = true;
			press("recording-actions.move2");
			stopped(source);
			require(!fs::exists(source) && fs::exists(root / "target2" / source.filename()),
				"Target 2 lifecycle");
			write(source);
			recording = true;
			press("recording-actions.delete");
			stopped(source);
			require(!fs::exists(source), "Delete lifecycle");
			write(source);
			recording = true;
			auto deleteId = registeredKeys.at("recording-actions.delete").first;
			obs_hotkey_trigger_routed_callback(deleteId, true);
			pump();
			stopped(source);
			auto repeatedCount = stopRequests;
			recording = true;
			obs_hotkey_trigger_routed_callback(deleteId, true);
			pump();
			require(stopRequests == repeatedCount, "Held key cannot act on next recording");
			obs_hotkey_trigger_routed_callback(deleteId, false);
			require(pluginLogs == 0, "Logging fully disabled");

			toolsAction->trigger();
			pump();
			auto dialogs = window.findChildren<QDialog *>();
			require(dialogs.size() == 1 && dialogs[0]->isVisible(), "Tools opens modeless settings");
			require(toolsAction->text() == "Recording Actions",
				"Embedded locale repairs missing installed resources");
			for (auto *label : dialogs[0]->findChildren<QLabel *>())
				require(!label->text().startsWith("Settings."), "Settings labels are translated");
			auto *aboutButtons = dialogs[0]->findChild<QDialogButtonBox *>();
			aboutButtons->button(QDialogButtonBox::Help)->click();
			pump();
			auto *about = dialogs[0]->findChild<QDialog *>();
			require(about && about->findChild<QLabel *>()->text().contains("GPL-2.0-or-later"),
				"About contains licenses with missing locale files");
			about->close();
			pump();
			auto edits = dialogs[0]->findChildren<QLineEdit *>();
			require(edits.size() == 4, "Two target rows");
			edits[0]->setText(QString::fromUtf8("Kunde Ü / 日本語"));
			auto *buttons = dialogs[0]->findChild<QDialogButtonBox *>();
			buttons->button(QDialogButtonBox::Save)->click();
			pump();
			keys();
			require(registeredKeys.at("recording-actions.move1").second.find("Kunde Ü / 日本語") !=
					std::string::npos,
				"Unicode alias updates hotkey");
			saved = loadConfiguration();
			require(readSettings(saved.get()).targets[0].alias == "Kunde Ü / 日本語",
				"Unicode alias persists");
			config_set_bool(profile, "Video", "AutoRemux", true);
			recording = true;
			auto count = stopRequests;
			press("recording-actions.delete");
			require(stopRequests == count, "Remux guard prevents destructive race");
			config_set_bool(profile, "Video", "AutoRemux", false);
			toolsAction->trigger();
			pump();
			dialogs = window.findChildren<QDialog *>();
			dialogs[0]->findChild<QCheckBox *>()->setChecked(true);
			dialogs[0]->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
			pump();
			recording = true;
			press("recording-actions.delete");
			stopped(root / "missing.mkv");
			require(pluginLogs > 0, "Logging can be enabled for failure diagnosis");
			manager.shutdown();
			keys();
			require(registeredKeys.empty() && !eventHandler, "Shutdown unregisters callbacks and hotkeys");
		}
		{
			RecordingActions manager;
			manager.initialize();
			keys();
			std::unique_ptr<obs_data_array_t, decltype(&obs_data_array_release)> restored(
				obs_hotkey_save(registeredKeys.at("recording-actions.move1").first),
				obs_data_array_release);
			require(restored && obs_data_array_count(restored.get()) == 1, "Hotkeys restored after reload");
			require(registeredKeys.at("recording-actions.move1").second.find("Kunde Ü / 日本語") !=
					std::string::npos,
				"Alias restored after reload");
			auto pendingFile = root / "pending.mkv";
			write(pendingFile);
			recording = true;
			press("recording-actions.delete");
			eventHandler(OBS_FRONTEND_EVENT_EXIT, eventContext);
			keys();
			require(registeredKeys.empty() && !eventHandler, "EXIT performs cleanup before unload");
			require(fs::exists(pendingFile), "EXIT while stop pending preserves recording");
		}
		config_close(profile);
		obs_shutdown();
		fs::remove_all(root);
		std::cout << checks << " integration checks passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "FAIL: " << error.what() << '\n';
		if (profile)
			config_close(profile);
		if (started)
			obs_shutdown();
		return 1;
	}
}
