#include <cstdio>
// #include <cstdlib>
#include <conio.h>
// #include <cstring>
// #include <cwchar>
// #include <cerrno>
#include <fstream>
#include <string>
#include <map>
// #include <ctime>
#include <chrono>
#include <thread>
#include "md5hash.h"
#include "debug.h"
#include "handle.h"
#include <Shlobj.h>
#include <urlmon.h>
#include <Psapi.h>
#include <wrl.h>
#include <strsafe.h>
#include <WinInet.h>

#pragma comment(lib, "Psapi")
#pragma comment(lib, "urlmon")
#pragma comment(lib, "Wininet")

#define PROGRAM_VERSION L"v1.3"
#define LAST_UPDATE_DATE L"04.02.2017"

// g++ *.cpp -Wall -Wextra -pedantic -std=c++14 -O3 -Ofast -mwin32 -municode -march=i386 -mtune=generic -static -static-libstdc++ -lpsapi -lurlmon -lwininet -o program
// cl *.cpp /W4 /O2 /Ot /Ox /MT /Feprogram /EHsc Shell32.lib Advapi32.lib User32.lib

#define MAX_PATH_LEN 32768

#define RTN_OK 0
#define RTN_USAGE 1
#define RTN_ERROR 13

//Define extra colours
#define FOREGROUND_WHITE		    (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN)
#define FOREGROUND_YELLOW       	(FOREGROUND_RED | FOREGROUND_GREEN)
#define FOREGROUND_CYAN		        (FOREGROUND_BLUE | FOREGROUND_GREEN)
#define FOREGROUND_MAGENTA	        (FOREGROUND_RED | FOREGROUND_BLUE)
#define FOREGROUND_BLACK 0

#define FOREGROUND_INTENSE_RED		(FOREGROUND_RED | FOREGROUND_INTENSITY)
#define FOREGROUND_INTENSE_GREEN	(FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define FOREGROUND_INTENSE_BLUE		(FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define FOREGROUND_INTENSE_WHITE	(FOREGROUND_WHITE | FOREGROUND_INTENSITY)
#define FOREGROUND_INTENSE_YELLOW	(FOREGROUND_YELLOW | FOREGROUND_INTENSITY)
#define FOREGROUND_INTENSE_CYAN		(FOREGROUND_CYAN | FOREGROUND_INTENSITY)
#define FOREGROUND_INTENSE_MAGENTA	(FOREGROUND_MAGENTA | FOREGROUND_INTENSITY)

using namespace std;
using namespace std::chrono;
using namespace KennyKerr;
using namespace Microsoft::WRL;

using ull = unsigned long long;

static auto useQuickSay = bool{ true };
static auto use_curl_to_download = bool{ true };
static auto use_wget_to_download = bool{ false };
static auto use_urlmon_to_download = bool{ true };
static auto use_wininet_to_download = bool{ true };
auto console = HANDLE{ INVALID_HANDLE_VALUE };
static wchar_t game_folder_path[MAX_PATH_LEN];
static wchar_t old_file_path[MAX_PATH_LEN];
static wchar_t new_file_path[MAX_PATH_LEN];
static wchar_t search_file_path[MAX_PATH_LEN];
static wchar_t game_install_path[MAX_PATH_LEN];
static wchar_t current_dir_path[MAX_PATH_LEN];
static wchar_t current_file_path[MAX_PATH_LEN];
static wchar_t download_link[8192];

//struct ComException {
//	const HRESULT error_code;
//
//	ComException(HRESULT const code) : error_code{ code } {}
//};
//
//void checkComErrorCode(HRESULT const status_code) {
//	if (status_code != S_OK) throw ComException{ status_code };
//}

typedef void(*customErrorFnc)(const wchar_t*, DWORD);

void displayErrorMessage(LPTSTR lpszFunction, DWORD const custom_error_code = GetLastError(), bool exitProcess = false);
void displayErrorInfo(wchar_t const* custom_error_msg = nullptr, DWORD custom_error_code = -1, bool terminate_program_execution = false, customErrorFnc fn = nullptr);

void reportCorrectErrorInformationForCreateDirectory(DWORD error_code, wchar_t const* dir_name);

void tc(const char* color_codes) {
	
	auto color_value = WORD{};
	auto len = size_t{};
	
	if (S_OK != StringCchLengthA(color_codes, 16, &len)) return;

	// int len = lstrlenA(color_codes);

	for (size_t i = 0; i < len; ++i) {
		switch (color_codes[i]) {
		case 'r':
			color_value |= FOREGROUND_RED;
			break;

		case 'g':
			color_value |= FOREGROUND_GREEN;
			break;

		case 'b':
			color_value |= FOREGROUND_BLUE;
			break;

		case 'c':
			color_value |= (FOREGROUND_BLUE | FOREGROUND_GREEN);
			break;

		case 'm':
			color_value |= (FOREGROUND_RED | FOREGROUND_BLUE);
			break;

		case 'y':
			color_value |= (FOREGROUND_RED | FOREGROUND_GREEN);
			break;

		case 'i':
			color_value |= FOREGROUND_INTENSITY;
			break;

		default:
			break;
		}
	}

	if (color_value == 0) color_value = (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);

	SetConsoleTextAttribute(console, color_value);
}

void displayImportantInformationToUser(const char* color_codes, const wchar_t* msg);

static char abuffer[16392];
// static wchar_t wbuffer[8196];

template <typename ...T>
void unused(T&&...) {}
void say_slow(const char* msg, size_t const len) {
	bool sleep_on = true;
	
	const size_t delay = min(static_cast<size_t>(10), static_cast<size_t>(1000) / len);

	for (size_t i = 0; i < len; ++i) {
		WriteConsoleA(console, &msg[i], 1, nullptr, nullptr);
		if (sleep_on) this_thread::sleep_for(milliseconds(delay));
		if (_kbhit()) sleep_on = false;
	}
}

template <typename... Args>
void say(const wchar_t* szoveg, Args... args) {
	static wchar_t buffer[8196];
	auto len = size_t{};
	auto ret = size_t{};
	int alen;
	
	if (S_OK != StringCchPrintfW(buffer, _countof(buffer), szoveg, args...)) return;
	if (S_OK != StringCchLengthW(buffer, 8196, &len)) return;
		
	wcstombs_s(&ret, abuffer, buffer, _TRUNCATE);
	alen = lstrlenA(abuffer);

	if (useQuickSay) WriteConsoleA(console, abuffer, alen, nullptr, nullptr);
	else say_slow(abuffer, alen);
}

template <typename... Args>
void csay(const char* cc, const wchar_t* szoveg, Args... args) {
	static wchar_t buffer[8196];
	auto len = size_t{};
	auto ret = size_t{};
	int alen;

	tc(cc);
	StringCchPrintfW(buffer, _countof(buffer), szoveg, args...);	
	if (S_OK != StringCchLengthW(buffer, 8196, &len)) return;	
	wcstombs_s(&ret, abuffer, buffer, _TRUNCATE);
	alen = lstrlenA(abuffer);
	if (useQuickSay) WriteConsoleA(console, abuffer, alen, nullptr, nullptr);
	else say_slow(abuffer, alen);
}

template <typename StringType>
StringType trim(const StringType& str) {
	int begin = 0;
	int end = str.size() - 1;

	if (end < 0) return StringType{};
	for (; ((begin <= end) && iswspace(str[begin])); ++begin) continue;
	if (begin > end) return StringType{};
	for (; ((end > begin) && iswspace(str[end])); --end) continue;
	return str.substr(begin, end - begin + 1);
}

namespace {

	::HINTERNET netstart()
	{
		const ::HINTERNET handle =
			::InternetOpenW(0, INTERNET_OPEN_TYPE_DIRECT, 0, 0, 0);
		if (handle == 0)
		{
			displayErrorInfo(L"InternetOpenW()");
		}
		return (handle);
	}

	void netclose(::HINTERNET object)
	{
		const ::BOOL result = ::InternetCloseHandle(object);
		if (result == FALSE)
		{
			displayErrorInfo(L"InternetCloseHandle()");
		}
	}

	::HINTERNET netopen(::HINTERNET session, ::LPCWSTR url)
	{
		const ::HINTERNET handle =
			::InternetOpenUrlW(session, url, 0, 0, 0, 0);
		if (handle == 0)
		{
			displayErrorInfo(L"InternetOpenUrlW()");
		}
		return (handle);
	}

	void netfetch(::HINTERNET istream, std::ostream& ostream)
	{
		static const ::DWORD SIZE = 1024;
		::DWORD error = ERROR_SUCCESS;
		::BYTE data[SIZE];
		::DWORD size = 0;
		do {
			::BOOL result = ::InternetReadFile(istream, data, SIZE, &size);
			if (result == FALSE)
			{
				displayErrorInfo(L"InternetReadFile()");
			}
			ostream.write((const char*)data, size);
		} while ((error == ERROR_SUCCESS) && (size > 0));
	}

}

// Op is a binary operation on T, non-commutative operation, associative
// I is a forward iterator with value_type(I) == T
// precondition: carry != zero_value
template <typename T, typename I, typename Op>
T add_to_counter(I first, I last, Op fn, const T& zero_value, T carry) {
	while (first != last) {
		if (*first == zero_value) {
			*first = carry;
			return zero_value;
		}

		carry = fn(*first, carry);
		*first = zero_value;
		++first;
	}

	return carry;
}

BOOL SetPrivilege(
	HANDLE hToken,          // token handle
	LPCTSTR Privilege,      // Privilege to enable/disable
	BOOL bEnablePrivilege   // TRUE to enable.  FALSE to disable
);

void DisplayError(LPTSTR szAPI);

class Mapped_File_In_Memory
{
	char const* m_begin{};
	char const* m_end{};
	long file_size{};
	bool file_unmapped{};

public:

	Mapped_File_In_Memory(Mapped_File_In_Memory const&) = delete; // disable copy construction for new Mapped_File_In_Memory objects
	auto operator=(Mapped_File_In_Memory const&)->Mapped_File_In_Memory& = delete; // disable assigment operator

	explicit Mapped_File_In_Memory(wchar_t const* filename) noexcept
	{
		auto file = invalid_handle
		{
				CreateFile(filename,
						   GENERIC_READ,
						   FILE_SHARE_READ,
						   nullptr,
						   OPEN_EXISTING,
						   FILE_ATTRIBUTE_NORMAL,
						   nullptr)
		};

		if (!file) return;

		auto map = null_handle
		{
				CreateFileMapping(file.get(), nullptr, PAGE_READONLY, 0, 0, nullptr)
		};

		if (!map) return;

		auto size = LARGE_INTEGER{}; // union

		if (!GetFileSizeEx(file.get(), &size)) return;

		m_begin = static_cast<char const*>(MapViewOfFile(map.get(), FILE_MAP_READ, 0, 0, 0));

		if (!m_begin) return;

		m_end = m_begin + size.QuadPart;

		file_size = static_cast<long>(size.QuadPart);

		file_unmapped = false;
	}

	auto unmap() noexcept -> void
	{
		if (m_begin && !file_unmapped)
		{
			VERIFY(UnmapViewOfFile(m_begin));
			file_unmapped = true;
		}
	}

	~Mapped_File_In_Memory()
	{
		unmap();
	}

	Mapped_File_In_Memory(Mapped_File_In_Memory&& other) noexcept : m_begin(other.m_begin), m_end(other.m_end)
	{
		other.m_begin = nullptr;
		other.m_end = nullptr;
	}

	auto operator=(Mapped_File_In_Memory&& other) noexcept -> Mapped_File_In_Memory&
	{
		if (this != &other)
		{
			unmap();

			m_begin = other.m_begin;
			m_end = other.m_end;

			other.m_begin = nullptr;
			other.m_end = nullptr;
		}

		return *this;
	}

	explicit operator bool() const noexcept
	{
		return m_begin != nullptr;
	}

	auto begin() const noexcept -> char const*
	{
		return m_begin;
	}

	auto end() const noexcept -> char const*
	{
		return m_end;
	}

	auto get_file_size() const noexcept -> long
	{
		return file_size;
	}
};

auto begin(Mapped_File_In_Memory const& file) noexcept -> char const*
{
	return file.begin();
}

auto end(Mapped_File_In_Memory const& file) noexcept -> char const*
{
	return file.end();
}

struct FileInfo {
	wstring file_name_;
	wstring md5_hash_value_;
	long file_size_;

	FileInfo() : file_name_{}, md5_hash_value_{}, file_size_{ -1 } {}

	FileInfo(const wstring& input_file_name, const wstring& input_md5_value, const long file_size) :
		file_name_{ input_file_name }, md5_hash_value_{ input_md5_value }, file_size_{ file_size } {
	}
};

using KeyValuePair = pair<wstring, wstring>;
using ProgramSettings = map<wstring, wstring>;
using GameFolderContents = map<wstring, FileInfo>;
using VerindraCustomModAndMapFiles = map<wstring, GameFolderContents>;

struct GameLocation {
	const wchar_t* short_game_name_;
	const wchar_t* long_game_name_;
	wstring game_path_;
	VerindraCustomModAndMapFiles existingModAndMapFiles_;
	VerindraCustomModAndMapFiles neededModAndMapFiles_;

	// explicit GameLocation() {}

	explicit GameLocation(wchar_t const* short_game_name, wchar_t const* long_game_name, wchar_t const *game_path) :
		short_game_name_{ short_game_name }, long_game_name_{ long_game_name }, game_path_{ game_path } {}

	explicit GameLocation(wchar_t const* short_game_name, wchar_t const* long_game_name, wchar_t const *game_path,
		const VerindraCustomModAndMapFiles& existingModAndMapFiles, const VerindraCustomModAndMapFiles& neededModAndMapFiles) :
		short_game_name_{ short_game_name }, long_game_name_{ long_game_name }, game_path_{ game_path },
		existingModAndMapFiles_{ existingModAndMapFiles }, neededModAndMapFiles_{ neededModAndMapFiles } {}

	void setExistingModAndMapFiles(const VerindraCustomModAndMapFiles& existingCustomModAndMapFiles) {
		existingModAndMapFiles_ = existingCustomModAndMapFiles;
	}

	void setNeededModAndMapFiles(const VerindraCustomModAndMapFiles& neededCustomModAndMapFiles) {
		neededModAndMapFiles_ = neededCustomModAndMapFiles;
	}
};

struct DownloadStatusInfo {
	unsigned long long da_bytes;
	float da_time;
	float ra_time;
	int number_of_mod_folders_created;
	int number_of_mod_files_deleted;
	int number_of_mod_files_downloaded;
	int number_of_mod_files_moved;
	int number_of_map_files_deleted;
	int number_of_map_files_downloaded;
	int number_of_map_files_moved;
	int number_of_map_tmp_files_deleted;
	int number_of_mod_tmp_files_deleted;
	long smallestMapFileDownloaded;
	long smallestModFileDownloaded;
	long biggestMapFileDownloaded;
	long biggestModFileDownloaded;
};

bool startsWith(const char* text, const char start_char, bool ignore_case = false) {
	if (!text) return false;

	if (!ignore_case) {
		return(text[0] == start_char);
	}
	else {
		return(_tolower(text[0]) == _tolower(start_char));
	}
}

bool startsWith(const char* text, const char* start_tag, bool ignore_case = false) {
	static char haystack[MAX_PATH_LEN];
	static char needle[4096];
	const char* index;
	bool found;

	if (!text || !start_tag || (*text == 0) || (*start_tag == 0) || (lstrlenA(start_tag) > lstrlenA(text))) return false;

	if (!ignore_case) {
		index = strstr(text, start_tag);

		if (index == text) found = true;

		else found = false;
	}
	else {
		strcpy_s(haystack, text);
		strcpy_s(needle, start_tag);

		int lSize = lstrlenA(haystack);
		int rSize = lstrlenA(needle);

		for (int i = 0; i < lSize; ++i) haystack[i] = static_cast<char>(tolower(haystack[i]));
		for (int i = 0; i < rSize; ++i) needle[i] = static_cast<char>(tolower(needle[i]));

		index = strstr(haystack, needle);
		if (index == haystack) found = true;
		else found = false;

	}

	return found;
}

