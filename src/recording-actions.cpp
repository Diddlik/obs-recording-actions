#include "recording-actions.hpp"
#include "version.hpp"
#include <obs-module.h>
#include <util/bmem.h>
#include <util/config-file.h>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <windows.h>

namespace {
QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}
constexpr std::array<const char *, 3> keys = {"recording-actions.move1", "recording-actions.move2",
					      "recording-actions.delete"};
} // namespace

RecordingActions::RecordingActions()
{
	timer.setInterval(50);
	connect(&timer, &QTimer::timeout, this, &RecordingActions::completed);
}

RecordingActions::~RecordingActions()
{
	shutdown();
}

void RecordingActions::initialize()
{
	data = loadConfiguration();
	settings = readSettings(data.get());
	registered = true;
	for (size_t i = 0; i < hotkeys.size(); ++i) {
		auto &key = hotkeys[i];
		key.owner = this;
		key.action = static_cast<PendingAction>(i + 1);
		key.id = obs_hotkey_register_frontend(keys[i], obs_module_text("Plugin.Name"), hotkeyCallback, &key);
		std::unique_ptr<obs_data_array_t, decltype(&obs_data_array_release)> bindings(
			obs_data_get_array(data.get(), keys[i]), obs_data_array_release);
		if (bindings)
			obs_hotkey_load(key.id, bindings.get());
	}
	updateDescriptions();
	obs_frontend_add_event_callback(eventCallback, this);
	signal_handler_connect(obs_get_signal_handler(), "hotkey_bindings_changed", bindingsChanged, this);
	menu = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(obs_module_text("Plugin.Name")));
	connect(menu, &QAction::triggered, this, &RecordingActions::showSettings);
}

void RecordingActions::shutdown()
{
	if (exiting.exchange(true))
		return;
	timer.stop();
	cancellation.cancel();
	if (workerThread.joinable()) {
		CancelSynchronousIo(workerThread.native_handle());
		workerThread.join();
	}
	if (registered) {
		save();
		obs_frontend_remove_event_callback(eventCallback, this);
		signal_handler_disconnect(obs_get_signal_handler(), "hotkey_bindings_changed", bindingsChanged, this);
		for (auto &key : hotkeys)
			if (key.id != OBS_INVALID_HOTKEY_ID)
				obs_hotkey_unregister(key.id);
		registered = false;
	}
	if (dialog)
		delete dialog.data();
	if (menu)
		delete menu.data();
	state.clear();
}

void RecordingActions::log(int level, const char *message)
{
	if (settings.enableLogs)
		blog(level, "[Recording Actions] %s", message);
}

void RecordingActions::hotkeyCallback(void *context, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	auto &key = *static_cast<Hotkey *>(context);
	bool wasDown = key.down.exchange(pressed);
	if (!pressed || wasDown || key.owner->exiting)
		return;
	QMetaObject::invokeMethod(
		key.owner, [owner = key.owner, action = key.action] { owner->start(action); }, Qt::QueuedConnection);
}

void RecordingActions::bindingsChanged(void *context, calldata_t *)
{
	auto *self = static_cast<RecordingActions *>(context);
	if (!self->exiting)
		QMetaObject::invokeMethod(
			self,
			[self] {
				if (!self->exiting)
					self->save();
			},
			Qt::QueuedConnection);
}

void RecordingActions::eventCallback(obs_frontend_event event, void *context)
{
	auto *self = static_cast<RecordingActions *>(context);
	// Frontend events are delivered by OBS's UI thread. Handle STOPPED immediately
	// to capture its exact path before another recording can replace it.
	self->event(event);
}

bool RecordingActions::remuxEnabled() const
{
	auto *config = obs_frontend_get_profile_config();
	return config && config_get_bool(config, "Video", "AutoRemux");
}

