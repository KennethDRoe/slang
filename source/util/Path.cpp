//------------------------------------------------------------------------------
// Path.cpp
// Cross platform file path handling and directory iteration.
//
// File is under the MIT license; see LICENSE for details.
//------------------------------------------------------------------------------
#include "Path.h"

#if defined(_WIN32)

typedef char CHAR;
typedef wchar_t WCHAR;
typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef __int64 LONG_PTR;
typedef void *HANDLE;
typedef WCHAR *NWPSTR, *LPWSTR, *PWSTR;
typedef const WCHAR *LPCWSTR, *PCWSTR;
typedef const WCHAR *LPCWCH, *PCWCH;
typedef const CHAR *LPCCH, *PCCH;
typedef CHAR *NPSTR, *LPSTR, *PSTR;
typedef BOOL *LPBOOL;

#define MAX_PATH 260
#define CP_UTF8 65001
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010  
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define ERROR_NO_MORE_FILES 18L
#define WINAPI __stdcall

extern "C" {

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    WCHAR  cFileName[MAX_PATH];
    WCHAR  cAlternateFileName[14];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;

DWORD WINAPI GetFileAttributesW(LPCWSTR lpFileName);
DWORD WINAPI GetFullPathNameW(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR * lpFilePart);
int WINAPI MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar);
int WINAPI WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWCH lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte, LPCCH lpDefaultChar, LPBOOL lpUsedDefaultChar);
HANDLE WINAPI FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData);
BOOL WINAPI FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData);
BOOL WINAPI FindClose(HANDLE hFindFile);
DWORD WINAPI GetLastError(void);

}

#endif

namespace slang {

bool Path::exists() const {
#if defined(_WIN32)
    return GetFileAttributesW(wstr().c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat sb;
    return stat(str().c_str(), &sb) == 0;
#endif
}

size_t Path::fileSize() const {
#if defined(_WIN32)
    struct _stati64 sb;
    if (_wstati64(wstr().c_str(), &sb) != 0)
        throw std::runtime_error("path::file_size(): cannot stat file \"" + str() + "\"!");
#else
    struct stat sb;
    if (stat(str().c_str(), &sb) != 0)
        throw std::runtime_error("path::file_size(): cannot stat file \"" + str() + "\"!");
#endif
    return (size_t)sb.st_size;
}

bool Path::isDirectory() const {
#if defined(_WIN32)
    DWORD result = GetFileAttributesW(wstr().c_str());
    if (result == INVALID_FILE_ATTRIBUTES)
        return false;
    return (result & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat sb;
    if (stat(str().c_str(), &sb))
        return false;
    return S_ISDIR(sb.st_mode);
#endif
}

bool Path::isFile() const {
#if defined(_WIN32)
    DWORD attr = GetFileAttributesW(wstr().c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
#else
    struct stat sb;
    if (stat(str().c_str(), &sb))
        return false;
    return S_ISREG(sb.st_mode);
#endif
}

Path Path::makeAbsolute(const Path& path) {
#if !defined(_WIN32)
    char temp[PATH_MAX];
    if (realpath(path.str().c_str(), temp) == nullptr)
        throw std::runtime_error("Internal error in realpath(): " + std::string(strerror(errno)));
    return Path(temp);
#else
    std::wstring value = path.wstr();
    std::wstring out(MAX_PATH, '\0');
    DWORD length = GetFullPathNameW(value.c_str(), MAX_PATH, &out[0], nullptr);
    if (length == 0)
        throw std::runtime_error("Internal error in GetFullPathNameW(): " + std::to_string(GetLastError()));
    return Path(out.substr(0, length));
#endif
}

Path Path::getCurrentDirectory() {
#if !defined(_WIN32)
    char temp[PATH_MAX];
    if (::getcwd(temp, PATH_MAX) == NULL)
        throw std::runtime_error("Internal error in getcwd(): " + std::string(strerror(errno)));
    return Path(temp);
#else
    std::wstring temp(MAX_PATH, '\0');
    if (!_wgetcwd(&temp[0], MAX_PATH))
        throw std::runtime_error("Internal error in _wgetcwd(): " + std::to_string(GetLastError()));
    return Path(temp.c_str());
#endif
}

#if defined(_WIN32)
std::wstring Path::wstr(PathType type) const {
    std::string temp = str(type);
    int size = MultiByteToWideChar(CP_UTF8, 0, &temp[0], (int)temp.size(), NULL, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, &temp[0], (int)temp.size(), &result[0], size);
    return result;
}

void Path::set(const std::wstring& wstring, PathType type) {
    std::string string;
    if (!wstring.empty()) {
        int size = WideCharToMultiByte(CP_UTF8, 0, &wstring[0], (int)wstring.size(),
                                       NULL, 0, NULL, NULL);
        string.resize(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstring[0], (int)wstring.size(),
                            &string[0], size, NULL, NULL);
    }
    set(string, type);
}
#endif

std::vector<Path> getFilesInDirectory(const Path& path) {
    std::vector<Path> result;

#if defined(_WIN32)
    WIN32_FIND_DATAW ffd;
    std::wstring base = path.wstr() + L"\\";
    HANDLE hFind = FindFirstFileW((base + +L"*").c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Internal error in FindFirstFile(): " + std::to_string(GetLastError()));

    do {
        if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            result.push_back(base + ffd.cFileName);
    } while (FindNextFileW(hFind, &ffd) != 0);

    DWORD dwError = GetLastError();
    if (dwError != ERROR_NO_MORE_FILES)
        throw std::runtime_error("Internal error in FindNextFile(): " + std::to_string(dwError));

    FindClose(hFind);
#else
    DIR* d;
    struct dirent* dir;
    std::string base = path.str() + "/";
    d = opendir(base.c_str());
    if (d) {
        while ((dir = readdir(d))) {
            if (dir->d_type == DT_REG)
                result.push_back(base + dir->d_name);
        }
        closedir(d);
    }
#endif

    return result;
}

}