bool startsWith(const wchar_t* text, const wchar_t start_char, bool ignore_case = false) {
	if (!text) return false;

	if (!ignore_case) {
		return (text[0] == start_char);
	}
	else {
		return(_tolower(text[0]) == _tolower(start_char));
	}

}

bool startsWith(const wchar_t* text, const wchar_t* start_tag, bool ignore_case = false) {
	static wchar_t haystack[MAX_PATH_LEN];
	static wchar_t needle[4096];
	const wchar_t* index;
	bool found;

	if (!text || !start_tag || (*text == 0) || (*start_tag == 0) || (lstrlenW(start_tag) > lstrlenW(text))) return false;

	if (!ignore_case) {
		index = wcsstr(text, start_tag);

		if (index == text) found = true;

		else found = false;
	}
	else {
		if (S_OK != StringCchCopyW(haystack, _countof(haystack), text)) displayErrorInfo();
		if (S_OK != StringCchCopyW(needle, _countof(needle), start_tag)) displayErrorInfo();

		auto lSize = size_t{};
		auto rSize = size_t{};

		if (S_OK != StringCchLengthW(haystack, _countof(haystack), &lSize)) displayErrorInfo();
		if (S_OK != StringCchLengthW(needle, _countof(needle), &rSize)) displayErrorInfo();

		for (size_t i = 0; i < lSize; ++i) haystack[i] = static_cast<wchar_t>(towlower(haystack[i]));
		for (size_t i = 0; i < rSize; ++i) needle[i] = static_cast<wchar_t>(towlower(needle[i]));

		index = wcsstr(haystack, needle);
		if (index == haystack) found = true;
		else found = false;

	}

	return found;
}

bool endsWith(const char* text, const char end_char, bool ignore_case = false) {
	if (!text) return false;

	if (!ignore_case) {
		return (text[lstrlenA(text) - 1] == end_char);
	}
	else {
		return (_tolower(text[lstrlenA(text) - 1]) == _tolower(end_char));
	}
}

bool endsWith(const wchar_t* text, const wchar_t end_char, bool ignore_case = false) {
	if (!text) return false;

	if (!ignore_case) {
		return (text[lstrlenW(text) - 1] == end_char);
	}
	else {
		return (_tolower(text[lstrlenW(text) - 1]) == _tolower(end_char));
	}
}

bool endsWith(const char* text, const char* end_tag, bool ignore_case = false) {
	static char haystack[MAX_PATH_LEN];
	static char needle[4096];
	auto found = bool{};

	if (!text || !end_tag || (*text == 0) || (*end_tag == 0) || (lstrlenA(end_tag) > lstrlenA(text))) return false;

	if (!ignore_case) {
		const char* index = strstr(text, end_tag);

		if (index && (lstrlenA(index) == lstrlenA(end_tag))) found = true;
	}
	else {
		strcpy_s(haystack, text);
		strcpy_s(needle, end_tag);

		int lSize = lstrlenA(haystack);
		int rSize = lstrlenA(needle);

		for (int i = 0; i < lSize; ++i) haystack[i] = (char)tolower(haystack[i]);
		for (int i = 0; i < rSize; ++i) needle[i] = (char)tolower(needle[i]);

		const char* index = strstr(haystack, needle);
		if (index && (lstrlenA(index) == lstrlenA(needle))) found = true;
	}

	return found;
}

bool endsWith(const wchar_t* text, const wchar_t* end_tag, bool ignore_case = false) {
	static wchar_t haystack[MAX_PATH_LEN];
	static wchar_t needle[4096];
	auto found = bool{};

	if (!text || !end_tag || (*text == 0) || (*end_tag == 0) || (lstrlenW(end_tag) > lstrlenW(text))) return false;

	if (!ignore_case) {
		const wchar_t* index = wcsstr(text, end_tag);

		if (index && (lstrlenW(index) == lstrlenW(end_tag))) found = true;
	}
	else {
		if (S_OK != StringCchCopyW(haystack, _countof(haystack), text)) displayErrorInfo();
		if (S_OK != StringCchCopyW(needle, _countof(needle), end_tag)) displayErrorInfo();

		auto lSize = size_t{};
		auto rSize = size_t{};

		if (S_OK != StringCchLengthW(haystack, _countof(haystack), &lSize)) displayErrorInfo();
		if (S_OK != StringCchLengthW(needle, _countof(needle), &rSize)) displayErrorInfo();

		for (size_t i = 0; i < lSize; ++i) haystack[i] = (wchar_t)towlower(haystack[i]);
		for (size_t i = 0; i < rSize; ++i) needle[i] = (wchar_t)towlower(needle[i]);

		const wchar_t* index = wcsstr(haystack, needle);
		if (index && (lstrlenW(index) == lstrlenW(needle))) found = true;
	}

	return found;
}

bool replaceBackslashCharWithForwardChar(string& buffer, bool replace_first_only = true) {


	for (size_t i = 0; i < buffer.size(); ++i) {
		if (buffer[i] == '\\') {
			buffer[i] = '/';
			if (replace_first_only) return true;
		}
	}

	return false;
}

bool replaceBackslashCharWithForwardSlash(wstring& buffer, bool replace_first_only = true) {

	for (size_t i = 0; i < buffer.size(); ++i) {
		if (buffer[i] == L'\\') {
			buffer[i] = L'/';
			if (replace_first_only) return true;
		}
	}

	return false;
}



bool addDirPathSepChar(char* dir_path) {
	if (!endsWith(dir_path, '\\')) {
		auto len = lstrlenA(dir_path);

		if (endsWith(dir_path, '/')) {
			dir_path[len - 1] = '\\';
			dir_path[len] = '\0';
		}
		else {
			dir_path[len] = '\\';
			dir_path[len + 1] = '\0';
		}

		return true;
	}

	return false;
}

bool addDirPathSepChar(wchar_t* dir_path) {
	if (!endsWith(dir_path, L'\\')) {
		auto len = lstrlenW(dir_path);

		if (endsWith(dir_path, L'/')) {
			dir_path[len - 1] = L'\\';
			dir_path[len] = L'\0';
		}
		else {
			dir_path[len] = L'\\';
			dir_path[len + 1] = L'\0';
		}

		return true;
	}

	return false;
}

bool removeDirPathSepChar(char* dir_path) {
	
	if (endsWith(dir_path, '\\') || endsWith(dir_path, '/')) {
		auto len = lstrlenA(dir_path);	
			dir_path[len - 1] = 0;
			return true;
	}

	return false;
}

bool removeDirPathSepChar(wchar_t* dir_path) {

	if (endsWith(dir_path, L'\\') || endsWith(dir_path, L'/')) {
		auto len = lstrlenW(dir_path);
		dir_path[len - 1] = L'\0';
		return true;
	}

	return false;
}

static auto GetPosition(HANDLE con) -> COORD
{
	CONSOLE_SCREEN_BUFFER_INFO info;

	VERIFY(GetConsoleScreenBufferInfo(con, &info));

	return info.dwCursorPosition;
}

static auto ShowCursor(HANDLE con, bool const visible = true) -> void
{
	CONSOLE_CURSOR_INFO info;

	VERIFY(GetConsoleCursorInfo(con, &info));

	info.bVisible = visible;

	VERIFY(SetConsoleCursorInfo(con, &info));
}

struct ProgressCallback : RuntimeClass<RuntimeClassFlags<ClassicCom>, IBindStatusCallback>
{
	HANDLE m_console;
	COORD m_position;

	ProgressCallback(HANDLE con, COORD position) : m_console(con), m_position(position)
	{}

	ProgressCallback(const ProgressCallback& rhs) : m_console(rhs.m_console), m_position(rhs.m_position)
	{}

	auto updateCursorPosition() -> void {
		m_position = GetPosition(m_console);
	}

	auto __stdcall OnProgress(ULONG progress, // ULONG typedef of unsigned long
		ULONG progressMax,
		ULONG,
		LPCWSTR) -> HRESULT override
	{
		if (0 < progress && progress <= progressMax)
		{
			float percentF = progress * 100.0f / progressMax;
			unsigned percentU = min(100U, static_cast<unsigned>(percentF));

			VERIFY(SetConsoleCursorPosition(m_console, m_position));

			wprintf(L"%3d%%\n", percentU);
		}

		return S_OK;
	}

	auto __stdcall OnStartBinding(DWORD, IBinding *) -> HRESULT override
	{
		return E_NOTIMPL;
	}

	auto __stdcall GetPriority(LONG *) -> HRESULT override
	{
		return E_NOTIMPL;
	}

	auto __stdcall OnLowResource(DWORD) -> HRESULT override
	{
		return E_NOTIMPL;
	}

	auto __stdcall OnStopBinding(HRESULT, LPCWSTR) -> HRESULT override
	{
		return E_NOTIMPL;
	}

	auto __stdcall GetBindInfo(DWORD *, BINDINFO *) -> HRESULT override
	{
		return E_NOTIMPL;
	}

	auto __stdcall OnDataAvailable(DWORD, DWORD, FORMATETC *, STGMEDIUM *) -> HRESULT override
	{
		return E_NOTIMPL;
	}

	auto __stdcall OnObjectAvailable(REFIID, IUnknown *) -> HRESULT override
	{
		return E_NOTIMPL;
	}
};

const KeyValuePair& processConfigurationLine(const wstring& line);
const FileInfo& processDataLine(wstring& line);

void setTitle(const wchar_t* title) {
	static wchar_t buffer[512];
	auto convertedChars = size_t{};
	StringCchPrintfW(buffer, _countof(buffer), L"title %s %s", title, PROGRAM_VERSION);
	wcstombs_s(&convertedChars, abuffer, buffer, _TRUNCATE);
	system(abuffer);
	// _wsystem(buffer);
}

bool readProgramInformationAndSettings();

const wchar_t* checkIfGameInstalled();

bool downloadAndProcessVerindraFilesList();

bool scanAndProcessLocalGameFolderContents(const wchar_t* gamePath);

bool downloadMissingVerindraModFilesAndCustomMaps(const wchar_t* game_installation_path);

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData);

const wchar_t* BrowseFolder(const wchar_t* saved_path, const wchar_t* user_info);

auto time_now() -> high_resolution_clock::time_point
{
	return high_resolution_clock::now();
}

auto time_elapsed(const high_resolution_clock::time_point& start) -> float
{
	return duration_cast<duration<float>>(time_now() - start).count();
}

static wchar_t user_msg[1024];
static VerindraCustomModAndMapFiles neededFilesData{};

BOOL CALLBACK EnumCodePagesCallbackFn(LPTSTR lpCodePageString) {
	say(L"%s\n", lpCodePageString);
	return TRUE;
}

static ProgramSettings gProgramSettings{};

static wstring gShortGameName{};
static wstring gLongGameName{};

static const wchar_t* def_cod2_registry_location_subkeys[] = {
	L"SOFTWARE\\Activision\\Call of Duty 2",
	L"SOFTWARE\\WOW6432Node\\Activision\\Call of Duty 2",
	L"SOFTWARE\\Activision\\Call of Duty(R) 2",
	L"SOFTWARE\\WOW6432Node\\Activision\\Call of Duty(R) 2",
	nullptr };
static const wchar_t* def_cod4_registry_location_subkeys[] = {
	L"SOFTWARE\\Activision\\Call of Duty 4",
	L"SOFTWARE\\WOW6432Node\\Activision\\Call of Duty 4",
	L"SOFTWARE\\Activision\\Call of Duty(R) 4",
	L"SOFTWARE\\WOW6432Node\\Activision\\Call of Duty(R) 4",
	nullptr };

static const wchar_t* gFtpUrlToWget{};
static const wchar_t* gHttpUrlToWget{};

static const wchar_t* gFtpUrlToCurl{};
static const wchar_t* gHttpUrlToCurl{};

static const wchar_t* gFtpUrlToLibCurlDll{};
static const wchar_t* gHttpUrlToLibCurlDll{};

static const wchar_t* gFtpUrlToUpdatedList{};
static const wchar_t* gHttpUrlToUpdatedList{};

static const wchar_t* gHttpUrlToGameFiles{};
static const wchar_t* gFtpUrlToGameFiles{};

auto gDSI = DownloadStatusInfo{};

static DWORD processIds[8196];

static HWND hConWindow;