void RecordingActions::start(PendingAction action)
{
	if (exiting || state.pending != PendingAction::None || !obs_frontend_recording_active())
		return;
	if (remuxEnabled()) {
		log(LOG_WARNING, "Action ignored: disable automatic remuxing before using Recording Actions.");
		return;
	}
	std::filesystem::path destination;
	if (action != PendingAction::Delete)
		destination = settings.targets[action == PendingAction::MoveTarget1 ? 0 : 1].directory;
	if (!state.start(action, true, destination)) {
		log(LOG_WARNING, "Action ignored: configure an absolute destination folder.");
		return;
	}
	log(LOG_INFO, "Recording stop requested.");
	obs_frontend_recording_stop();
}

void RecordingActions::event(obs_frontend_event event)
{
	if (exiting)
		return;
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		shutdown();
		return;
	}
	if (event == OBS_FRONTEND_EVENT_RECORDING_STARTING) {
		if (state.processing)
			cancellation.cancel();
		else
			state.clear();
		return;
	}
	if (event != OBS_FRONTEND_EVENT_RECORDING_STOPPED || !state.stopped())
		return;
	std::unique_ptr<char, decltype(&bfree)> path(obs_frontend_get_last_recording(), bfree);
	if (!path || !*path || remuxEnabled()) {
		log(LOG_WARNING, "Action cancelled: recording path unavailable or automatic remuxing enabled.");
		state.clear();
		return;
	}
	try {
		auto source = std::filesystem::u8path(path.get());
		auto destination = state.directory;
		bool deleting = state.pending == PendingAction::Delete;
		cancellation.requested = false;
		std::packaged_task<FileResult()> task([source, destination, deleting, this] {
			return processRecording(source, destination, deleting, cancellation);
		});
		worker = task.get_future();
		workerThread = std::thread(std::move(task));
		log(LOG_INFO, "Recording finalized; processing exact OBS recording path.");
		timer.start();
	} catch (...) {
		state.clear();
		log(LOG_ERROR, "Unable to start file operation; source preserved.");
	}
}

