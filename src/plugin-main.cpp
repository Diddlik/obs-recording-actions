#include "recording-actions.hpp"
#include <obs-module.h>
#include <memory>
#include <QCoreApplication>
#include <util/bmem.h>
#include <windows.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-recording-actions", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Configurable stop-and-move and stop-and-delete recording hotkeys";
}
MODULE_EXPORT const char *obs_module_name(void)
{
	return "Recording Actions";
}
MODULE_EXPORT const char *obs_module_author(void)
{
	return "Diddlik and contributors";
}

namespace {
std::unique_ptr<RecordingActions> manager;
}
bool obs_module_load(void)
{
	// Keep this mutex until process termination so an installer cannot replace a loaded DLL.
	CreateMutexW(nullptr, FALSE, L"Global\\ObsRecordingActions");
	std::unique_ptr<char, decltype(&bfree)> tlsPath(obs_module_file("qt"), bfree);
	if (tlsPath)
		QCoreApplication::addLibraryPath(QString::fromUtf8(tlsPath.get()));
	try {
		manager = std::make_unique<RecordingActions>();
		manager->initialize();
		return true;
	} catch (...) {
		manager.reset();
		return false;
	}
}
void obs_module_unload(void)
{
	manager.reset();
}