auto wmain() -> int {
	BOOL status;
	// auto error_no = DWORD{};
	// auto countProcessIds = DWORD{ _countof(processIds) };
	auto pBytesReturned = DWORD{};
	auto game_instance_terminated = bool{};
	// int pressed_key;
	char ch;
	errno = 0;

	console = GetStdHandle(STD_OUTPUT_HANDLE);
	if (console == INVALID_HANDLE_VALUE) {
		system("color 0C");
		_wperror(L"> Received error while trying to gain control of command prompt's STD_OUTPUT_HANDLE handle!\n> Exiting program...\n");
		return 1;
	}
		
	typedef BOOL(WINAPI *FN_SETCONSOLEFONT)(HANDLE, DWORD);
	FN_SETCONSOLEFONT SetConsoleFont;
	HMODULE hm = GetModuleHandleA("KERNEL32.DLL");

	if (!hm) {
		displayErrorInfo();
	} else {
		SetConsoleFont = (FN_SETCONSOLEFONT)GetProcAddress(hm, "SetConsoleFont");
		// int fontIndex = 10; // 10 is known to identify Lucida Console (a Unicode font)
		// BOOL bRet = SetConsoleFont(console, fontIndex);
		SetConsoleFont(console, 10);
	}

	// const UINT codePage = CP_UTF8;  // CP: 65001 - UTF8
	// const UINT codePage = 1200;     // 1200(utf-16 Unicode)
	SetConsoleOutputCP(CP_UTF8);
	
	// tc("ci");

	// SetConsoleTextAttribute(console, FOREGROUND_GREEN | FOREGROUND_INTENSITY);

	//auto cpinfo = CPINFOEX{};
	//auto ccp = GetConsoleOutputCP(); // ccp : UINT (identifier of currently used code page)
	//GetCPInfoEx(ccp, 0, &cpinfo);
	//say(L"Currently used output code page: %s\n", cpinfo.CodePageName);

	//auto status_check = EnumSystemCodePages(EnumCodePagesCallbackFn, CP_INSTALLED);
	//if (!status_check) {
	//	error_no = GetLastError();
	//	_wcserror_s(user_msg, error_no);
	//	say(user_msg);
	//	say(L"Press any key to terminate program...\n");
	//	_getch();
	//	return 0;
	//}

	auto cbi = CONSOLE_SCREEN_BUFFER_INFO{};
	if (!GetConsoleScreenBufferInfo(console, &cbi)) displayErrorInfo();

	// say(L"\n> cbi.dwSize.X = %d, cbi.dwSize.Y = %d", cbi.dwSize.X, cbi.dwSize.Y);

	auto maxConsoleWindowSizeCoordinate = GetLargestConsoleWindowSize(console);

	if (maxConsoleWindowSizeCoordinate.X == 0 || maxConsoleWindowSizeCoordinate.Y == 0) displayErrorInfo();

	// say(L"\n> maxConsoleWindowSizeCoordinate.X = %d, maxConsoleWindowSizeCoordinate.Y = %d", maxConsoleWindowSizeCoordinate.X, maxConsoleWindowSizeCoordinate.Y);

	short maxConWinSizeX = maxConsoleWindowSizeCoordinate.X - static_cast<short>(30);
	short maxConWinSizeY = maxConsoleWindowSizeCoordinate.Y;

	cbi.dwSize = COORD{ maxConWinSizeX , static_cast<short>(300) };
	// cbi.bFullscreenSupported = true;
	cbi.dwMaximumWindowSize = COORD{ maxConWinSizeX , maxConWinSizeY };
	cbi.dwCursorPosition = COORD{ 0, 0 };
	auto consoleWindowCoordinates = _SMALL_RECT{ 0, 0, maxConWinSizeX, maxConWinSizeY };
	cbi.srWindow = consoleWindowCoordinates;

	if (!SetConsoleScreenBufferSize(console, cbi.dwSize)) displayErrorInfo();

	// auto maxConsoleWindowCoordinates = COORD{ maxConsoleWindowSizeCoordinate.X , 80 };

	if (!SetConsoleWindowInfo(console, TRUE, &consoleWindowCoordinates)) displayErrorInfo();
	hConWindow = GetConsoleWindow();
	auto const current_screen_width = GetSystemMetrics(SM_CXSCREEN);
	auto const current_screen_height = GetSystemMetrics(SM_CYSCREEN);
	auto const width = current_screen_width / 2;
	auto const height = current_screen_height - 200;
	MoveWindow(hConWindow, (current_screen_width - width) / 2, (current_screen_height - height) / 2, width, height, TRUE);

	SetConsoleMode(console, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
	// SetConsoleTextAttribute(console, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);

	setTitle(L"Verindra auto-updater/downloader tool");
	
	const wchar_t* important_info[] = {
	L"\n*********************** Important information for user ************************",
	L"* If you notice that the program isn't working properly, correctly, meaning   *",
	L"* if you notice that it is unable to create new Verindra mod folders and/or   *",
	L"* create, delete, move and download Verindra mod and custom map files,        *",
	L"* then run the program as Administrator.                                      *",
	L"*******************************************************************************" };	

	auto current_time = time_t{};

	do {
		system("cls");
		tc("ci");
		current_time = time_t{ time(nullptr) };
		say(L"\n# Program execution started at %s\n", _wctime(&current_time));

		say(L"\n# Welcome to TheSyndicate[VER]'s Verindra auto-updater/downloader tool %s\n# Date of last update: %s"
			" AM\n# Developed by the Verindra team\n# Contact e - mail: ffverindra@gmail.com\n# Contact website: verindra.ga\n"
			"# Contact facebook page: Verindra Freedom Fighters\n\n", PROGRAM_VERSION, LAST_UPDATE_DATE);
			
		for (size_t i = 0; i < (sizeof(important_info) / sizeof(wchar_t*)); ++i) {
			displayImportantInformationToUser("yi", important_info[i]);
		}


		csay("yi", L"\n\n>> Select your Verindra game <<\n\n");
		tc("gi");
		say(L" 1. Call of Duty 2\n\n");
		say(L" 2. Call of Duty 4\n\n");
		say(L" q. Quit\n\n");
		csay("yi", L">> Your choice (1, 2 or q): ");
		tc("ri");
		ch = static_cast<char>(_getche());

		switch (ch) {
		case '1':
			gShortGameName = L"cod2";
			gLongGameName = L"Call of Duty 2";

			tc("yi");
			say(L"\n\n> *** Important information ***\n");
			say(L"\n> Please, make sure that your game isn't running in the background...\n\n> ...while the Verindra auto-updater tool is working.\n");
			say(L"\n> Checking if Call of Duty 2 Multiplayer game is currently running on your computer...\n");

			status = EnumProcesses(processIds, sizeof(processIds), &pBytesReturned);

			if (!status) {
				displayErrorInfo();
			}
			else {
				// auto pHandle = HANDLE{};
				auto hToken = HANDLE{};

				// show correct usage for kill
				if (!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &hToken))
				{
					if (GetLastError() == ERROR_NO_TOKEN)
					{
						// if (!ImpersonateSelf(SecurityImpersonation)) DisplayError(L"\nRTN_ERROR");
						if (!ImpersonateSelf(SecurityImpersonation)) displayErrorInfo();
						// if (!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &hToken)) DisplayError(L"\nOpenThreadToken");
						OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &hToken);
					}
					else DisplayError(L"\nRTN_ERROR");
				}

				// enable SeDebugPrivilege
				SetPrivilege(hToken, SE_DEBUG_NAME, TRUE);

				for (DWORD i = 0; i < pBytesReturned / sizeof(DWORD); ++i) {
					auto hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processIds[i]);
					if (hProcess == NULL) {
						// displayErrorInfo();
						continue;
					}

					if (GetProcessImageFileName(hProcess, search_file_path, MAX_PATH_LEN)) {
						if (endsWith(search_file_path, L"CoD2MP_s.exe", true)) {
							tc("yi");
							say(L"> Detected a running instance of Call of Duty 2 Multiplayer game on your computer...\n");
							say(L"> Shutting down detected instance of Call of Duty 2 Multiplayer...\n");

							// disable SeDebugPrivilege
							// SetPrivilege(hToken, SE_DEBUG_NAME, FALSE);

							if (!TerminateProcess(hProcess, 0))
							{
								auto retval = system("taskkill /FI \"IMAGENAME eq cod2mp_s.exe\" /F /T");
								if (retval != 0) {
									tc("ri");
									say(L"\n> Failed to terminate detected instance of Call of Duty 2 Multiplayer game on your computer!\n> Please, close your game now and press any key when you are ready to continue...\n");
									tc("yi");
									say(L"\n> When you are ready press any key to continue...");
									(void)_getch();
								}
								else {
									game_instance_terminated = true;
									tc("gi");
									say(L"\n> Successfully terminated detected instance of Call of Duty 2 Multiplayer game's process in memory.\n");
								}
							}
							else {
								game_instance_terminated = true;
								tc("gi");
								say(L"\n> Successfully terminated detected instance of Call of Duty 2 Multiplayer game's process in memory.\n");
							}

							break;
						}
					}

					// should I close an open process HANDLE after opening it with OpenProcess() function?

					// close handles
					CloseHandle(hProcess);
				}

				SetPrivilege(hToken, SE_DEBUG_NAME, FALSE);
				CloseHandle(hToken);
			}

			if (!game_instance_terminated) {
				auto retval = system("TASKLIST /NH | findstr \"CoD2MP_s.exe\"");
				if (retval == 0) {
					tc("yi");
					say(L"> Detected a running instance of Call of Duty 2 Multiplayer game on your computer...\n");
					say(L"> Shutting down detected instance of Call of Duty 2 Multiplayer...\n");
					retval = system("taskkill /FI \"IMAGENAME eq cod2mp_s.exe\" /F /T");
					if (retval != 0) {
						tc("ri");
						say(L"\n> Failed to terminate detected instance of Call of Duty 2 Multiplayer game running on your computer!\n> Please close your game manually now.\n");
						tc("yi");
						say(L"\n> When you are ready press any key to continue...");
						(void) _getch();
					}
					game_instance_terminated = true;
				}
			}

			if (!game_instance_terminated) {
				tc("gi");
				say(L"\n> Success! It seems like Call of Duty 2 Multiplayer isn't running on your computer now...\n");
			}

			break;

		case '2':
			gShortGameName = L"cod4";
			gLongGameName = L"Call of Duty 4";
			break;

		case VK_ESCAPE:
		case 'q':
		case 'Q':
		case 'x':
		case 'X':			
			useQuickSay = false;
			tc("ci");
			csay("yi", L"\n\n> Exiting TheSyndicate[VER]'s Verindra auto-updater/downloader tool v1.0...\n");
			csay("ci", L"\n> Thank you for using this program. Bye!\n\n> Press any key to quit...");
			(void) _getch();
			// Sleep(1000);
			return 0;

		default:
			tc("ri");
			say(L"\n\n> You chose an incorrect option! Please try again.");
			tc("yi");
			say(L"\n> Acceptable character keys are 1, 2 or x.\n");
			Sleep(1000);
			break;
		}
	} while ((ch != '1') && (ch != '2') && (ch != 'q') && (ch != 'Q') && (ch != 'x') && (ch != 'X'));

	float measured_time;
	long seconds, milliseconds, minutes;
	auto const ra_start_time = time_now();

	tc("yi");
	say(L"\n\n> Reading in custom program settings from Verindra auto-updater/downloader tool's default settings.ini file...\n");

	if (!readProgramInformationAndSettings()) {
		tc("ri");
		say(L"\n> Failed to process program's custom settings that are defined in program's default settings.ini!\n> Falling back to using default program settings...\n");
	}
	else {
		tc("gi");
		say(L"\n> Successfully read in program's custom settings from its default settings.ini file.");
	}

	if (gShortGameName == L"cod2") {
		gFtpUrlToUpdatedList = gProgramSettings[L"ftp_url_to_updated_list_of_cod2_files"].c_str();
		gHttpUrlToUpdatedList = gProgramSettings[L"http_url_to_updated_list_of_cod2_files"].c_str();
		gFtpUrlToGameFiles = gProgramSettings[L"ftp_base_url_to_cod2_files"].c_str();
		gHttpUrlToGameFiles = gProgramSettings[L"http_base_url_to_cod2_files"].c_str();
	}
	else {
		gFtpUrlToUpdatedList = gProgramSettings[L"ftp_url_to_updated_list_of_cod4_files"].c_str();
		gHttpUrlToUpdatedList = gProgramSettings[L"http_url_to_updated_list_of_cod4_files"].c_str();
		gFtpUrlToGameFiles = gProgramSettings[L"ftp_base_url_to_cod4_files"].c_str();
		gHttpUrlToGameFiles = gProgramSettings[L"http_base_url_to_cod4_files"].c_str();
	}

	gFtpUrlToCurl = gProgramSettings[L"ftp_download_link_for_curl"].c_str();
	gHttpUrlToCurl = gProgramSettings[L"http_download_link_for_curl"].c_str();
	gFtpUrlToLibCurlDll = gProgramSettings[L"ftp_download_link_for_libcurldll"].c_str();
	gHttpUrlToLibCurlDll = gProgramSettings[L"http_download_link_for_libcurldll"].c_str();
	gFtpUrlToWget = gProgramSettings[L"ftp_download_link_for_wget"].c_str();
	gHttpUrlToWget = gProgramSettings[L"http_download_link_for_wget"].c_str();

	bool status_code = downloadAndProcessVerindraFilesList();

	if (!status_code) {
		tc("ri");
		say(L"\n> Error downloading and processing updated list of Verindra's required custom mod and map files!\n"
			L"> Please make sure that your internet connection is working and then try again.\n");
		return 1;
	}

	const wchar_t* installed_game_path = checkIfGameInstalled();

	/*if (lstrcmpW(installed_game_path, L"C:\\") == 0) {
		StringCchPrintfW(user_msg, _countof(user_msg), L"Select your Call of Duty 2 game installation folder and click OK to continue.");
	}
	else {
		StringCchPrintfW(user_msg, _countof(user_msg), L"Please, choose the correct path to your Call of Duty 2 installation folder:");
	}*/

	StringCchPrintfW(user_msg, _countof(user_msg), L"Please, select your %s game installation folder and click OK to continue.", gLongGameName.c_str());

	const wchar_t* game_path = BrowseFolder(installed_game_path, user_msg);

	if ((lstrcmpW(game_path, L"") == 0) || (lstrcmpW(game_path, L"C:\\") == 0))
	{
		csay("ri", L"\n> Error! You haven't selected a valid folder for your game installation.");
		csay("yi", L"\n\n> You have to select your game's directory and press the OK button.");
		csay("gi", L"\n\n> Restart the program and try again.\n");
		useQuickSay = false;
		tc("gi");
		measured_time = time_elapsed(ra_start_time);
		seconds = static_cast<long>(floor(measured_time));
		milliseconds = static_cast<long>((measured_time - static_cast<float>(seconds)) * 1000);
		minutes = seconds / 60;
		seconds -= (minutes * 60);

		long hours = 0;
		if (minutes > 59)
		{
			hours = minutes / 60;
			minutes -= (hours * 60);
		}

		csay("ci", L"\n> Program finished running in %02ld:%02ld:%02ld:%03ld ms.\n", hours, minutes, seconds, milliseconds);
		csay("ci", L"\n> Exiting TheSyndicate[VER]'s Verindra auto-updater/downloader tool %s...\n", PROGRAM_VERSION);
		csay("ci", L"\n> Thank you for using this program.\n\n> Press any key to quit...");
		(void) _getch();
		return 0;
	}

	scanAndProcessLocalGameFolderContents(game_path);

	auto noOfModFiles = size_t{};
	auto noOfMapFiles = size_t{};

	auto mod_files_size = unsigned long long{};
	auto map_files_size = unsigned long long{};


	for (const auto& data : neededFilesData) {
		if (gShortGameName == L"cod2") {
			if (startsWith(data.first.c_str(), L"main", true)) {
				noOfMapFiles += data.second.size();
				continue;
			}
			else {
				noOfModFiles += data.second.size();
				continue;
			}
		}
		else if (gShortGameName == L"cod4") {
			if (startsWith(data.first.c_str(), L"usermaps", true)) {
				noOfMapFiles += data.second.size();
				continue;
			}
			else {
				noOfModFiles += data.second.size();
				continue;
			}
		}
	}

	if (noOfModFiles || noOfMapFiles) {
	
		for (const auto& data : neededFilesData) {
			if (gShortGameName == L"cod2") {
				if (startsWith(data.first.c_str(), L"main", true)) {
					for (auto const& fname : data.second) {
						map_files_size += fname.second.file_size_;
					}	

					continue;
				}
				else {
					for (auto const& fname : data.second) {
						mod_files_size += fname.second.file_size_;
					}

					continue;
				}
			}
			else if (gShortGameName == L"cod4") {
				if (startsWith(data.first.c_str(), L"usermaps", true)) {
					
					for (auto const& fname : data.second) {
						map_files_size += fname.second.file_size_;
					}

					continue;
				}
				else {
					for (auto const& fname : data.second) {
						mod_files_size += fname.second.file_size_;
					}
					continue;
				}
			}
		}

		csay("yi", L"\n> Your %s game is missing %lu required mod files <%.2f MB> and %lu necessary custom map files <%.2f MB>.\n", gLongGameName.c_str(), noOfModFiles, static_cast<float>(mod_files_size)/1048576, noOfMapFiles, static_cast<float>(map_files_size)/1048576);
		csay("gi", L"\n> Launching task of downloading missing Verindra files...\n");
		csay("yi", L"\n> You can always abort the download task by pressing the Escape key...\n\n> ...or temporarily pause it by pressing the Space key.\n");
		tc("mi");
		
		downloadMissingVerindraModFilesAndCustomMaps(game_path);
	
	} else {
		csay("gi", L"\n> Great, your %s game has already got all the necessary Verindra custom mod and map files!\n\n> There is nothing to download from the internet for your game.\n", gLongGameName.c_str());		
	}	

	useQuickSay = false;
	tc("gi");
	measured_time = time_elapsed(ra_start_time);
	seconds = static_cast<long>(floor(measured_time));
	milliseconds = static_cast<long>((measured_time - static_cast<float>(seconds)) * 1000);
	minutes = seconds / 60;
	seconds -= (minutes * 60);

	long hours = 0;
	if (minutes > 59)
	{
		hours = minutes / 60;
		minutes -= (hours * 60);
	}

	csay("ci", L"\n> Program successfully finished running in %02ld:%02ld:%02ld:%03ld ms.\n", hours, minutes, seconds, milliseconds);
	csay("ci", L"\n> Exiting TheSyndicate[VER]'s Verindra auto-updater/downloader tool %s...\n", PROGRAM_VERSION);
	csay("ci", L"\n> Thank you for using this program.\n\n> Press any key to quit...");
	(void) _getch();
	return 0;
}

