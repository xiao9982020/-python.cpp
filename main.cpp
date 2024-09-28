#include <filesystem>
#include <string>
#include <fstream>
#include <set>
#include <json/json.h>
#include <windows.h>
#include <tchar.h>
#include <dbt.h>

#define WM_TRAYICON (WM_USER + 1)
#define IDI_TRAYICON 1
#define ID_TRAY_EXIT 1001

namespace fs = std::filesystem;

NOTIFYICONDATA nid = { 0 };
HMENU hPopMenu;
HDEVNOTIFY hDevNotify;
TCHAR szBackupName[MAX_PATH + 1];

fs::path destination;

void HandleNewVolume(DWORD dwDrives, DWORD dbcv_flags);
BOOL GetDriveInfo(TCHAR driveLetter, TCHAR* volumeName, DWORD bufferSize);
void DoBackUP(std::wstring backDrive);
void GetFilesInDirectory(const fs::path& directory, std::set<fs::path>& files);
void DeleteFileInDirectory(const fs::path& directory, const fs::path& file);
void SyncDirectories(const fs::path& sourceDirectory, const fs::path& destDirectory);
int GetConfig();
std::wstring ConvertStringToWstring(const std::string& str);

std::wstring ConvertStringToWstring(const std::string& str) {
    std::mbstate_t state = std::mbstate_t();
    const char* src = str.c_str();
    size_t len = str.size() + 1; // +1 for null terminator

    std::vector<wchar_t> buffer(len);
    size_t convertedChars = 0;

    errno_t err = mbsrtowcs_s(&convertedChars, buffer.data(), buffer.size(), &src, len - 1, &state);
    if (err != 0) {
        throw std::runtime_error("Conversion from multibyte to wide character failed.");
    }

    return std::wstring(buffer.data());
}

int GetConfig() {
    std::ifstream inputFile("config.json", std::ifstream::binary);
    if (!inputFile.is_open()) {
        MessageBoxW(NULL, L"坏，没配置文件(config.json)咋运行`(*>﹏<*)′", L"失败", MB_ICONERROR | MB_OK);
        return 1;
    }

    // 读取文件内容到 Json::Value 对象
    Json::Value config;
    Json::CharReaderBuilder reader;
    std::string errs;
    if (!Json::parseFromStream(reader, inputFile, &config, &errs)) {
        MessageBoxW(NULL, L"Json写错了啊喂(～￣▽￣)～", L"失败", MB_ICONERROR | MB_OK);
        return 1;
    }
    inputFile.close();

    // 读取 JSON 数据
    try {
        std::string volume_name = static_cast<std::string>(config["volume_name"].asString());
        #ifdef _UNICODE
                if (volume_name.size() > MAX_PATH) {
                    MessageBoxW(NULL, L"谁让你超过旧WindowsAPI的长度上限了？", L"失败", MB_ICONERROR | MB_OK);
                }

                int numChars = MultiByteToWideChar(CP_UTF8, 0, volume_name.c_str(), -1, szBackupName, MAX_PATH + 1);
                if (numChars == 0) {
                    MessageBoxW(NULL, L"一种一般性的类型转换失败", L"失败", MB_ICONERROR | MB_OK);
                }
                szBackupName[MAX_PATH] = '\0';
        #else
                if (str.size() > MAX_PATH) {
                    MessageBoxW(NULL, L"谁让你超过旧WindowsAPI的长度上限了？", L"失败", MB_ICONERROR | MB_OK);
                }

                std::strncpy(szBackupName, str.c_str(), MAX_PATH);
                szBackupName[MAX_PATH] = '\0';
        #endif
        std::string backup_folder = static_cast<std::string>(config["backup_folder"].asString());
        destination = fs::path(backup_folder);
    }
    catch (const std::exception& e) {
        MessageBoxW(NULL, L"Json少写字段了！错误的！", L"失败", MB_ICONERROR | MB_OK);
        return 1;
    }

    return 0;
}

void GetFilesInDirectory(const fs::path& directory, std::set<fs::path>& files) {
    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            files.insert(entry.path());
        }
    }
}

void DeleteFileInDirectory(const fs::path& file) {
    fs::remove(file);
}

