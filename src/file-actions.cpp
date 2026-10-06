#include "file-actions.hpp"
#include <windows.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <vector>

namespace {
namespace fs = std::filesystem;
struct Handle {
	HANDLE value = INVALID_HANDLE_VALUE;
	explicit Handle(HANDLE value) : value(value) {}
	~Handle()
	{
		if (value != INVALID_HANDLE_VALUE)
			CloseHandle(value);
	}
	Handle(const Handle &) = delete;
	Handle &operator=(const Handle &) = delete;
	explicit operator bool() const { return value != INVALID_HANDLE_VALUE; }
};

bool removeHandle(HANDLE handle)
{
	FILE_DISPOSITION_INFO info{TRUE};
	return SetFileInformationByHandle(handle, FileDispositionInfo, &info, sizeof(info)) != FALSE;
}

bool validPath(const fs::path &path)
{
	if (!path.is_absolute() || path.filename().empty())
		return false;
	auto text = path.native();
	if (text.find(L'\0') != std::wstring::npos || text.find_first_of(L"*?") != std::wstring::npos)
		return false;
	// Reject alternate data streams and device namespaces, including extended paths supplied as input.
	if (text.rfind(L"\\\\.\\", 0) == 0 || text.rfind(L"\\\\?\\", 0) == 0)
		return false;
	return text.find(L':', 2) == std::wstring::npos;
}

FileResult attempt(const fs::path &source, const fs::path &directory, bool deleting, Cancellation &cancel,
		   const BY_HANDLE_FILE_INFORMATION &expected)
{
	auto failure = [](DWORD error) {
		return FileResult{false, error, {}, 0};
	};
	if (cancel.requested)
		return failure(ERROR_CANCELLED);
	if (!validPath(source) || (!deleting && !validPath(directory / L"probe")))
		return failure(ERROR_INVALID_NAME);
	// Deny writes, renames and deletion for the lifetime of this operation. Delete by handle,
	// so a replacement at the original pathname can never be removed accidentally.
	Handle input(CreateFileW(source.c_str(), GENERIC_READ | DELETE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
				 FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
	if (!input)
		return failure(GetLastError());
	BY_HANDLE_FILE_INFORMATION info{};
	if (!GetFileInformationByHandle(input.value, &info))
		return failure(GetLastError());
	if (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
		return failure(ERROR_INVALID_DATA);
	if (info.dwVolumeSerialNumber != expected.dwVolumeSerialNumber ||
	    info.nFileIndexHigh != expected.nFileIndexHigh || info.nFileIndexLow != expected.nFileIndexLow ||
	    info.nFileSizeHigh != expected.nFileSizeHigh || info.nFileSizeLow != expected.nFileSizeLow ||
	    CompareFileTime(&info.ftLastWriteTime, &expected.ftLastWriteTime))
		return failure(ERROR_FILE_INVALID);
	if (cancel.requested)
		return failure(ERROR_CANCELLED);
	if (deleting) {
		if (!removeHandle(input.value))
			return failure(GetLastError());
		return {true, 0, {}, 0};
	}

	std::error_code ec;
	fs::create_directories(directory, ec);
	if (ec)
		return failure(static_cast<DWORD>(ec.value()));
	if (fs::equivalent(source.parent_path(), directory, ec) && !ec)
		return {true, 0, source, 0};
	if (ec)
		return failure(static_cast<DWORD>(ec.value()));

	for (unsigned suffix = 0; suffix < 100000; ++suffix) {
		if (cancel.requested)
			return failure(ERROR_CANCELLED);
		fs::path name = source.filename();
		if (suffix)
			name = source.stem().native() + L"_" + std::to_wstring(suffix) + source.extension().native();
		auto destination = directory / name;
		auto absolute = destination.native();
		const auto bytes = static_cast<DWORD>(absolute.size() * sizeof(wchar_t));
		std::vector<unsigned char> storage(sizeof(FILE_RENAME_INFO) + bytes);
		auto *rename = reinterpret_cast<FILE_RENAME_INFO *>(storage.data());
		rename->ReplaceIfExists = FALSE;
		rename->RootDirectory = nullptr;
		rename->FileNameLength = bytes;
		memcpy(rename->FileName, absolute.data(), bytes);
		if (SetFileInformationByHandle(input.value, FileRenameInfo, rename, static_cast<DWORD>(storage.size())))
			return {true, 0, destination, 0};
		DWORD error = GetLastError();
		if (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS)
			continue;
		if (error != ERROR_NOT_SAME_DEVICE)
			return failure(error);

		Handle output(CreateFileW(destination.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW,
					  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
		if (!output) {
			error = GetLastError();
			if (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS)
				continue;
			return failure(error);
		}
		auto buffer = std::make_unique<std::array<char, 1024 * 1024>>();
		LARGE_INTEGER total{};
		for (;;) {
			if (cancel.requested) {
				error = ERROR_CANCELLED;
				break;
			}
			DWORD read = 0;
			if (!ReadFile(input.value, buffer->data(), static_cast<DWORD>(buffer->size()), &read,
				      nullptr)) {
				error = GetLastError();
				break;
			}
			if (!read) {
				error = ERROR_SUCCESS;
				break;
			}
			DWORD written = 0;
			if (!WriteFile(output.value, buffer->data(), read, &written, nullptr)) {
				error = GetLastError();
				break;
			}
			if (written != read) {
				error = ERROR_WRITE_FAULT;
				break;
			}
			total.QuadPart += written;
		}
		LARGE_INTEGER sourceSize{}, targetSize{};
		if (!error && (!GetFileSizeEx(input.value, &sourceSize) || !GetFileSizeEx(output.value, &targetSize)))
			error = GetLastError();
		if (!error && (total.QuadPart != sourceSize.QuadPart || total.QuadPart != targetSize.QuadPart))
			error = ERROR_WRITE_FAULT;
		if (!error && !FlushFileBuffers(output.value))
			error = GetLastError();
		if (!error && cancel.requested)
			error = ERROR_CANCELLED;
		if (!error && !removeHandle(input.value))
			error = GetLastError();
		if (error) {
			// We exclusively own this newly created file; never remove an unrelated collision.
			removeHandle(output.value);
			return failure(error);
		}
		return {true, 0, destination, 0};
	}
	return failure(ERROR_TOO_MANY_NAMES);
}
} // namespace

FileResult processRecording(const fs::path &source, const fs::path &directory, bool deleteFile, Cancellation &cancel)
{
	FileResult result;
	try {
		if (cancel.requested)
			return {false, ERROR_CANCELLED, {}, 0};
		if (!validPath(source))
			return {false, ERROR_INVALID_NAME, {}, 1};
		// Keep a metadata-only handle alive across retries: detect pathname replacement
		// and prevent file-ID reuse while another recording can start.
		Handle identity(CreateFileW(source.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
					    nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
		if (!identity)
			return {false, GetLastError(), {}, 1};
		BY_HANDLE_FILE_INFORMATION expected{};
		if (!GetFileInformationByHandle(identity.value, &expected))
			return {false, GetLastError(), {}, 1};
		for (unsigned n = 0; n <= 20; ++n) {
			result = attempt(source, directory, deleteFile, cancel, expected);
			result.attempts = n + 1;
			if (result.success ||
			    (result.error != ERROR_ACCESS_DENIED && result.error != ERROR_SHARING_VIOLATION &&
			     result.error != ERROR_LOCK_VIOLATION) ||
			    n == 20)
				return result;
			std::unique_lock<std::mutex> lock(cancel.mutex);
			if (cancel.changed.wait_for(lock, std::chrono::milliseconds(500),
						    [&] { return cancel.requested.load(); }))
				return {false, ERROR_CANCELLED, {}, n + 1};
		}
	} catch (const fs::filesystem_error &e) {
		result.error = static_cast<unsigned long>(e.code().value());
	} catch (...) {
		result.error = ERROR_NOT_ENOUGH_MEMORY;
	}
	return result;
}