const wchar_t* BrowseFolder(const wchar_t* saved_path, const wchar_t* user_info)
{
	static wchar_t path[MAX_PATH_LEN];
	// const wchar_t* path_param = saved_path.c_str();

	BROWSEINFO bi{};
	bi.lpszTitle = user_info;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
	bi.lpfn = BrowseCallbackProc;
	bi.lParam = reinterpret_cast<LPARAM>(saved_path);

	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);

	if (pidl != 0)
	{
		//get the name of the folder and put it in path
		SHGetPathFromIDList(pidl, path);

		//free memory used
		IMalloc* imalloc = 0;
		if (SUCCEEDED(SHGetMalloc(&imalloc)))
		{
			imalloc->Free(pidl);
			imalloc->Release();
		}

		return path;
	}

	path[0] = L'\0';
	return path;
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	unused(lParam);

	if (uMsg == BFFM_INITIALIZED)
	{
		auto rect_con = RECT{};
		auto rect_fbd = RECT{};
		csay("yi", L"\n> Currently selected path: %s\n", reinterpret_cast<const wchar_t*>(lpData));
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
		SendMessage(hwnd, BFFM_SETEXPANDED, TRUE, lpData);
		SendMessage(hwnd, BFFM_SETSTATUSTEXT, 0L, lpData);		
		auto const current_screen_width = GetSystemMetrics(SM_CXSCREEN);
		auto const current_screen_height = GetSystemMetrics(SM_CYSCREEN);
		if (!GetWindowRect(hConWindow, &rect_con)) displayErrorInfo();
		if (!GetWindowRect(hwnd, &rect_fbd)) displayErrorInfo();
		MoveWindow(hConWindow, (current_screen_width - (rect_con.right - rect_con.left)) / 2, (current_screen_height - (rect_con.bottom - rect_con.top)) / 2,
			rect_con.right - rect_con.left, rect_con.bottom - rect_con.top, TRUE);
		ShowWindow(hwnd, SW_NORMAL);		
		MoveWindow(hwnd, (current_screen_width - (rect_fbd.right - rect_fbd.left)) / 2, 
						 (current_screen_height - (rect_fbd.bottom - rect_fbd.top)) / 2, 
						   rect_fbd.right - rect_fbd.left, rect_fbd.bottom - rect_fbd.top, TRUE);
		UpdateWindow(hwnd);
		SetFocus(hwnd);
		SetForegroundWindow(hwnd);
	}

	return 0;
}

