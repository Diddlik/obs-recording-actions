#pragma once
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>

struct Cancellation {
	std::atomic_bool requested{false};
	std::mutex mutex;
	std::condition_variable changed;
	void cancel()
	{
		requested = true;
		changed.notify_all();
	}
};

struct FileResult {
	bool success = false;
	unsigned long error = 0;
	std::filesystem::path destination;
	unsigned attempts = 0;
};

// Empty destination means permanent deletion of this one exact regular file.
FileResult processRecording(const std::filesystem::path &source, const std::filesystem::path &directory,
			    bool deleteFile, Cancellation &cancel);
