#include "action-state.hpp"
#include "file-actions.hpp"
#include <windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;
namespace {
unsigned checks = 0;
void require(bool value, const char *message)
{
	++checks;
	if (!value)
		throw std::runtime_error(message);
}
void write(const fs::path &path, const std::string &value = "recording-content")
{
	fs::create_directories(path.parent_path());
	std::ofstream stream(path, std::ios::binary);
	stream << value;
	if (!stream)
		throw std::runtime_error("Fixture creation failed");
}
std::string read(const fs::path &path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), {}};
}
} // namespace

int main()
{
	auto name = L"recording-actions-tests-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
		    std::to_wstring(GetTickCount64());
	auto root = fs::current_path() / name;
	auto cross = fs::temp_directory_path() / name;
	try {
		ActionState state;
		require(!state.start(PendingAction::Delete, false), "No recording must do nothing");
		require(!state.start(PendingAction::MoveTarget1, true), "Empty target must not stop recording");
		require(!state.start(PendingAction::MoveTarget1, true, L"relative"), "Relative target rejected");
		require(!state.start(PendingAction::MoveTarget1, true, root / L"bad*folder"),
			"Wildcard target rejected before stop");
		require(state.start(PendingAction::MoveTarget2, true, root), "Valid target starts action");
		require(!state.start(PendingAction::Delete, true), "Second action ignored");
		require(state.pending == PendingAction::MoveTarget2 && state.directory == root,
			"Target snapshot preserved");
		require(state.stopped() && !state.stopped(), "Stopped event processed once");
		require(!state.start(PendingAction::Delete, true), "Hotkeys ignored while worker active");
		state.clear();
		require(state.pending == PendingAction::None && state.directory.empty() && !state.processing,
			"State cleared");
		require(!state.stopped(), "Normal stop has no action");
		Cancellation cancel;
		auto source = root / L"source" / L"録画 Ü.mkv";
		auto target = root / L"target";
		write(source);
		auto result = processRecording(source, target, false, cancel);
		require(result.success && !fs::exists(source) && read(result.destination) == "recording-content",
			"Unicode move");
		write(source, "second");
		result = processRecording(source, target, false, cancel);
		require(result.success && result.destination.filename() == L"録画 Ü_1.mkv", "Deterministic suffix");
		require(read(target / L"録画 Ü.mkv") == "recording-content", "Collision never overwritten");
		write(source, "third");
		result = processRecording(source, target, false, cancel);
		require(result.success && result.destination.filename() == L"録画 Ü_2.mkv", "Second suffix");
		write(source);
		result = processRecording(source, source.parent_path(), false, cancel);
		require(result.success && fs::exists(source), "Same directory is harmless no-op");
		auto obstacle = root / L"not-a-directory";
		write(obstacle, "obstacle");
		result = processRecording(source, obstacle / L"child", false, cancel);
		require(!result.success && fs::exists(source) && read(obstacle) == "obstacle",
			"Invalid destination preserves source");
		result = processRecording(source, L"relative", false, cancel);
		require(!result.success && fs::exists(source), "Relative destination preserves source");
		result = processRecording(root / L"source", {}, true, cancel);
		require(!result.success && fs::exists(source), "Directory delete rejected");
		result = processRecording(source.wstring() + L":stream", {}, true, cancel);
		require(!result.success && fs::exists(source), "Alternate stream rejected");
		result = processRecording(source, {}, true, cancel);
		require(result.success && !fs::exists(source) && fs::exists(target / L"録画 Ü.mkv"),
			"Exact delete only");
		result = processRecording(source, {}, true, cancel);
		require(!result.success && result.attempts == 1, "Missing source fails without guessing");

		write(source);
		HANDLE held = CreateFileW(source.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		require(held != INVALID_HANDLE_VALUE, "Lock fixture");
		auto task =
			std::async(std::launch::async, [&] { return processRecording(source, target, false, cancel); });
		std::this_thread::sleep_for(std::chrono::milliseconds(650));
		CloseHandle(held);
		result = task.get();
		require(result.success && result.attempts >= 2, "Sharing violation retries then succeeds");

		write(source);
		held = CreateFileW(source.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		auto before = std::chrono::steady_clock::now();
		task = std::async(std::launch::async, [&] { return processRecording(source, {}, true, cancel); });
		std::this_thread::sleep_for(std::chrono::milliseconds(80));
		cancel.cancel();
		result = task.get();
		CloseHandle(held);
		require(!result.success && result.error == ERROR_CANCELLED && fs::exists(source),
			"Cancelled retry preserves source");
		require(std::chrono::steady_clock::now() - before < std::chrono::seconds(2),
			"Cancellation wakes retry promptly");
		cancel.requested = false;
		held = CreateFileW(source.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		result = processRecording(source, {}, true, cancel);
		CloseHandle(held);
		require(!result.success && result.attempts == 21 && fs::exists(source),
			"Retry limit preserves locked file");

		held = CreateFileW(source.c_str(), GENERIC_READ, FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
		require(held != INVALID_HANDLE_VALUE, "Replacement lock fixture");
		task = std::async(std::launch::async, [&] { return processRecording(source, {}, true, cancel); });
		std::this_thread::sleep_for(std::chrono::milliseconds(150));
		auto original = source.parent_path() / L"original.mkv";
		fs::rename(source, original);
		write(source, "replacement");
		CloseHandle(held);
		result = task.get();
		require(!result.success && result.error == ERROR_FILE_INVALID && read(source) == "replacement" &&
				fs::exists(original),
			"Retry never deletes a replacement recording");

		auto left = root / L"left" / L"race.mp4";
		auto right = root / L"right" / L"race.mp4";
		write(left, "left");
		write(right, "right");
		task = std::async(std::launch::async, [&] { return processRecording(left, target, false, cancel); });
		auto other = processRecording(right, target, false, cancel);
		result = task.get();
		require(result.success && other.success && result.destination != other.destination,
			"Concurrent collision safe");
		require(read(result.destination) == "left" && read(other.destination) == "right",
			"Both race payloads intact");

		if (root.root_name() != cross.root_name()) {
			std::string payload(3 * 1024 * 1024 + 37, 'x');
			payload[123456] = 'z';
			write(source, payload);
			result = processRecording(source, cross, false, cancel);
			require(result.success && !fs::exists(source) && read(result.destination) == payload,
				"Real cross-volume copy and delete");
			write(source, "cross collision");
			result = processRecording(source, cross, false, cancel);
			require(result.success && result.destination.filename() == L"録画 Ü_1.mkv",
				"Cross-volume collision");
			require(read(cross / L"録画 Ü.mkv") == payload, "Cross-volume original destination intact");
			std::cout << "Real cross-volume tests passed\n";
		} else {
			std::cout << "SKIP real cross-volume: build and TEMP are on the same volume\n";
		}
		fs::remove_all(root);
		if (cross != root)
			fs::remove_all(cross);
		std::cout << checks << " checks passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "FAIL: " << error.what() << "\nFixtures retained: " << root << '\n';
		return 1;
	}
}