bool downloadAndProcessVerindraFilesList() {
	// auto const line_pattern = regex{R"(\[([a-zA-Z0-9_\/-]+)\])"};
	static wchar_t file_name[256];
	static wchar_t correct_time_word[128];
	auto fileData = map<wstring, FileInfo>{};
	WIN32_FIND_DATAW file_data{};
	bool download_success;
	HRESULT status;
	HANDLE search_handle;
	float dt;
	// auto dmsf = float{};
	long dms, ds, dm, dh;
	auto file_size = float{};
	long file_size_long;
	auto convertedChars = size_t{};
	auto start = high_resolution_clock::time_point{};
	const wchar_t* correct_size_word;

	auto callback = ProgressCallback
	{
		console,
		GetPosition(console)
	};

	csay("yi", L"\n\n> Getting current working directory...");
	auto count = GetCurrentDirectory(_countof(current_dir_path), current_dir_path);

	if (!count) {
		csay("ri", L"\n\n> Could not retrieve current working directory!");
		displayErrorInfo();
	}
	else {
		// say("\nA. Current working directory: %s\n", current_dir_path);
		csay("gi", L"\n\n> Current working directory: %s", current_dir_path);
		addDirPathSepChar(current_dir_path);
		// say("\nB. Current working directory: %s\n", current_dir_path);
	}

	StringCchPrintfW(current_file_path, _countof(current_file_path), L"%scurl.exe", current_dir_path);

	search_handle = FindFirstFile(current_file_path, &file_data);

	if (search_handle == INVALID_HANDLE_VALUE) {
		tc("yi");
		say(L"\n\n> Could not find curl.exe downloader tool located in the current working directory!\n");
		say(L"\n> Trying to acquire curl.exe from %s...\n", gHttpUrlToCurl);
		csay("ri", L"\n> Downloading curl.exe... ");
		callback.updateCursorPosition();
		ShowCursor(console, false);
		start = time_now();
		status = URLDownloadToFileW(nullptr, gHttpUrlToCurl, current_file_path, 0, &callback);
		// status = URLDownloadToFileW(nullptr, gHttpUrlToCurl, current_file_path, 0, nullptr); // for win XP
		dt = time_elapsed(start);
		this_thread::sleep_for(milliseconds(250));
		// StringCchPrintfW(current_file_path, _countof(current_file_path), L"%scurl.exe", current_dir_path);
		ZeroMemory(&file_data, sizeof(WIN32_FIND_DATAW));
		search_handle = FindFirstFile(current_file_path, &file_data);
		ShowCursor(console, true);
		if ((status == S_OK) && (search_handle != INVALID_HANDLE_VALUE)) {
			// download_success = true;
			ds = static_cast<long>(floor(dt));
			dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
			dm = dh = 0;
			if (ds > 59)
			{
				dm = ds / 60;
				ds -= (dm * 60);
				if (dm > 59) {
					dh = dm / 60;
					dm -= (dh * 60);
				}
			}

			auto fHandle = HANDLE
			{
				CreateFile(current_file_path,
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr)
			};

			if (!fHandle) {
				file_size_long = 0;
			}
			else {
				auto size = LARGE_INTEGER{}; // union

				if (!GetFileSizeEx(fHandle, &size)) file_size_long = 0;
				else file_size_long = static_cast<long>(size.QuadPart);
				CloseHandle(fHandle);
			}

			if (file_size_long < 10240L) {
				// file_size = static_cast<float>(file_size_long);
				correct_size_word = L"bytes";
			}
			else if (file_size_long < 1048576L) {
				file_size = static_cast<float>(file_size_long) / 1024L;
				correct_size_word = L"KB";
			}
			else {
				file_size = static_cast<float>(file_size_long) / 1048576L;
				correct_size_word = L"MB";
			}

			StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%03ld ms", dm, ds, dms);
			if (file_size_long > 10240L)
				csay("gi", L"\n> Download of curl.exe (%.2f %s) has successfully completed in %s\n", file_size, correct_size_word, correct_time_word);
			else
				csay("gi", L"\n> Download of curl.exe (%ld %s) has successfully completed in %s\n", file_size_long, correct_size_word, correct_time_word);
		    
			// download_success = true;

			if (!FindClose(search_handle)) {
				displayErrorInfo();
			}

		}
		else {

			csay("ri", L"\n\n> Failed to download curl.exe from %s using the urlmon.h library!\n", gHttpUrlToCurl);
			csay("yi", L"\n> Trying to download curl.exe from %s using the wininet.h library...\n", gHttpUrlToCurl);

			use_wininet_to_download = false;

			const ::HINTERNET session = ::netstart();
			if (session != 0)
			{
				const ::HINTERNET istream = ::netopen(session, gHttpUrlToCurl);
				if (istream != 0)
				{
					std::ofstream ostream(current_file_path, std::ios::binary);
					if (ostream.is_open()) {
						start = time_now();
						::netfetch(istream, ostream);
						dt = time_elapsed(start);
						this_thread::sleep_for(milliseconds(250));
						// StringCchPrintfW(current_file_path, _countof(current_file_path), L"%scurl.exe", current_dir_path);
						ZeroMemory(&file_data, sizeof(WIN32_FIND_DATA));
						search_handle = FindFirstFile(current_file_path, &file_data);
						if (search_handle != INVALID_HANDLE_VALUE) {
							use_wininet_to_download = true;
							ds = static_cast<long>(floor(dt));
							dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
							dm = dh = 0;
							if (ds > 59)
							{
								dm = ds / 60;
								ds -= (dm * 60);
								if (dm > 59) {
									dh = dm / 60;
									dm -= (dh * 60);
								}
							}

							auto fHandle = HANDLE
							{
								CreateFile(current_file_path,
								GENERIC_READ,
								FILE_SHARE_READ,
								nullptr,
								OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL,
								nullptr)
							};

							if (!fHandle) {
								file_size_long = 0;
							}
							else {
								auto size = LARGE_INTEGER{}; // union

								if (!GetFileSizeEx(fHandle, &size)) file_size_long = 0;
								else file_size_long = static_cast<long>(size.QuadPart);
								CloseHandle(fHandle);
							}

							if (file_size_long < 10240L) {
								// file_size = static_cast<float>(file_size_long);
								correct_size_word = L"bytes";
							}
							else if (file_size_long < 1048576L) {
								file_size = static_cast<float>(file_size_long) / 1024L;
								correct_size_word = L"KB";
							}
							else {
								file_size = static_cast<float>(file_size_long) / 1048576L;
								correct_size_word = L"MB";
							}

							StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%03ld ms", dm, ds, dms);
							if (file_size_long > 10240L)
								csay("gi", L"\n> Download of curl.exe (%.2f %s) has successfully completed in %s\n", file_size, correct_size_word, correct_time_word);
							else
								csay("gi", L"\n> Download of curl.exe (%ld %s) has successfully completed in %s\n", file_size_long, correct_size_word, correct_time_word);
							
							// download_success = true;

							if (!FindClose(search_handle)) {
								displayErrorInfo();
							}
						}
						else {
							csay("ri", L"\n\n> Failed to download curl.exe from %s using the wininet.h library!\n", gHttpUrlToCurl);
							csay("yi", L"\n> Please make sure that you have a working internet connection\nand that your firewall software isn't blocking this program.\n");
							// download_success = false;
						}

					}
					else {
						csay("ri", L"\n\n> Failed to download curl.exe from %s using the wininet.h library!\n", gHttpUrlToCurl);
						csay("yi", L"\n> Please make sure that you have a working internet connection\nand that your firewall software isn't blocking this program.\n");
						// download_success = false;
					}

					::netclose(istream);

				}

				::netclose(session);

			}

		}
	}

	ZeroMemory(&file_data, sizeof(WIN32_FIND_DATAW));

	StringCchPrintfW(current_file_path, _countof(current_file_path), L"%slibcurl.dll", current_dir_path);

	search_handle = FindFirstFile(current_file_path, &file_data);

	if (search_handle == INVALID_HANDLE_VALUE) {
		tc("yi");
		say(L"\n\n> Could not find libcurl.dll downloader tool located in the current working directory!\n");
		say(L"\n> Trying to acquire libcurl.dll from %s...\n", gHttpUrlToLibCurlDll);
		csay("ri", L"\n> Downloading libcurl.dll... ");
		callback.updateCursorPosition();
		ShowCursor(console, false);
		start = time_now();
		status = URLDownloadToFileW(nullptr, gHttpUrlToLibCurlDll, current_file_path, 0, &callback);
		// status = URLDownloadToFileW(nullptr, gHttpUrlToLibCurlDll, current_file_path, 0, nullptr); // for win XP
		dt = time_elapsed(start);
		this_thread::sleep_for(milliseconds(250));
		// StringCchPrintfW(current_file_path, _countof(current_file_path), L"%slibcurl.dll", current_dir_path);
		ZeroMemory(&file_data, sizeof(WIN32_FIND_DATAW));
		search_handle = FindFirstFile(current_file_path, &file_data);
		ShowCursor(console, true);
		if ((status == S_OK) && (search_handle != INVALID_HANDLE_VALUE)) {
			// download_success = true;
			ds = static_cast<long>(floor(dt));
			dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
			dm = dh = 0;
			if (ds > 59)
			{
				dm = ds / 60;
				ds -= (dm * 60);
				if (dm > 59) {
					dh = dm / 60;
					dm -= (dh * 60);
				}
			}

			auto fHandle = HANDLE
			{
				CreateFile(current_file_path,
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr)
			};

			if (!fHandle) {
				file_size_long = 0;
			}
			else {
				auto size = LARGE_INTEGER{}; // union

				if (!GetFileSizeEx(fHandle, &size)) file_size_long = 0;
				else file_size_long = static_cast<long>(size.QuadPart);
				CloseHandle(fHandle);
			}

			if (file_size_long < 10240L) {
				// file_size = static_cast<float>(file_size_long);
				correct_size_word = L"bytes";
			}
			else if (file_size_long < 1048576L) {
				file_size = static_cast<float>(file_size_long) / 1024L;
				correct_size_word = L"KB";
			}
			else {
				file_size = static_cast<float>(file_size_long) / 1048576L;
				correct_size_word = L"MB";
			}

			StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%03ld ms", dm, ds, dms);
			if (file_size_long > 10240L)
				csay("gi", L"\n> Download of libcurl.dll (%.2f %s) has successfully completed in %s\n", file_size, correct_size_word, correct_time_word);
			else
				csay("gi", L"\n> Download of libcurl.dll (%ld %s) has successfully completed in %s\n", file_size_long, correct_size_word, correct_time_word);
			// download_success = true;

			if (!FindClose(search_handle)) {
				displayErrorInfo();
			}

		}
		else {

			csay("ri", L"\n\n> Failed to download libcurl.dll from %s using the urlmon.h library!\n", gHttpUrlToLibCurlDll);
			csay("yi", L"\n> Trying to download libcurl.dll from %s using the wininet library...\n", gHttpUrlToLibCurlDll);

			use_wininet_to_download = false;

			const ::HINTERNET session = ::netstart();
			if (session != 0)
			{
				const ::HINTERNET istream = ::netopen(session, gHttpUrlToLibCurlDll);
				if (istream != 0)
				{
					std::ofstream ostream(current_file_path, std::ios::binary);
					if (ostream.is_open()) {
						start = time_now();
						::netfetch(istream, ostream);
						dt = time_elapsed(start);
						this_thread::sleep_for(milliseconds(250));
						// StringCchPrintfW(current_file_path, _countof(current_file_path), L"%slibcurl.dll", current_dir_path);
						ZeroMemory(&file_data, sizeof(WIN32_FIND_DATA));
						search_handle = FindFirstFile(current_file_path, &file_data);
						if (search_handle != INVALID_HANDLE_VALUE) {
							use_wininet_to_download = true;
							ds = static_cast<long>(floor(dt));
							dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
							dm = dh = 0;
							if (ds > 59)
							{
								dm = ds / 60;
								ds -= (dm * 60);
								if (dm > 59) {
									dh = dm / 60;
									dm -= (dh * 60);
								}
							}

							auto fHandle = HANDLE
							{
								CreateFile(current_file_path,
								GENERIC_READ,
								FILE_SHARE_READ,
								nullptr,
								OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL,
								nullptr)
							};

							if (!fHandle) {
								file_size_long = 0;
							}
							else {
								auto size = LARGE_INTEGER{}; // union

								if (!GetFileSizeEx(fHandle, &size)) file_size_long = 0;
								else file_size_long = static_cast<long>(size.QuadPart);
								CloseHandle(fHandle);
							}

							if (file_size_long < 10240L) {
								// file_size = static_cast<float>(file_size_long);
								correct_size_word = L"bytes";
							}
							else if (file_size_long < 1048576L) {
								file_size = static_cast<float>(file_size_long) / 1024L;
								correct_size_word = L"KB";
							}
							else {
								file_size = static_cast<float>(file_size_long) / 1048576L;
								correct_size_word = L"MB";
							}

							StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%03ld ms", dm, ds, dms);
							if (file_size_long > 10240L)
								csay("gi", L"\n> Download of libcurl.dll (%.2f %s) has successfully completed in %s\n", file_size, correct_size_word, correct_time_word);
							else
								csay("gi", L"\n> Download of libcurl.dll (%ld %s) has successfully completed in %s\n", file_size_long, correct_size_word, correct_time_word);
							// download_success = true;

							if (!FindClose(search_handle)) {
								displayErrorInfo();
							}
						}
						else {
							csay("ri", L"\n\n> Failed to download libcurl.dll from %s using the wininet.h library!\n", gHttpUrlToLibCurlDll);
							csay("yi", L"\n> Please make sure that you have a working internet connection\nand that your firewall software isn't blocking this program.\n");
							// download_success = false;
						}

					}
					else {
						csay("ri", L"\n\n> Failed to download libcurl.dll from %s using the wininet.h library!\n", gHttpUrlToLibCurlDll);
						csay("yi", L"\n> Please make sure that you have a working internet connection\nand that your firewall software isn't blocking this program.\n");
						// download_success = false;
					}

					::netclose(istream);

				}

				::netclose(session);

			}
		}
	}

	ZeroMemory(&file_data, sizeof(WIN32_FIND_DATAW));

	StringCchPrintfW(current_file_path, _countof(current_file_path), L"%swget.exe", current_dir_path);

	search_handle = FindFirstFile(current_file_path, &file_data);

	if (search_handle == INVALID_HANDLE_VALUE) {
		tc("yi");
		say(L"\n\n> Could not find wget.exe downloader tool located in the current working directory!\n");
		say(L"\n> Trying to acquire wget.exe from %s...\n", gHttpUrlToWget);
		csay("ri", L"\n> Downloading wget.exe... ");
		callback.updateCursorPosition();
		ShowCursor(console, false);
		start = time_now();
		status = URLDownloadToFileW(nullptr, gHttpUrlToWget, current_file_path, 0, &callback);
		// status = URLDownloadToFileW(nullptr, gHttpUrlToWget, current_file_path, 0, nullptr); // for Windows XP
		dt = time_elapsed(start);
		this_thread::sleep_for(milliseconds(250));
		// StringCchPrintfW(current_file_path, _countof(current_file_path), L"%swget.exe", current_dir_path);
		ZeroMemory(&file_data, sizeof(WIN32_FIND_DATAW));
		search_handle = FindFirstFile(current_file_path, &file_data);
		ShowCursor(console, true);
		if ((status == S_OK) && (search_handle != INVALID_HANDLE_VALUE)) {
			ds = static_cast<long>(floor(dt));
			dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
			dm = dh = 0;
			if (ds > 59)
			{
				dm = ds / 60;
				ds -= (dm * 60);
				if (dm > 59) {
					dh = dm / 60;
					dm -= (dh * 60);
				}
			}

			auto fHandle = HANDLE
			{
				CreateFile(current_file_path,
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr)
			};

			if (!fHandle) {
				file_size_long = 0;
			}
			else {
				auto size = LARGE_INTEGER{}; // union

				if (!GetFileSizeEx(fHandle, &size)) file_size_long = 0;
				else file_size_long = static_cast<long>(size.QuadPart);
				CloseHandle(fHandle);
			}

			if (file_size_long < 10240L) {
				// file_size = static_cast<float>(file_size_long);
				correct_size_word = L"bytes";
			}
			else if (file_size_long < 1048576L) {
				file_size = static_cast<float>(file_size_long) / 1024L;
				correct_size_word = L"KB";
			}
			else {
				file_size = static_cast<float>(file_size_long) / 1048576L;
				correct_size_word = L"MB";
			}

			StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%03ld ms", dm, ds, dms);
			if (file_size_long > 10240L)
				csay("gi", L"\n> Download of wget.exe (%.2f %s) has successfully completed in %s\n", file_size, correct_size_word, correct_time_word);
			else
				csay("gi", L"\n> Download of wget.exe (%ld %s) has successfully completed in %s\n", file_size_long, correct_size_word, correct_time_word);
			// download_success = true;

			if (!FindClose(search_handle)) {
				displayErrorInfo();
			}

		}
		else {

			csay("ri", L"\n\n> Failed to download wget.exe from %s using the urlmon.h library!\n", gHttpUrlToWget);
			csay("yi", L"\n> Trying to download wget.exe from %s using the wininet.h library...\n", gHttpUrlToWget);

			use_wininet_to_download = false;

			const ::HINTERNET session = ::netstart();
			if (session != 0)
			{
				const ::HINTERNET istream = ::netopen(session, gHttpUrlToWget);
				if (istream != 0)
				{
					std::ofstream ostream(current_file_path, std::ios::binary);
					if (ostream.is_open()) {
						start = time_now();
						::netfetch(istream, ostream);
						dt = time_elapsed(start);
						this_thread::sleep_for(milliseconds(250));
						// StringCchPrintfW(current_file_path, _countof(current_file_path), L"%swget.exe", current_dir_path);
						ZeroMemory(&file_data, sizeof(WIN32_FIND_DATAW));
						search_handle = FindFirstFile(current_file_path, &file_data);
						if (search_handle != INVALID_HANDLE_VALUE) {
							use_wininet_to_download = true;
							ds = static_cast<long>(floor(dt));
							dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
							dm = dh = 0;
							if (ds > 59)
							{
								dm = ds / 60;
								ds -= (dm * 60);
								if (dm > 59) {
									dh = dm / 60;
									dm -= (dh * 60);
								}
							}

							auto fHandle = HANDLE
							{
								CreateFile(current_file_path,
								GENERIC_READ,
								FILE_SHARE_READ,
								nullptr,
								OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL,
								nullptr)
							};

							if (!fHandle) {
								file_size_long = 0;
							}
							else {
								auto size = LARGE_INTEGER{}; // union

								if (!GetFileSizeEx(fHandle, &size)) file_size_long = 0;
								else file_size_long = static_cast<long>(size.QuadPart);
								CloseHandle(fHandle);
							}

							if (file_size_long < 10240L) {
								// file_size = static_cast<float>(file_size_long);
								correct_size_word = L"bytes";
							}
							else if (file_size_long < 1048576L) {
								file_size = static_cast<float>(file_size_long) / 1024L;
								correct_size_word = L"KB";
							}
							else {
								file_size = static_cast<float>(file_size_long) / 1048576L;
								correct_size_word = L"MB";
							}

							StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%03ld ms", dm, ds, dms);
							if (file_size_long > 10240L)
								csay("gi", L"\n> Download of wget.exe (%.2f %s) has successfully completed in %s\n", file_size, correct_size_word, correct_time_word);
							else
								csay("gi", L"\n> Download of wget.exe (%ld %s) has successfully completed in %s\n", file_size_long, correct_size_word, correct_time_word);
							// download_success = true;

							if (!FindClose(search_handle)) {
								displayErrorInfo();
							}
						}
						else {
							csay("ri", L"\n\n> Failed to download wget.exe from %s using the wininet.h library!\n", gHttpUrlToWget);
							csay("yi", L"\n> Please make sure that you have a working internet connection\nand that your firewall software isn't blocking this program.\n");
							// download_success = false;
						}

					}
					else {
						csay("ri", L"\n\n> Failed to download wget.exe from %s using the wininet.h library!\n", gHttpUrlToWget);
						csay("yi", L"\n> Please make sure that you have a working internet connection\nand that your firewall software isn't blocking this program.\n");
						// download_success = false;
					}

					::netclose(istream);

				}

				::netclose(session);

			}
		}
	}

	csay("ci", L"\n\n> Downloading updated list of Verindra's custom mod and map files for your %s game...\n\n", gLongGameName.c_str());

	StringCchPrintfW(file_name, _countof(file_name), L"updated_list_of_verindra_%s_mod_and_custom_map_files.lst", gShortGameName.c_str());
	StringCchPrintfW(new_file_path, _countof(new_file_path), L"%supdated_list_of_verindra_%s_mod_and_custom_map_files.lst", current_dir_path, gShortGameName.c_str());

	download_success = false;

	if (use_curl_to_download) {		
		csay("yi", L"\n> Trying to download %s using curl.exe...\n", file_name);
		StringCchPrintfW(download_link, _countof(download_link),
			L"curl.exe --progress-bar -4 --output \"%s\" -R --url %s", new_file_path, gHttpUrlToUpdatedList);
		wcstombs_s(&convertedChars, abuffer, download_link, _TRUNCATE);
		start = time_now();
		system(abuffer);
		dt = time_elapsed(start);
		this_thread::sleep_for(milliseconds(250));
		// StringCchPrintfW(current_file_path, _countof(current_file_path), L"%supdated_list_of_verindra_%s_mod_and_custom_map_files.lst", current_dir_path, gShortGameName.c_str());
		ZeroMemory(&file_data, sizeof(WIN32_FIND_DATAW));
		search_handle = FindFirstFile(new_file_path, &file_data);
		if (search_handle != INVALID_HANDLE_VALUE) {
			// download_success = true;
			ds = static_cast<long>(floor(dt));
			dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
			dm = dh = 0;
			if (ds > 59)
			{
				dm = ds / 60;
				ds -= (dm * 60);
				if (dm > 59) {
					dh = dm / 60;
					dm -= (dh * 60);
				}
			}

			auto fHandle = HANDLE
			{
				CreateFile(new_file_path,
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr)
			};

			if (!fHandle) {
				file_size_long = 0;
			}
			else {
				auto size = LARGE_INTEGER{}; // union

				if (!GetFileSizeEx(fHandle, &size)) file_size_long = 0;
				else file_size_long = static_cast<long>(size.QuadPart);
				CloseHandle(fHandle);
			}

			if (file_size_long < 10240L) {
				// file_size = static_cast<float>(file_size_long);
				correct_size_word = L"bytes";
			}
			else if (file_size_long < 1048576L) {
				file_size = static_cast<float>(file_size_long) / 1024L;
				correct_size_word = L"kB";
			}
			else {
				file_size = static_cast<float>(file_size_long) / 1048576L;
				correct_size_word = L"MB";
			}

			StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%03ld ms", dm, ds, dms);
			if (file_size_long > 10240L)
				csay("gi", L"\n> Download of %s (%.2f %s)\n has successfully completed in %s\n", file_name, file_size, correct_size_word, correct_time_word);
			else
				csay("gi", L"\n> Download of %s (%ld %s)\n has successfully completed in %s\n", file_name, file_size_long, correct_size_word, correct_time_word);

			download_success = true;

			if (!FindClose(search_handle)) {
				displayErrorInfo();
			}
		} else
		{
			csay("ri", L"\n> Failed to download %s using curl.exe!\n", file_name);
			download_success = false;
		}
	}

	if (!download_success && use_wget_to_download) {
		csay("yi", L"\n> Trying to download %s using wget.exe...\n", file_name);
		// wget.exe -q --show-progress --progress=bar:forced:noscroll -4 -c --tries=10 --ouput-document=wget.exe gHttpUrlToUpdatedList
		StringCchPrintfW(download_link, _countof(download_link),
			L"wget.exe -q --show-progress --progress=bar:forced:noscroll -4 --tries=10 --output-document=\"%s\" %s", new_file_path, gHttpUrlToUpdatedList);		
		wcstombs_s(&convertedChars, abuffer, download_link, _TRUNCATE);
		start = time_now();
		system(abuffer);
		dt = time_elapsed(start);
		this_thread::sleep_for(milliseconds(250));
		// StringCchPrintfW(current_file_path, _countof(current_file_path), L"%supdated_list_of_verindra_%s_mod_and_custom_map_files.lst", current_dir_path, gShortGameName.c_str());
		ZeroMemory(&file_data, sizeof(WIN32_FIND_DATAW));
		search_handle = FindFirstFile(new_file_path, &file_data);
		if (search_handle != INVALID_HANDLE_VALUE) {
			// download_success = true;
			ds = static_cast<long>(floor(dt));
			dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
			dm = dh = 0;
			if (ds > 59)
			{
				dm = ds / 60;
				ds -= (dm * 60);
				if (dm > 59) {
					dh = dm / 60;
					dm -= (dh * 60);
				}
			}

			auto fHandle = HANDLE
			{
				CreateFile(new_file_path,
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr)
			};

			if (!fHandle) {
				file_size_long = 0;
			}
			else {
				auto size = LARGE_INTEGER{}; // union

				if (!GetFileSizeEx(fHandle, &size)) file_size_long = 0;
				else file_size_long = static_cast<long>(size.QuadPart);
				CloseHandle(fHandle);
			}

			if (file_size_long < 10240L) {
				// file_size = static_cast<float>(file_size_long);
				correct_size_word = L"bytes";
			}
			else if (file_size_long < 1048576L) {
				file_size = static_cast<float>(file_size_long) / 1024L;
				correct_size_word = L"KB";
			}
			else {
				file_size = static_cast<float>(file_size_long) / 1048576L;
				correct_size_word = L"MB";
			}

			StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%03ld ms", dm, ds, dms);
			if (file_size_long > 10240L)
				csay("gi", L"\n> Download of %s (%.2f %s)\n has successfully completed in %s\n", new_file_path, file_size, correct_size_word, correct_time_word);
			else
				csay("gi", L"\n> Download of %s (%ld %s)\n has successfully completed in %s\n", new_file_path, file_size_long, correct_size_word, correct_time_word);

			download_success = true;

			if (!FindClose(search_handle)) {
				displayErrorInfo();
			}
		} else {
			csay("ri", L"\n> Failed to download %s using wget.exe!\n", file_name);
			download_success = false;
		}

	}

	if (!download_success && (use_urlmon_to_download || use_wininet_to_download)) {
		csay("yi", L"\n> Trying to download %s using the <urlmon.h> library...\n", file_name);
		callback.updateCursorPosition();
		ShowCursor(console, false);
		start = time_now();
		status = URLDownloadToFileW(nullptr, gHttpUrlToUpdatedList, new_file_path, 0, &callback);
		// status = URLDownloadToFileW(nullptr, gHttpUrlToUpdatedList, new_file_path, 0, nullptr);
		dt = time_elapsed(start);
		this_thread::sleep_for(milliseconds(250));
		// StringCchPrintfW(current_file_path, _countof(current_file_path), L"%supdated_list_of_verindra_%s_mod_and_custom_map_files.lst", current_dir_path, gShortGameName.c_str());
		ZeroMemory(&file_data, sizeof(WIN32_FIND_DATAW));
		search_handle = FindFirstFile(new_file_path, &file_data);
		ShowCursor(console, true);
		if ((status == S_OK) && (search_handle != INVALID_HANDLE_VALUE)) {
			// download_success = true;
			ds = static_cast<long>(floor(dt));
			dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
			dm = dh = 0;
			if (ds > 59)
			{
				dm = ds / 60;
				ds -= (dm * 60);
				if (dm > 59) {
					dh = dm / 60;
					dm -= (dh * 60);
				}
			}

			auto fHandle = HANDLE
			{
				CreateFile(new_file_path,
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr)
			};

			if (!fHandle) {
				file_size_long = 0;
			}
			else {
				auto size = LARGE_INTEGER{}; // union

				if (!GetFileSizeEx(fHandle, &size)) file_size_long = 0;
				else file_size_long = static_cast<long>(size.QuadPart);
				CloseHandle(fHandle);
			}

			if (file_size_long < 10240L) {
				// file_size = static_cast<float>(file_size_long);
				correct_size_word = L"bytes";
			}
			else if (file_size_long < 1048576L) {
				file_size = static_cast<float>(file_size_long) / 1024L;
				correct_size_word = L"KB";
			}
			else {
				file_size = static_cast<float>(file_size_long) / 1048576L;
				correct_size_word = L"MB";
			}

			StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%03ld ms", dm, ds, dms);
			if (file_size_long > 10240L)
				csay("gi", L"\n> Download of %s (%.2f %s)\n has successfully completed in %s\n", new_file_path, file_size, correct_size_word, correct_time_word);
			else
				csay("gi", L"\n> Download of %s (%ld %s)\n has successfully completed in %s\n", new_file_path, file_size_long, correct_size_word, correct_time_word);

			download_success = true;

			if (!FindClose(search_handle)) {
				displayErrorInfo();
			}

		}
		else {

			download_success = false;
			csay("ri", L"\n> Failed to download %s using <urlmon.h> library!\n", file_name);			
			csay("yi", L"\n> Trying to download %s using the <wininet.h> library...\n", file_name);
			use_wininet_to_download = false;

			const ::HINTERNET session = ::netstart();
			if (session != 0)
			{
				const ::HINTERNET istream = ::netopen(session, gHttpUrlToUpdatedList);
				if (istream != 0)
				{
					std::ofstream ostream(new_file_path, std::ios::binary);
					if (ostream.is_open()) {
						start = time_now();
						::netfetch(istream, ostream);
						dt = time_elapsed(start);
						this_thread::sleep_for(milliseconds(250));
						// StringCchPrintfW(current_file_path, _countof(current_file_path), L"%supdated_list_of_verindra_%s_mod_and_custom_map_files.lst", current_dir_path, gShortGameName.c_str());
						ZeroMemory(&file_data, sizeof(WIN32_FIND_DATAW));
						search_handle = FindFirstFile(new_file_path, &file_data);
						if (search_handle != INVALID_HANDLE_VALUE) {
							use_wininet_to_download = true;
							ds = static_cast<long>(floor(dt));
							dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
							dm = dh = 0;
							if (ds > 59)
							{
								dm = ds / 60;
								ds -= (dm * 60);
								if (dm > 59) {
									dh = dm / 60;
									dm -= (dh * 60);
								}
							}

							auto fHandle = HANDLE
							{
								CreateFile(new_file_path,
								GENERIC_READ,
								FILE_SHARE_READ,
								nullptr,
								OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL,
								nullptr)
							};

							if (!fHandle) {
								file_size_long = 0;
							}
							else {
								auto size = LARGE_INTEGER{}; // union

								if (!GetFileSizeEx(fHandle, &size)) file_size_long = 0;
								else file_size_long = static_cast<long>(size.QuadPart);
								CloseHandle(fHandle);
							}

							if (file_size_long < 10240L) {
								// file_size = static_cast<float>(file_size_long);
								correct_size_word = L"bytes";
							}
							else if (file_size_long < 1048576L) {
								file_size = static_cast<float>(file_size_long) / 1024L;
								correct_size_word = L"KB";
							}
							else {
								file_size = static_cast<float>(file_size_long) / 1048576L;
								correct_size_word = L"MB";
							}

							StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%03ld ms", dm, ds, dms);
							if (file_size_long > 10240L)
								csay("gi", L"\n> Download of %s (%.2f %s) has successfully completed in %s\n", new_file_path, file_size, correct_size_word, correct_time_word);
							else
								csay("gi", L"\n> Download of %s (%ld %s) has successfully completed in %s\n", new_file_path, file_size_long, correct_size_word, correct_time_word);

							download_success = true;

							if (!FindClose(search_handle)) {
								displayErrorInfo();
							}
						}
						else {
							csay("ri", L"\n> Error: could not download updated list of Verindra's custom mod and map files for your %s game!\n", gLongGameName.c_str());
							csay("yi", L"\n> Please make sure that you have a working internet connection\nand that your firewall software isn't blocking this program.\n");

							download_success = false;
						}

					}
					else {
						csay("ri", L"\n> Error: could not download updated list of Verindra's custom mod and map files for your %s game!\n", gLongGameName.c_str());
						csay("yi", L"\n> Please make sure that you have a working internet connection\nand that your firewall software isn't blocking this program.\n");

						download_success = false;
					}

					::netclose(istream);

				}

				::netclose(session);

			}
		}
	}

	if (!download_success) {
		csay("ri", L"\n> Error: could not download updated list of Verindra's custom mod and map files for your %s game!\n", gLongGameName.c_str());
		csay("yi", L"\n> Please make sure that you have a working internet connection\nand that your firewall software isn't blocking this program's correct execution.\n");
		return false;
	}

	// csay("gi", "\n> Download of updated list of Verindra's custom mod and map files has successfully completed.\n");
	csay("yi", L"\n> Beginning parsing necessary information from downloaded list of custom mod and map file entries...\n");

	auto file_input = wifstream(new_file_path, ios::in);
	auto text_line = wstring{};
	auto trimmed_line = wstring{};
	auto previous_folder = wstring{ L"" };
	auto current_folder = wstring{};
	// auto file_name = wstring{};
	auto md5line = wstring{};
	// auto file_size = long{};
	bool isNewFolder = false;

	while (getline(file_input, text_line)) {
		trimmed_line = trim(text_line);

		if (empty(trimmed_line)) {
			isNewFolder = false;
			if (previous_folder != L"") {
				neededFilesData[previous_folder] = fileData;
				fileData.clear();
			}
			// previous_folder = current_folder;
			current_folder = L"";
			continue;
		}

		if ((trimmed_line[0] == L'#') || (trimmed_line[0] == L';')) continue;

		if (!isNewFolder && (trimmed_line[0] == L'[')) {
			auto end = trimmed_line.find(L']', 1);

			if (end != wstring::npos) {
				current_folder = trimmed_line.substr(1, end - 1);
				isNewFolder = true;
				previous_folder = current_folder;
			}

			continue;
		}

		if (isNewFolder) {
			auto const& file = processDataLine(trimmed_line);

			fileData[file.file_name_] = file;
		}
	}

	if (current_folder != L"") {
		neededFilesData[current_folder] = fileData;
		fileData.clear();
	}

	file_input.close();
	DeleteFile(new_file_path);
	csay("gi", L"\n> Parsing necessary information from downloaded list has successfully completed.\n");

	return true;
}

