#include "recording-actions.hpp"
#include <obs-module.h>
#include <memory>

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
