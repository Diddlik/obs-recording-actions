#pragma once
#include <filesystem>

enum class PendingAction { None, MoveTarget1, MoveTarget2, Delete };

// Only accessed on the frontend thread. A pending action includes worker retries.
struct ActionState {
	PendingAction pending = PendingAction::None;
	bool processing = false;
	std::filesystem::path directory;

	bool start(PendingAction action, bool recording, const std::filesystem::path &target = {})
	{
		if (!recording || pending != PendingAction::None || action == PendingAction::None)
			return false;
		if (action != PendingAction::Delete) {
			const auto &value = target.native();
			if (target.empty() || !target.is_absolute() ||
			    value.find_first_of(L"*?") != std::wstring::npos ||
			    value.find(L':', 2) != std::wstring::npos || value.find(L'\0') != std::wstring::npos ||
			    value.rfind(L"\\\\.\\", 0) == 0)
				return false;
		}
		pending = action;
		directory = target;
		return true;
	}
	bool stopped()
	{
		if (pending == PendingAction::None || processing)
			return false;
		processing = true;
		return true;
	}
	void clear() { *this = {}; }
};