bool scanAndProcessLocalGameFolderContents(const wchar_t* gamePath) {
	auto md5checksum = wstring{};
	BOOL status;
	// auto error_no = DWORD{};
	long fileSize;

	if (!gamePath || !(*gamePath)) {
		csay("ri", L"> Selected game path cannot be nullptr or empty character string!\n");
		return false;
	}

	say(L"\n");

	if (gShortGameName == L"cod2") {
		StringCchPrintfW(game_folder_path, _countof(game_folder_path), L"\\\\?\\%s\\temp_main", gamePath);

		// csay("yi", L"\n> Creating necessary folder (temp_main) for storing temporary custom mod and map files...");
		status = CreateDirectory(game_folder_path, nullptr);

		if (status != 0) {
			// csay("gi", L"\n> Necessary folder (temp_main) has been successfully created.");
			gDSI.number_of_mod_folders_created++;
		}
		/*else {

			reportCorrectErrorInformationForCreateDirectory(GetLastError(), L"temp_main");
		}*/
	}
	else if (gShortGameName == L"cod4") {
		// Create necessary parent directories for holding subdirectories of downloaded custom mod and map files: Mods, usermaps
		// Create 'Mods' parent directory
		StringCchPrintfW(game_folder_path, _countof(game_folder_path), L"\\\\?\\%s\\Mods", gamePath);

		// csay("yi", L"\n> Creating necessary folder: Mods...");
		status = CreateDirectory(game_folder_path, nullptr);

		if (status != 0) {
			// csay("gi", L"\n> Necessary folder (Mods) has been successfully created.");
			gDSI.number_of_mod_folders_created++;
		}
		/*else {
			reportCorrectErrorInformationForCreateDirectory(GetLastError(), L"Mods");
		}*/

		// Create 'usermaps' parent directory
		StringCchPrintfW(game_folder_path, _countof(game_folder_path), L"\\\\?\\%s\\usermaps", gamePath);

		// csay("yi", L"\n> Creating necessary folder: usermaps...");
		status = CreateDirectory(game_folder_path, nullptr);

		if (status != 0) {
			// csay("gi", L"\n> Missing folder (usermaps) has been successfully created.");
			gDSI.number_of_mod_folders_created++;
		}/*else {
			reportCorrectErrorInformationForCreateDirectory(GetLastError(), L"usermaps");
		}*/
	}

	for (auto& data : neededFilesData) {
		StringCchPrintfW(game_folder_path, _countof(game_folder_path), L"\\\\?\\%s\\%s", gamePath, data.first.c_str());

		status = CreateDirectory(game_folder_path, nullptr);

		if (status != 0) {
			// csay("gi", L"\n> Required game folder (%s) has been successfully created.", data.first.c_str());
			gDSI.number_of_mod_folders_created++;
		} /*else {
			reportCorrectErrorInformationForCreateDirectory(GetLastError(), data.first.c_str());
		}*/

		// csay("mi", L"> Entering specified folder path (%s)\n> and starting to search for *.iwd, *.ff and *.tmp files...\n", game_folder_path);

		StringCchPrintfW(search_file_path, _countof(search_file_path), L"%s\\*.iwd", game_folder_path);

		// csay("mi", L"> search_file_path = \"%s\"\n", search_file_path);

		WIN32_FIND_DATA file_data{};

		BOOL search_status{};

		auto search_handle = FindFirstFile(search_file_path, &file_data);

		if (search_handle != INVALID_HANDLE_VALUE) {
			do {
				if (endsWith(file_data.cFileName, L".iwd", true) && !startsWith(file_data.cFileName, L"iw", true) && !startsWith(file_data.cFileName, L"localized_", true)) {
					// csay("mi", L"> Current file: %s\n", file_data.cFileName);

					StringCchPrintfW(old_file_path, _countof(old_file_path), L"%s\\%s", game_folder_path, file_data.cFileName);

					if (data.second.find(file_data.cFileName) != data.second.end()) {
						
						if (!startsWith(data.first.c_str(), L"usermaps", true)) {

							auto f = Mapped_File_In_Memory{ old_file_path };

							if (!f) {
								csay("ri", L"\n> Error mapping file: %s into program's address space!\n", old_file_path);
								f.unmap();

								auto fHandle = HANDLE
								{
									CreateFile(old_file_path,
									GENERIC_READ,
									FILE_SHARE_READ,
									nullptr,
									OPEN_EXISTING,
									FILE_ATTRIBUTE_NORMAL,
									nullptr)
								};

								if (!fHandle) {
									fileSize = 0;
								}
								else {
									auto size = LARGE_INTEGER{}; // union

									if (!GetFileSizeEx(fHandle, &size)) fileSize = 0;
									else fileSize = static_cast<long>(size.QuadPart);
									// md5checksum = L"";
									CloseHandle(fHandle);
								}

								if (data.second[file_data.cFileName].file_size_ == fileSize) {
									data.second.erase(file_data.cFileName);
									if (data.first == L"main") {
										csay("gi", L"> Skipping processing of already existing, required custom map file: %s...\n", file_data.cFileName);
									}
									else {
										csay("gi", L"> Skipping processing of already existing, required custom mod file: %s...\n", file_data.cFileName);
									}
								}
								else {
									if (data.first == L"main") { // main, usermaps\\map_name_sub_dir
										csay("yi", L"> Removing potentially conflicting, unnecessary custom map file: %s...\n", file_data.cFileName);
										gDSI.number_of_map_files_deleted++;
									}
									else {
										csay("yi", L"> Removing potentially conflicting, unnecessary custom mod file: %s...\n", file_data.cFileName);
										gDSI.number_of_mod_files_deleted++;
									}

									DeleteFile(old_file_path);
								}

								continue;
							}
							else {
								fileSize = f.get_file_size();
								// csay("mi", L"> File size(%s) -> %ld bytes\n", file_data.cFileName, fileSize);
								md5checksum = md5ws(f.begin(), fileSize);
								f.unmap();
								// csay("mi", L"> MD5(%s) -> %s\n", file_data.cFileName, md5checksum.c_str());
							}
						}
						else {
							auto fHandle = HANDLE
							{
								CreateFile(old_file_path,
								GENERIC_READ,
								FILE_SHARE_READ,
								nullptr,
								OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL,
								nullptr)
							};

							if (!fHandle) {
								fileSize = 0;
							}
							else {
								auto size = LARGE_INTEGER{}; // union

								if (!GetFileSizeEx(fHandle, &size)) fileSize = 0;
								else fileSize = static_cast<long>(size.QuadPart);

								CloseHandle(fHandle);
							}

							md5checksum = L"";
						}

						if (((data.second[file_data.cFileName].md5_hash_value_ == md5checksum) || startsWith(data.first.c_str(), L"usermaps", true)) &&
							(data.second[file_data.cFileName].file_size_ == fileSize)) {
							data.second.erase(file_data.cFileName);
							if ((data.first == L"main") || startsWith(data.first.c_str(), L"usermaps", true)) {
								csay("gi", L"> Skipping processing of already existing, required custom map file: %s...\n", file_data.cFileName);
							}
							else {
								csay("gi", L"> Skipping processing of already existing, required custom mod file: %s...\n", file_data.cFileName);
							}
						}
						else {
							if ((data.first == L"main") || startsWith(data.first.c_str(), L"usermaps", true)) { // main, usermaps\\map_name_sub_dir
								csay("yi", L"> Removing potentially conflicting, unnecessary custom map file: %s...\n", file_data.cFileName);
								gDSI.number_of_map_files_deleted++;
							}
							else {
								csay("yi", L"> Removing potentially conflicting, unnecessary custom mod file: %s...\n", file_data.cFileName);
								gDSI.number_of_mod_files_deleted++;
							}

							DeleteFile(old_file_path);
						}
					}
					else if (data.first == L"main") {
						StringCchPrintfW(new_file_path, _countof(new_file_path), L"\\\\?\\%s\\temp_main\\%s", gamePath, file_data.cFileName);
						csay("yi", L"> Moving unnecessary custom map/mod file %s to temp_main directory...\n", file_data.cFileName);
						if (!MoveFileEx(old_file_path, new_file_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
							displayErrorInfo();
						}
						gDSI.number_of_map_files_moved++;
					}
					else {
						csay("yi", L"> Removing potentially conflicting, unnecessary custom map/mod file %s...\n", file_data.cFileName);
						DeleteFile(old_file_path);
						gDSI.number_of_mod_files_deleted++;
					}
				}

				search_status = FindNextFile(search_handle, &file_data);

			} while (search_status != 0);

			if (!FindClose(search_handle)) {
				displayErrorInfo();
			}
		}

		StringCchPrintfW(search_file_path, _countof(search_file_path), L"%s\\*.tmp", game_folder_path);

		// csay("mi", L"> search_file_path = \"%s\"\n", search_file_path);

		ZeroMemory(&file_data, sizeof(WIN32_FIND_DATA));

		// search_status = 0;

		search_handle = FindFirstFile(search_file_path, &file_data);

		if (search_handle != INVALID_HANDLE_VALUE) {
			do {
				if (endsWith(file_data.cFileName, L".tmp", true)) {
					// csay("mi", L"> Current file: %s\n", file_data.cFileName);

					if (data.first == L"main") {
						csay("yi", L"> Removing unnecessary temporary custom map file %s...\n", file_data.cFileName);
						gDSI.number_of_map_files_deleted++;
					}
					else {
						csay("yi", L"> Removing unnecessary temporary custom mod file %s...\n", file_data.cFileName);
						gDSI.number_of_mod_files_deleted++;
					}

					DeleteFile(old_file_path);
				}
			} while ((search_status = FindNextFile(search_handle, &file_data)) != 0);

			if (!FindClose(search_handle)) {
				displayErrorInfo();
			}
		}

		if (gShortGameName == L"cod4") {
			StringCchPrintfW(search_file_path, _countof(search_file_path), L"%s\\*.ff", game_folder_path);

			// csay("mi", L"> search_file_path = \"%s\"\n", search_file_path);

			ZeroMemory(&file_data, sizeof(WIN32_FIND_DATA));

			search_status = 0;

			search_handle = FindFirstFile(search_file_path, &file_data);

			if (search_handle != INVALID_HANDLE_VALUE) {
				do {
					if (endsWith(file_data.cFileName, L".ff", true)) {
						// csay("mi", L"> Current file: %s\n", file_data.cFileName);

						StringCchPrintfW(old_file_path, _countof(old_file_path), L"%s\\%s", game_folder_path, file_data.cFileName);

						if (data.second.find(file_data.cFileName) != data.second.end()) {
							if (!startsWith(data.first.c_str(), L"usermaps", true)) {
								auto f = Mapped_File_In_Memory{ old_file_path };

								if (!f) {
									csay("ri", L"> Error mapping file: %s into program's address space!\n", old_file_path);
									f.unmap();
									auto fHandle = HANDLE
									{
										CreateFile(old_file_path,
										GENERIC_READ,
										FILE_SHARE_READ,
										nullptr,
										OPEN_EXISTING,
										FILE_ATTRIBUTE_NORMAL,
										nullptr)
									};

									if (!fHandle) {
										fileSize = 0;
									}
									else {
										auto size = LARGE_INTEGER{}; // union

										if (!GetFileSizeEx(fHandle, &size)) fileSize = 0;
										else fileSize = static_cast<long>(size.QuadPart);
										CloseHandle(fHandle);
									}

									if (data.second[file_data.cFileName].file_size_ == fileSize) {
										data.second.erase(file_data.cFileName);
										csay("gi", L"> Skipping processing of already existing, required custom mod file: %s...\n", file_data.cFileName);
									}
									else {
										csay("yi", L"> Removing potentially conflicting, unnecessary custom mod file: %s...\n", file_data.cFileName);
										gDSI.number_of_mod_files_deleted++;
									}

									DeleteFile(old_file_path);

									continue;
								}
								else {
									fileSize = f.get_file_size();
									// csay("mi", L"> File size(%s) -> %ld bytes\n", file_data.cFileName, fileSize);
									md5checksum = md5ws(f.begin(), fileSize);
									f.unmap();
									// csay("mi", L"> MD5(%s) -> %s\n", file_data.cFileName, md5checksum.c_str());
								}
							}
							else {
								auto fHandle = HANDLE
								{
									CreateFile(old_file_path,
									GENERIC_READ,
									FILE_SHARE_READ,
									nullptr,
									OPEN_EXISTING,
									FILE_ATTRIBUTE_NORMAL,
									nullptr)
								};

								if (!fHandle) {
									fileSize = 0;
								}
								else {
									auto size = LARGE_INTEGER{}; // union
									if (!GetFileSizeEx(fHandle, &size)) fileSize = 0;
									else fileSize = static_cast<long>(size.QuadPart);
									CloseHandle(fHandle);
								}

								md5checksum = L"";
							}

							if (((data.second[file_data.cFileName].md5_hash_value_ == md5checksum) || startsWith(data.first.c_str(), L"usermaps", true)) &&
								(data.second[file_data.cFileName].file_size_ == fileSize)) {
								data.second.erase(file_data.cFileName);
								if (startsWith(data.first.c_str(), L"usermaps", true)) {
									csay("gi", L"> Skipping processing of already existing, required custom map file: %s...\n", file_data.cFileName);
								}
								else {
									csay("gi", L"> Skipping processing of already existing, required custom mod file: %s...\n", file_data.cFileName);
								}
							}
							else {
								if (startsWith(data.first.c_str(), L"usermaps", true)) { // main, usermaps\\map_name_sub_dir
									csay("yi", L"> Removing potentially conflicting, unnecessary custom map file: %s...\n", file_data.cFileName);
									gDSI.number_of_map_files_deleted++;
								}
								else {
									csay("yi", L"> Removing potentially conflicting, unnecessary custom mod file: %s...\n", file_data.cFileName);
									gDSI.number_of_mod_files_deleted++;
								}

								DeleteFile(old_file_path);
							}
						}
						else {
							csay("yi", L"> Removing potentially conflicting, unnecessary custom map/mod file %s...\n", file_data.cFileName);
							DeleteFile(old_file_path);
							gDSI.number_of_mod_files_deleted++;
						}
					}

					search_status = FindNextFile(search_handle, &file_data);
				
				} while (search_status != 0);

				if (!FindClose(search_handle)) {
					displayErrorInfo();
				}
			}
		}
	}

	return true;
}

bool downloadMissingVerindraModFilesAndCustomMaps(const wchar_t* gamePath) {
	static wchar_t correct_time_word[128];
	auto convertedChars = size_t{};
	int key;
	auto status = HRESULT{};
	auto stop_downloading = bool{};
	bool download_success;
	const wchar_t* correct_word;
	const wchar_t* correct_size_word;
	auto cod4_url_folder_path = wstring{};
	// auto rs = float{};
	float dt;
	// auto dmsf = float{};
	long dms, ds, dm, dh;
	float file_size;
	auto start = high_resolution_clock::time_point{};

	auto find_data = WIN32_FIND_DATAW{};
	HANDLE search_handle;
	// auto res = DWORD{};

	auto callback = ProgressCallback
	{
		console,
		GetPosition(console)
	};

	ShowCursor(console, false);

	auto const da_start_time = time_now();

	for (auto const& folder_data : neededFilesData) {

		if (stop_downloading) break;

		// StringCchPrintfW(game_folder_path, _countof(game_folder_path), L"%s\\%s\\", gamePath, folder_data.first.c_str());

		/*
		if (!SetCurrentDirectoryW(game_folder_path)) {
			displayErrorInfo();
		}
		*/

		for (auto const& file_data : folder_data.second) {
			StringCchPrintfW(new_file_path, _countof(new_file_path), L"%s\\%s\\%s", gamePath, folder_data.first.c_str(), file_data.first.c_str());

			if (!startsWith(folder_data.first.c_str(), L"main", true) && !startsWith(folder_data.first.c_str(), L"usermaps", true)) correct_word = L"custom mod file:";
			else correct_word = L"custom map file:";

			if (file_data.second.file_size_ < 1048576L) {
				file_size = static_cast<float>(file_data.second.file_size_) / 1024L;
				correct_size_word = L"kB";
			}
			else {
				file_size = static_cast<float>(file_data.second.file_size_) / 1048576L;
				correct_size_word = L"MB";
			}

			download_success = false;

			cod4_url_folder_path = folder_data.first;
			if (gShortGameName == L"cod4") replaceBackslashCharWithForwardSlash(cod4_url_folder_path);
			csay("ci", L"\n> Downloading missing %s %s (%.2f %s) ...\n\n", correct_word, file_data.first.c_str(), file_size, correct_size_word);

			if (use_curl_to_download) {
				StringCchPrintfW(download_link, _countof(download_link),
					L"curl.exe --progress-bar -4 --output \"%s\" -R --url %s/%s/%s", new_file_path, gHttpUrlToGameFiles,
					cod4_url_folder_path.c_str(), file_data.first.c_str());
				wcstombs_s(&convertedChars, abuffer, download_link, _TRUNCATE);
				start = time_now();
				system(abuffer);				
				dt = time_elapsed(start);
				this_thread::sleep_for(milliseconds(250));
				ZeroMemory(&find_data, sizeof(find_data));
				search_handle = FindFirstFileW(new_file_path, &find_data);
				if (search_handle != INVALID_HANDLE_VALUE)
				{
					ds = static_cast<long>(floor(dt));
					dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
					dm = dh = 0;
					if (ds > 59)
					{
						dm = ds / 60;
						ds -= (dm * 60);
						if (dm > 59) {
							dh = dm / 60;
							dm -= (dh * 60);
						}
					}

					if (!FindClose(search_handle)) {
						displayErrorInfo();
					}

					StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%02ld:%03ld ms", dh, dm, ds, dms);
					csay("gi", L"\n> Download of %s (%.2f %s) has successfully completed in %s\n", file_data.first.c_str(), file_size, correct_size_word, correct_time_word);
					download_success = true;
				}
				else
				{
					csay("ri", L"\n> Failed to download %s (%.2f %s)!\n", file_data.first.c_str(), file_size, correct_size_word);
					download_success = false;
				}
			}

			if (!download_success && use_wget_to_download) {

				csay("ci", L"\n> Downloading missing %s %s (%.2f %s) ...\n\n", correct_word, file_data.first.c_str(), file_size, correct_size_word);

				StringCchPrintfW(download_link, _countof(download_link),
					L"wget.exe -q --show-progress --progress=bar:forced:noscroll -4 --tries=10 --output-document=\"%s\" %s/%s/%s",
					new_file_path, gHttpUrlToGameFiles, cod4_url_folder_path.c_str(), file_data.first.c_str());
				wcstombs_s(&convertedChars, abuffer, download_link, _TRUNCATE);
				start = time_now();
				system(abuffer);
				dt = time_elapsed(start);
				this_thread::sleep_for(milliseconds(250));
				ZeroMemory(&find_data, sizeof(find_data));
				search_handle = FindFirstFileW(new_file_path, &find_data);
				if (search_handle != INVALID_HANDLE_VALUE)
				{
					ds = static_cast<long>(floor(dt));
					dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
					dm = dh = 0;
					if (ds > 59)
					{
						dm = ds / 60;
						ds -= (dm * 60);
						if (dm > 59) {
							dh = dm / 60;
							dm -= (dh * 60);
						}
					}

					if (!FindClose(search_handle)) {
						displayErrorInfo();
					}

					StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%02ld:%03ld ms", dh, dm, ds, dms);
					csay("gi", L"\n> Download of %s (%.2f %s) has successfully completed in %s\n", file_data.first.c_str(), file_size, correct_size_word, correct_time_word);
					download_success = true;
				}
				else
				{
					csay("ri", L"\n> Failed to download %s (%.2f %s)!\n", file_data.first.c_str(), file_size, correct_size_word);
					download_success = false; // LONG_PTR <=> long

				}

			}

			if (!download_success && use_urlmon_to_download) {
				csay("ci", L"\n> Downloading missing %s %s (%.2f %s) ... ", correct_word, file_data.first.c_str(), file_size, correct_size_word);
				callback.updateCursorPosition();
				ShowCursor(console, false);
				StringCchPrintfW(download_link, _countof(download_link), L"%s/%s/%s", gHttpUrlToGameFiles, cod4_url_folder_path.c_str(), file_data.first.c_str());
				start = time_now();
				status = URLDownloadToFileW(nullptr, download_link, new_file_path, 0, &callback);
				// status = URLDownloadToFileW(nullptr, download_link, new_file_path, 0, nullptr);
				dt = time_elapsed(start);
				this_thread::sleep_for(milliseconds(250));
				ZeroMemory(&find_data, sizeof(find_data));
				search_handle = FindFirstFileW(new_file_path, &find_data);
				if ((status == S_OK) && (search_handle != INVALID_HANDLE_VALUE))
				{
					ds = static_cast<long>(floor(dt));
					dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
					dm = dh = 0;
					if (ds > 59)
					{
						dm = ds / 60;
						ds -= (dm * 60);
						if (dm > 59) {
							dh = dm / 60;
							dm -= (dh * 60);
						}
					}

					if (!FindClose(search_handle)) {
						displayErrorInfo();
					}

					StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%02ld:%03ld ms", dh, dm, ds, dms);
					csay("gi", L"\n> Download of %s (%.2f %s) has successfully completed in %s\n", file_data.first.c_str(), file_size, correct_size_word, correct_time_word);
					download_success = true;
				}
				else
				{
					csay("ri", L"\n> Failed to download %s (%.2f %s)!\n", file_data.first.c_str(), file_size, correct_size_word);
					download_success = false; // LONG_PTR <=> long

				}

				ShowCursor(console, true);

			}

			if (!download_success) {

				csay("ci", L"\n> Downloading missing %s %s (%.2f %s) ...", correct_word, file_data.first.c_str(), file_size, correct_size_word);

				const ::HINTERNET session = ::netstart();
				if (session != 0)
				{
					StringCchPrintfW(download_link, _countof(download_link), L"%s/%s/%s", gHttpUrlToGameFiles, cod4_url_folder_path.c_str(), file_data.first.c_str());
					start = time_now();
					const ::HINTERNET istream = ::netopen(session, download_link);
					if (istream != 0)
					{
						std::ofstream ostream(new_file_path, std::ios::binary);
						if (ostream.is_open()) {
							::netfetch(istream, ostream);
							dt = time_elapsed(start);
							this_thread::sleep_for(milliseconds(250));
							ZeroMemory(&find_data, sizeof(find_data));
							search_handle = FindFirstFileW(new_file_path, &find_data);
							if ((status == S_OK) && (search_handle != INVALID_HANDLE_VALUE))
							{
								ds = static_cast<long>(floor(dt));
								dms = static_cast<long>((dt - ds) * 1000); // get the milliseconds part [0.0,0.99] * 1000 = [000,999]
								dm = dh = 0;
								if (ds > 59)
								{
									dm = ds / 60;
									ds -= (dm * 60);
									if (dm > 59) {
										dh = dm / 60;
										dm -= (dh * 60);
									}
								}

								if (!FindClose(search_handle)) {
									displayErrorInfo();
								}

								StringCchPrintfW(correct_time_word, _countof(correct_time_word), L"%02ld:%02ld:%02ld:%03ld ms", dh, dm, ds, dms);
								csay("gi", L"\n> Download of %s (%.2f %s) has successfully completed in %s\n", file_data.first.c_str(), file_size, correct_size_word, correct_time_word);
								// download_success = true;
							}
							else
							{
								csay("ri", L"\n> Failed to download %s (%.2f %s)!\n", file_data.first.c_str(), file_size, correct_size_word);
								// download_success = false; // LONG_PTR <=> long

							}
						}
						else {
							csay("ri", L"\n> Failed to download %s (%.2f %s)!\n", file_data.first.c_str(), file_size, correct_size_word);
							// download_success = false;

						}

						::netclose(istream);
					}
					::netclose(session);
				}
			}

			if (_kbhit()) {
				key = _getch();
				if (key == VK_ESCAPE) {
					// tc(console, "yi");
					ShowCursor(console, true);
					stop_downloading = true;
					csay("yi", L"\n\n> You have pressed the Esc key! Terminating task of downloading missing Verindra custom mod and map files...\n");
					this_thread::sleep_for(milliseconds(1000));
					break;
				}
				else if (key == VK_SPACE) {

					csay("yi", L"\n\n> You have pressed the Space key!\n> The current download task has temporarily been suspended.\n");
					csay("gi", L"\n> You can press any key to continue the download process...");

				}
			}
		}
	}

	ShowCursor(console, true);

	gDSI.da_time = time_elapsed(da_start_time);

	return true;
}

const wchar_t* checkIfGameInstalled() {
	
	static wchar_t install_path[MAX_PATH_LEN];
	const wchar_t* game_install_path_key = L"InstallPath";
	auto cch = DWORD{};
	HKEY game_installation_reg_key{};
	auto found = bool{};

	const wchar_t** def_game_reg_key;

	if (gShortGameName == L"cod2") def_game_reg_key = def_cod2_registry_location_subkeys;
	else if (gShortGameName == L"cod4") def_game_reg_key = def_cod4_registry_location_subkeys;
	else
	{
		StringCchCopyW(install_path, ARRAYSIZE(install_path), L"C:\\");
		return install_path;
	}

	LRESULT status;

	while (!found && *def_game_reg_key) {

		ZeroMemory(&game_installation_reg_key, sizeof(HKEY));

		cch = sizeof(install_path);

		status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, *def_game_reg_key, 0, KEY_QUERY_VALUE, &game_installation_reg_key);

		if (status == ERROR_SUCCESS) {

			status = RegQueryValueExW(game_installation_reg_key, game_install_path_key, nullptr, nullptr, reinterpret_cast<LPBYTE>(install_path), &cch);
			// status = RegGetValueA(HKEY_LOCAL_MACHINE, *def_game_reg_key, game_install_path_key, RRF_RT_ANY, nullptr, reinterpret_cast<LPBYTE>(install_path), &cch);

			if (status == ERROR_SUCCESS) {

				removeDirPathSepChar(install_path);
				found = true;
				csay("gi", L"\n> Successfully located your %S game's installation path: %S\n", gLongGameName.c_str(), install_path);
				*def_game_reg_key = nullptr;
				break;
				
			}

			RegCloseKey(game_installation_reg_key);
		}

		def_game_reg_key++;
	}

	if (!found) {

		if (gShortGameName == L"cod2") def_game_reg_key = def_cod2_registry_location_subkeys;
		else if (gShortGameName == L"cod4") def_game_reg_key = def_cod4_registry_location_subkeys;

		while (!found && *def_game_reg_key) {

			ZeroMemory(&game_installation_reg_key, sizeof(HKEY));
			cch = sizeof(install_path);

			status = RegOpenKeyExW(HKEY_CURRENT_USER, *def_game_reg_key, 0, KEY_QUERY_VALUE, &game_installation_reg_key);

			if (status == ERROR_SUCCESS) {

				status = RegQueryValueExW(game_installation_reg_key, game_install_path_key, nullptr, nullptr, reinterpret_cast<LPBYTE>(install_path), &cch);
				// status = RegGetValueA(HKEY_CURRENT_USER, *def_game_reg_key, game_install_path_key, REG_SZ, NULL, reinterpret_cast<LPBYTE>(install_path), &cch);

				if (status == ERROR_SUCCESS) {

					removeDirPathSepChar(install_path);
					found = true;
					csay("gi", L"\n> Successfully located your %S game's installation path: %S\n", gLongGameName.c_str(), install_path);
					*def_game_reg_key = nullptr;
					break;
					
				}

				RegCloseKey(game_installation_reg_key);
			}

			def_game_reg_key++;

		}

	}

	if (!found) {
		StringCchCopyW(install_path, ARRAYSIZE(install_path), L"C:\\");
	}

	return install_path;
}

