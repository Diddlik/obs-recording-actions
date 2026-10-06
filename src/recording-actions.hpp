#pragma once
#include "action-state.hpp"
#include "file-actions.hpp"
#include "settings.hpp"
#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <QAction>
#include <QDialog>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <future>
#include <thread>

class RecordingActions : public QObject {
public:
	RecordingActions();
	~RecordingActions() override;
	void initialize();
	void shutdown();

private:
	struct Hotkey {
		RecordingActions *owner = nullptr;
		PendingAction action = PendingAction::None;
		obs_hotkey_id id = OBS_INVALID_HOTKEY_ID;
		std::atomic_bool down{false};
	};
	static void hotkeyCallback(void *, obs_hotkey_id, obs_hotkey_t *, bool);
	static void eventCallback(obs_frontend_event, void *);
	static void bindingsChanged(void *, calldata_t *);
	void event(obs_frontend_event);
	void start(PendingAction);
	void completed();
	void save();
	void updateDescriptions();
	void showSettings();
	void log(int level, const char *message);
	bool remuxEnabled() const;
	QString alias(size_t index) const;

	ObsData data{nullptr, obs_data_release};
	PluginSettings settings;
	ActionState state;
	std::array<Hotkey, 3> hotkeys;
	std::atomic_bool exiting{false};
	bool registered = false;
	QPointer<QAction> menu;
	QPointer<QDialog> dialog;
	QTimer timer;
	Cancellation cancellation;
	std::future<FileResult> worker;
	std::thread workerThread;
};