void SyncDirectories(const fs::path& sourceDirectory, const fs::path& destDirectory) {
    std::set<fs::path> sourceFiles;
    std::set<fs::path> destFiles;

    GetFilesInDirectory(sourceDirectory, sourceFiles);
    GetFilesInDirectory(destDirectory, destFiles);

    for (const auto& destFile : destFiles) {
        fs::path relativePath = fs::relative(destFile, destDirectory);
        fs::path correspondingSourceFile = sourceDirectory / relativePath;

        if (sourceFiles.find(correspondingSourceFile) == sourceFiles.end()) {
            DeleteFileInDirectory(destFile);
        }
    }

    for (const auto& sourceFile : sourceFiles) {
        fs::path relativePath = fs::relative(sourceFile, sourceDirectory);
        fs::path destFilePath = destDirectory / relativePath;

        if (!fs::exists(destFilePath) || fs::last_write_time(sourceFile) > fs::last_write_time(destFilePath)) {
            fs::create_directories(destFilePath.parent_path());
            fs::copy_file(sourceFile, destFilePath, fs::copy_options::overwrite_existing);
        }
    }
}

void DoBackUP(std::wstring backDrive) {
    fs::path sourcePath = backDrive;
    try {
        SyncDirectories(sourcePath, destination);
        MessageBoxW(NULL, L"备份完成", L"成功", MB_OK);
    }
    catch (...) {
        MessageBoxW(NULL, L"备份失败，我也不知道为什么哦`(*>﹏<*)′", L"失败", MB_ICONERROR | MB_OK);
    }
}

void HandleNewVolume(DWORD dwDrives, DWORD dbcv_flags) {
    TCHAR szDrive[] = _T("A:\\");
    TCHAR szVolumeName[MAX_PATH + 1];
    TCHAR szMsg[512];

    for (int i = 0; i < 26; i++) {
        if (dwDrives & (1 << i)) {
            szDrive[0] = _T('A') + i;
            if (GetDriveInfo(szDrive[0], szVolumeName, MAX_PATH + 1)) {
                MessageBox(NULL, szVolumeName, _T("通知"), MB_OK | MB_ICONINFORMATION);

                int response = MessageBox(NULL, _T("是否开始备份？"), _T("通知"), MB_OKCANCEL | MB_ICONQUESTION);
                if (response == IDCANCEL) {
                    MessageBox(NULL, _T("备份已取消"), _T("通知"), MB_OK | MB_ICONINFORMATION);
                    return;
                }

                if (static_cast<std::wstring>(szVolumeName) == static_cast<std::wstring>(szBackupName)) {
                    MessageBox(NULL, _T("开始备份"), _T("通知"), MB_OK | MB_ICONINFORMATION);
                    DoBackUP(szDrive);
                }
            }
        }
    }
}

BOOL GetDriveInfo(TCHAR driveLetter, TCHAR* volumeName, DWORD bufferSize) {
    TCHAR rootPath[] = _T("A:\\");
    rootPath[0] = driveLetter;

    return GetVolumeInformation(
        rootPath,
        volumeName,
        bufferSize,
        NULL,
        NULL,
        NULL,
        NULL,
        0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        // 创建系统托盘图标
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hWnd;
        nid.uID = IDI_TRAYICON;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APPLICATION));
        _tcscpy_s(nid.szTip, _T("BackUpYourUSBStorage"));
        Shell_NotifyIcon(NIM_ADD, &nid);

        // 创建右键菜单
        hPopMenu = CreatePopupMenu();
        InsertMenu(hPopMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, _T("exit"));

        // 注册设备通知
        DEV_BROADCAST_DEVICEINTERFACE notificationFilter;
        ZeroMemory(&notificationFilter, sizeof(notificationFilter));
        notificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
        notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

        hDevNotify = RegisterDeviceNotification(
            hWnd,
            &notificationFilter,
            DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES
        );
        break;

    case WM_DEVICECHANGE:
    if (wParam == DBT_DEVICEARRIVAL) {
        DEV_BROADCAST_HDR* pHdr = (DEV_BROADCAST_HDR*)lParam;
        if (pHdr->dbch_devicetype == DBT_DEVTYP_VOLUME) {
            DEV_BROADCAST_VOLUME* pVol = (DEV_BROADCAST_VOLUME*)pHdr;
            HandleNewVolume(pVol->dbcv_unitmask, pVol->dbcv_flags);
        }
    }
    break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hPopMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON,
                pt.x, pt.y, 0, hWnd, NULL);
            PostMessage(hWnd, WM_NULL, 0, 0);
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_EXIT) {
            DestroyWindow(hWnd);
        }
        break;

    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow) {
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = _T("TrayIconClass");
    RegisterClassEx(&wcex);

    if (GetConfig() != 0) {
        return -1;
    }

    // 创建一个隐藏的窗口
    HWND hWnd = CreateWindow(_T("TrayIconClass"), _T("WBackUpYourUSBStorage"), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        return FALSE;
    }

    // 不显示窗口
    ShowWindow(hWnd, SW_HIDE);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