const FileInfo& processDataLine(wstring& line) {
	static FileInfo rv{};
	static FileInfo ev{}; // empty FileInfo structure
	wchar_t* endPtr{};

	if (empty(line)) return ev;

	const wchar_t* pstr = line.c_str();

	if (!pstr || !(*pstr)) return ev;

	wchar_t* mi = const_cast<wchar_t*>(pstr);

	while (*mi != L',') mi++;

	rv.file_name_.assign(pstr, mi);

	mi++;

	pstr = mi;

	while (*mi != L',') mi++;

	*mi = L'\0';

	mi++;

	rv.file_size_ = wcstol(pstr, &endPtr, 10);

	if (*mi) {
		rv.md5_hash_value_ = mi;
	}
	else {
		return ev;
	}

	return rv;
}

bool readProgramInformationAndSettings() {
	auto file_data = WIN32_FIND_DATA{};
	// auto status = HRESULT{};
	HANDLE handle;
	auto file_output = wofstream{};

	gProgramSettings[L"program_name"] = L"Verindra auto-updater/downloader tool";
	gProgramSettings[L"exe_name"] = L"VerindraAutoUpdaterDownloader.exe";
	gProgramSettings[L"version"] = wstring(PROGRAM_VERSION).substr(1);
	gProgramSettings[L"author"] = L"Freedom_figher (Atila Budai)";
	gProgramSettings[L"last_update"] = LAST_UPDATE_DATE;
	gProgramSettings[L"development_language"] = L"C++";
	gProgramSettings[L"development_environment"] = L"Visual Studio 2015 Professional, Jetbrains CLion";
	gProgramSettings[L"development_libraries"] = L"win32 api, wininet, urlmon, handle.h, debug.h, c++ stl libraries";
	gProgramSettings[L"external_tools"] = L"wget,curl";
	gProgramSettings[L"development_time"] = L"3 weeks";
	gProgramSettings[L"use_ftp"] = L"1";
	gProgramSettings[L"use_http"] = L"1";
	gProgramSettings[L"use_curl"] = L"1";
	gProgramSettings[L"use_urlmon"] = L"1";
	gProgramSettings[L"use_wget"] = L"1";
	gProgramSettings[L"use_wininet"] = L"1";
	gProgramSettings[L"http_download_link_for_curl"] = L"http://ve111.venus.fastwebserver.de:7777/Tools/curl.exe";
	gProgramSettings[L"ftp_download_link_for_curl"] = L"ftp://ve111.venus.fastwebserver.de/Tools/curl.exe";
	gProgramSettings[L"http_download_link_for_libcurldll"] = L"http://ve111.venus.fastwebserver.de:7777/Tools/libcurl.dll";
	gProgramSettings[L"ftp_download_link_for_libcurldll"] = L"ftp://ve111.venus.fastwebserver.de/Tools/libcurl.dll";
	gProgramSettings[L"http_download_link_for_wget"] = L"http://ve111.venus.fastwebserver.de:7777/Tools/wget.exe";
	gProgramSettings[L"ftp_download_link_for_wget"] = L"ftp://ve111.venus.fastwebserver.de/Tools/wget.exe";
	gProgramSettings[L"http_url_to_updated_list_of_cod2_files"] = L"http://ve111.venus.fastwebserver.de:7777/COD2/updated_list_of_verindra_cod2_mod_and_custom_map_files.lst";
	gProgramSettings[L"ftp_url_to_updated_list_of_cod2_files"] = L"ftp://ve111.venus.fastwebserver.de/COD2/updated_list_of_verindra_cod2_mod_and_custom_map_files.lst";
	gProgramSettings[L"http_url_to_updated_list_of_cod4_files"] = L"http://ve111.venus.fastwebserver.de:7777/COD4/updated_list_of_verindra_cod4_mod_and_custom_map_files.lst";
	gProgramSettings[L"ftp_url_to_updated_list_of_cod4_files"] = L"ftp://ve111.venus.fastwebserver.de/COD4/updated_list_of_verindra_cod4_mod_and_custom_map_files.lst";
	gProgramSettings[L"http_base_url_to_cod2_files"] = L"http://ve111.venus.fastwebserver.de:7777/COD2";
	gProgramSettings[L"ftp_base_url_to_cod2_files"] = L"ftp://ve111.venus.fastwebserver.de/COD2";
	gProgramSettings[L"http_base_url_to_cod4_files"] = L"http://ve111.venus.fastwebserver.de:7777/COD4";
	gProgramSettings[L"ftp_base_url_to_cod4_files"] = L"ftp://ve111.venus.fastwebserver.de/COD4";

	auto count = GetCurrentDirectory(_countof(current_dir_path), current_dir_path);

	if (!count) {
		displayErrorInfo();
	}
	else {
		// say(L"\nA. Current working directory: %s\n", current_dir_path);
		addDirPathSepChar(current_dir_path);
		// say(L"\nB. Current working directory: %s\n", current_dir_path);
	}

	swprintf_s(current_file_path, L"\\\\?\\%ssettings.ini", current_dir_path);

	handle = FindFirstFile(current_file_path, &file_data);

	if (handle == INVALID_HANDLE_VALUE) {
		file_output.open(L"settings.ini", ios::out);

		if (!file_output) {
			say(L"\n> Could not create default settings.ini file in current directory!\nMake sure that you have sufficient write access to the current working directory.\n");
			displayErrorInfo();
			return false;
		}
		else {
			file_output << L"[Information]" << endl;
			file_output << L"program_name = \"" << gProgramSettings[L"program_name"] << L'"' << endl;
			file_output << L"exe_name = \"" << gProgramSettings[L"exe_name"] << L'"' << endl;
			file_output << L"version = \"" << gProgramSettings[L"version"] << L'"' << endl;
			file_output << L"author = \"" << gProgramSettings[L"author"] << L'"' << endl;
			file_output << L"last_update = \"" << gProgramSettings[L"last_update"] << L'"' << endl;
			file_output << L"development_language = \"" << gProgramSettings[L"development_language"] << L'"' << endl;
			file_output << L"development_environment = \"" << gProgramSettings[L"development_environment"] << L'"' << endl;
			file_output << L"development_libraries = \"" << gProgramSettings[L"development_libraries"] << L'"' << endl;
			file_output << L"external_tools = \"" << gProgramSettings[L"external_tools"] << L'"' << endl;
			file_output << L"development_time = \"" << gProgramSettings[L"development_time"] << L'"' << endl << endl;
			file_output << L"[Settings]" << endl;
			file_output << L"use_ftp = " << gProgramSettings[L"use_ftp"] << endl;
			file_output << L"use_http = " << gProgramSettings[L"use_http"] << endl;
			file_output << L"use_curl = " << gProgramSettings[L"use_curl"] << endl;
			file_output << L"use_urlmon = " << gProgramSettings[L"use_urlmon"] << endl;
			file_output << L"use_wget = " << gProgramSettings[L"use_wget"] << endl;
			file_output << L"use_wininet = " << gProgramSettings[L"use_wininet"] << endl;
			file_output << L"http_download_link_for_curl = \"" << gProgramSettings[L"http_download_link_for_curl"] << L'"' << endl;
			file_output << L"ftp_download_link_for_curl = \"" << gProgramSettings[L"ftp_download_link_for_curl"] << L'"' << endl;
			file_output << L"http_download_link_for_libcurldll = \"" << gProgramSettings[L"http_download_link_for_libcurldll"] << L'"' << endl;
			file_output << L"ftp_download_link_for_libcurldll = \"" << gProgramSettings[L"ftp_download_link_for_libcurldll"] << L'"' << endl;
			file_output << L"http_download_link_for_wget = \"" << gProgramSettings[L"http_download_link_for_wget"] << L'"' << endl;
			file_output << L"ftp_download_link_for_wget = \"" << gProgramSettings[L"ftp_download_link_for_wget"] << L'"' << endl;
			file_output << L"http_url_to_updated_list_of_cod2_files = \"" << gProgramSettings[L"http_url_to_updated_list_of_cod2_files"] << L'"' << endl;
			file_output << L"ftp_url_to_updated_list_of_cod2_files = \"" << gProgramSettings[L"ftp_url_to_updated_list_of_cod2_files"] << L'"' << endl;
			file_output << L"http_url_to_updated_list_of_cod4_files = \"" << gProgramSettings[L"http_url_to_updated_list_of_cod4_files"] << L'"' << endl;
			file_output << L"ftp_url_to_updated_list_of_cod4_files = \"" << gProgramSettings[L"ftp_url_to_updated_list_of_cod4_files"] << L'"' << endl;
			file_output << L"http_base_url_to_cod2_files = \"" << gProgramSettings[L"http_base_url_to_cod2_files"] << L'"' << endl;
			file_output << L"ftp_base_url_to_cod2_files = \"" << gProgramSettings[L"ftp_base_url_to_cod2_files"] << L'"' << endl;
			file_output << L"http_base_url_to_cod4_files = \"" << gProgramSettings[L"http_base_url_to_cod4_files"] << L'"' << endl;
			file_output << L"ftp_base_url_to_cod4_files = \"" << gProgramSettings[L"ftp_base_url_to_cod4_files"] << L'"' << endl;

			file_output.flush();
			file_output.close();
			return true;
		}
	}
	else {
		auto file_input = wifstream{};
		auto text_line = wstring{};
		auto trimmed_line = wstring{};

		file_input.open(current_file_path, ios::in);

		if (!file_input) return false;

		while (getline(file_input, text_line)) {
			trimmed_line = trim(text_line);

			if ((trimmed_line[0] == L'#') || (trimmed_line[0] == L';')) continue;

			if (empty(trimmed_line) || ((trimmed_line[0] == L'[') && (trimmed_line.find(L']', 1) != wstring::npos))) continue;

			auto const& key_value_pair = processConfigurationLine(trimmed_line);

			gProgramSettings[key_value_pair.first] = key_value_pair.second;
		}

		file_input.close();
	}

	return true;
}