void RecordingActions::completed()
{
	if (!worker.valid() || worker.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		return;
	auto result = worker.get();
	if (workerThread.joinable())
		workerThread.join();
	timer.stop();
	state.clear();
	if (result.success)
		log(LOG_INFO, "Recording action completed.");
	else if (settings.enableLogs)
		blog(LOG_ERROR,
		     "[Recording Actions] File operation failed (Windows error %lu, attempts %u); source preserved.",
		     result.error, result.attempts);
}

void RecordingActions::save()
{
	for (const auto &key : hotkeys) {
		std::unique_ptr<obs_data_array_t, decltype(&obs_data_array_release)> bindings(obs_hotkey_save(key.id),
											      obs_data_array_release);
		obs_data_set_array(data.get(), keys[&key - hotkeys.data()], bindings.get());
	}
	if (!writeConfiguration(data.get(), settings))
		log(LOG_ERROR, "Unable to save configuration.");
}

QString RecordingActions::alias(size_t index) const
{
	auto name = QString::fromUtf8(settings.targets[index].alias.c_str()).trimmed();
	return name.isEmpty() ? text(index == 0 ? "Target1" : "Target2") : name;
}

void RecordingActions::updateDescriptions()
{
	for (size_t i = 0; i < 2; ++i) {
		auto description = text("Hotkey.Move").arg(alias(i)).toUtf8();
		obs_hotkey_set_description(hotkeys[i].id, description.constData());
	}
	obs_hotkey_set_description(hotkeys[2].id, obs_module_text("Hotkey.Delete"));
}

void RecordingActions::showSettings()
{
	if (exiting)
		return;
	if (dialog) {
		dialog->show();
		dialog->raise();
		dialog->activateWindow();
		return;
	}
	auto *window = new QDialog(static_cast<QWidget *>(obs_frontend_get_main_window()));
	dialog = window;
	window->setAttribute(Qt::WA_DeleteOnClose);
	window->setWindowTitle(text("Plugin.Name") + " " RECORDING_ACTIONS_VERSION);
	auto *layout = new QVBoxLayout(window);
	layout->setSpacing(16);
	layout->setContentsMargins(24, 24, 24, 24);
	std::array<QLineEdit *, 2> names{}, directories{};
	for (size_t i = 0; i < 2; ++i) {
		auto *group = new QGroupBox(text(i == 0 ? "Settings.Target1" : "Settings.Target2"), window);
		auto *form = new QFormLayout(group);
		names[i] = new QLineEdit(QString::fromUtf8(settings.targets[i].alias.c_str()), group);
		names[i]->setPlaceholderText(text(i == 0 ? "Target1" : "Target2"));
		form->addRow(text("Settings.Alias"), names[i]);
		auto *row = new QHBoxLayout;
		directories[i] = new QLineEdit(QString::fromStdWString(settings.targets[i].directory.native()), group);
		directories[i]->setAccessibleName(text("Settings.Directory"));
		auto *browse = new QPushButton(text("Settings.Browse"), group);
		row->addWidget(directories[i], 1);
		row->addWidget(browse);
		form->addRow(text("Settings.Directory"), row);
		connect(browse, &QPushButton::clicked, window, [window, edit = directories[i]] {
			auto path = QFileDialog::getExistingDirectory(window, text("Settings.Directory"), edit->text());
			if (!path.isEmpty())
				edit->setText(path);
		});
		layout->addWidget(group);
	}
	auto *logging = new QCheckBox(text("Settings.EnableLogs"), window);
	logging->setChecked(settings.enableLogs);
	layout->addWidget(logging);
	auto *hint = new QLabel(text("Settings.Hint"), window);
	hint->setWordWrap(true);
	layout->addWidget(hint);
	auto *error = new QLabel(window);
	error->setWordWrap(true);
	error->setAccessibleName(text("Settings.Error"));
	layout->addWidget(error);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel | QDialogButtonBox::Help,
					     window);
	buttons->button(QDialogButtonBox::Help)->setText(text("Settings.About"));
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::rejected, window, &QDialog::reject);
	connect(buttons, &QDialogButtonBox::helpRequested, window, [window] {
		auto *about = new QDialog(window);
		about->setAttribute(Qt::WA_DeleteOnClose);
		about->setWindowTitle(text("Settings.About"));
		auto *box = new QVBoxLayout(about);
		auto *label = new QLabel(text("About.Text").arg(RECORDING_ACTIONS_VERSION), about);
		label->setWordWrap(true);
		label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByKeyboard);
		box->addWidget(label);
		auto *close = new QDialogButtonBox(QDialogButtonBox::Close, about);
		box->addWidget(close);
		connect(close, &QDialogButtonBox::rejected, about, &QDialog::close);
		about->resize(440, 240);
		about->show();
	});
	connect(buttons, &QDialogButtonBox::accepted, window, [=] {
		auto next = settings;
		for (size_t i = 0; i < 2; ++i) {
			next.targets[i].alias = names[i]->text().toUtf8().toStdString();
			auto directory = directories[i]->text().trimmed();
			auto path = std::filesystem::path(directory.toStdWString());
			if (!directory.isEmpty() && (!path.is_absolute() || directory.contains('*') ||
						     directory.contains('?') || directory.mid(2).contains(':'))) {
				error->setText(text("Settings.InvalidDirectory"));
				directories[i]->setFocus();
				return;
			}
			next.targets[i].directory = path;
		}
		next.enableLogs = logging->isChecked();
		next.geometry = window->saveGeometry().toBase64().toStdString();
		if (!writeConfiguration(data.get(), next)) {
			error->setText(text("Settings.SaveFailed"));
			return;
		}
		settings = next;
		updateDescriptions();
		window->accept();
	});
	connect(window, &QDialog::finished, this, [this, window] {
		settings.geometry = window->saveGeometry().toBase64().toStdString();
		save();
	});
	window->resize(560, 460);
	if (!settings.geometry.empty())
		window->restoreGeometry(QByteArray::fromBase64(settings.geometry.c_str()));
	window->show();
}