const KeyValuePair& processConfigurationLine(const wstring& line) {
	static auto key_value_pair = KeyValuePair{};

	// auto quot_marks_used = bool{};

	const wchar_t* start;
	const wchar_t* stop;

	start = stop = line.c_str();

	while (*stop != L'=') stop++;

	key_value_pair.first = trim(wstring{ start, stop });

	start = stop;

	while (iswspace(*start) || (*start == L'=') || (*start == L'"')) {
		// if (*start == L'"') quot_marks_used = true;
		start++;
	}

	stop = start;

	while ((*stop != L'"') && (*stop != L'\0')) stop++;

	/*
	if (quot_marks_used) {
		while (*stop != L'"') stop++;
	} else {
		while (!iswspace(*stop)) stop++;
	}
	*/

	key_value_pair.second = trim(wstring{ start, stop });

	return key_value_pair;
}

BOOL SetPrivilege(
	HANDLE hToken,          // token handle
	LPCTSTR Privilege,      // Privilege to enable/disable
	BOOL bEnablePrivilege   // TRUE to enable.  FALSE to disable
)
{
	TOKEN_PRIVILEGES tp;
	LUID luid;
	TOKEN_PRIVILEGES tpPrevious;
	DWORD cbPrevious = sizeof(TOKEN_PRIVILEGES);

	if (!LookupPrivilegeValue(NULL, Privilege, &luid)) return FALSE;

	//
	// first pass.  get current privilege setting
	//
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = 0;

	AdjustTokenPrivileges(
		hToken,
		FALSE,
		&tp,
		sizeof(TOKEN_PRIVILEGES),
		&tpPrevious,
		&cbPrevious
	);

	if (GetLastError() != ERROR_SUCCESS) return FALSE;

	//
	// second pass.  set privilege based on previous setting
	//
	tpPrevious.PrivilegeCount = 1;
	tpPrevious.Privileges[0].Luid = luid;

	if (bEnablePrivilege) {
		tpPrevious.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
	}
	else {
		tpPrevious.Privileges[0].Attributes ^= (SE_PRIVILEGE_ENABLED &
			tpPrevious.Privileges[0].Attributes);
	}

	AdjustTokenPrivileges(
		hToken,
		FALSE,
		&tpPrevious,
		cbPrevious,
		NULL,
		NULL
	);

	if (GetLastError() != ERROR_SUCCESS) return FALSE;

	return TRUE;
}

/************************************************************************/
/* fn param szApi : pointer to failed API name                          */
/* fn return type : void                                                */
/************************************************************************/
void DisplayError(LPTSTR szAPI)  // 
{
	LPTSTR MessageBuffer;
	DWORD dwBufferLength;

	fwprintf(stderr, L"%s() error!\n", szAPI);

	if ((dwBufferLength = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		GetLastError(),
		GetSystemDefaultLangID(),
		(LPTSTR)&MessageBuffer,
		0,
		NULL
	)) > 0)
	{
		DWORD dwBytesWritten;

		//
		// Output message string on stderr
		//
		WriteFile(
			GetStdHandle(STD_ERROR_HANDLE),
			MessageBuffer,
			dwBufferLength,
			&dwBytesWritten,
			NULL
		);

		//
		// free the buffer allocated by the system
		//
		LocalFree(MessageBuffer);
	}
}

void displayImportantInformationToUser(const char* color_codes, const wchar_t* msg) {	
	csay(color_codes, L"\n%s\n", msg);	
}

void displayErrorMessage(LPTSTR lpszFunction, DWORD const custom_error_code, bool exitProcess)
{
	// Retrieve the system error message for the last-error code

	LPVOID lpMsgBuf{};
	LPVOID lpDisplayBuf;
	// DWORD const dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		custom_error_code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	if (!lpMsgBuf) {
		if (exitProcess) ExitProcess(custom_error_code);		
	}

	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));

	if (!lpDisplayBuf)
	{
		if (lpMsgBuf) LocalFree(lpMsgBuf);
		if (exitProcess) ExitProcess(custom_error_code);
	}

	StringCchPrintf((LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR), TEXT("%s failed with error %d: %s"), lpszFunction, custom_error_code, (LPCTSTR)lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error information"), MB_OK | MB_ICONEXCLAMATION);

	if (lpMsgBuf) LocalFree(lpMsgBuf);
	if (lpDisplayBuf) LocalFree(lpDisplayBuf);

	if (exitProcess) ExitProcess(custom_error_code);
}

void displayErrorInfo(wchar_t const* custom_error_msg, DWORD custom_error_code, bool terminate_program_execution, customErrorFnc fn) {
	static wchar_t error_msg[1024];
	DWORD error_code;

#if defined(_DEBUG) || defined(DEBUG)

	if (!custom_error_msg || !(*custom_error_msg) || (custom_error_code == -1)) {
		error_code = GetLastError();
		_wcserror_s(error_msg, error_code);

		if (fn) {
			fn(error_msg, error_code);
		}
		else {
			say(L"\nError: %s [Error code: %d]\n", error_msg, error_code);
		}

		custom_error_code = error_code;
	}
	else {
		if (fn) {
			fn(custom_error_msg, custom_error_code);
		}
		else {
			say(L"\nError: %s [Error code: %d]\n", custom_error_msg, custom_error_code);
		}
	}

	if (terminate_program_execution) exit(custom_error_code);

#else

	unused(error_msg, error_code, custom_error_msg, custom_error_code, terminate_program_execution, fn);

#endif
}

void reportCorrectErrorInformationForCreateDirectory(DWORD error_code, wchar_t const* dir_name) {

	csay("yi", L"\n> Failed to create missing directory: %s", dir_name);
	switch (error_code) {

	case ERROR_ALREADY_EXISTS:
		csay("gi", L"\n> Directory: %s already exists.", dir_name);
		break;

	case ERROR_PATH_NOT_FOUND:
		csay("ri", L"\n> Specified folder path %s contains non-existing directory entries in its path!\n\n", game_folder_path);
		csay("yi", L">Please, make sure that the fully specified path exists.\n");
		break;

	default:
		csay("ri", L"\n> Unknown error!");
		break;
	}
}