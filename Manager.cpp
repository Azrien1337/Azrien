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
#include <commctrl.h>
#include <cmath>
#include <shlobj.h>
#include <wincrypt.h>
#include <sstream>
#include <iomanip>
#include "resource.h"

#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "crypt32.lib")

using namespace Microsoft::WRL;

const std::string GITEE_OWNER = "xxx";
const std::string GITEE_REPO = "xxx";
const std::string GITEE_TOKEN = "xxx";
const std::string HWID_SEED = "xxx";

#define WINDOW_WIDTH 920
#define WINDOW_HEIGHT 700
#define LOGIN_WIDTH 520
#define LOGIN_HEIGHT 460

static const BYTE AES_KEY_RAW[] = {0x6D, 0x71, 0x74, 0x60, 0x68, 0x75, 0x09, 0x49, 0x70, 0x6A, 0x60, 0x68, 0x74, 0x53, 0x4A, 0x46, 0x4A, 0x49, 0x24, 0x0B, 0x36, 0x3A, 0x35, 0x23, 0x30, 0x21, 0x22, 0x3C};
static const BYTE AES_IV_RAW[] = {0x01, 0x71, 0x74, 0x60, 0x68, 0x75, 0x09, 0x49, 0x70, 0x6A, 0x60, 0x68, 0x74};
const std::string AES_KEY(reinterpret_cast<const char *>(AES_KEY_RAW), 28);
const std::string AES_IV(reinterpret_cast<const char *>(AES_IV_RAW), 13);
const DWORD REM_DUR = 604800000;

ID2D1Factory *g_pF = nullptr;
ID2D1HwndRenderTarget *g_pRT = nullptr;
ID2D1SolidColorBrush *g_pWB = nullptr;
IDWriteFactory *g_pDW = nullptr;
IDWriteTextFormat *g_pSF = nullptr;
IDWriteTextFormat *g_pBF = nullptr;

HWND g_hWnd;
std::vector<std::wstring> g_allData;
std::vector<int> g_fileNums;
std::vector<int> g_selectedIndices;
std::wstring g_searchText;
int g_vScrollPos = 0, g_hScrollPos = 0, g_hoverItem = -1;
bool g_searchFocused = false;
int g_cursorBlink = 0;
bool g_vScrollbarDragging = false, g_hScrollbarDragging = false;
bool g_bFilterTodesk = false, g_bFilterBaidu = false, g_bFilterBilibili = false;
bool g_bTodeskHover = false, g_bBaiduHover = false, g_bBilibiliHover = false;
float g_fTodeskAlpha = 0.3f, g_fTodeskTarget = 0.3f, g_fBaiduAlpha = 0.3f, g_fBaiduTarget = 0.3f, g_fBilibiliAlpha = 0.3f, g_fBilibiliTarget = 0.3f;
bool g_bFilterAnimating = false;
DWORD g_dwFilterAnimStart = 0;
float g_fFilterAnimProgress = 0.0f;
bool g_bLoading = false;
float g_fProgress = 0.0f;
int g_nTotalFiles = 0, g_nLoadedFiles = 0, g_nDuplicates = 0;
std::wstring g_wStatusText = L"";
RECT g_rcSearchBox, g_rcSearchBtn, g_rcFilterTodesk, g_rcFilterBaidu, g_rcFilterBilibili, g_rcList, g_rcExportBtn, g_rcRefreshBtn, g_rcExportAllBtn;
bool g_bSearchHover = false, g_bExportHover = false, g_bRefreshHover = false, g_bExportAllHover = false;
bool g_bSearchPress = false, g_bExportPress = false, g_bRefreshPress = false, g_bExportAllPress = false;
float g_fSearchBtnHover = 1.0f, g_fSearchBtnTarget = 1.0f, g_fExportHover = 1.0f, g_fExportTarget = 1.0f, g_fRefreshHover = 1.0f, g_fRefreshTarget = 1.0f, g_fExportAllHover = 1.0f, g_fExportAllTarget = 1.0f;
bool g_bLoggedIn = false, g_bLoginUI = true;
std::wstring g_wLoginUser = L"", g_wLoginPass = L"", g_wLoginMsg = L"";
bool g_bUserActive = true, g_bPassActive = false, g_bRememberMe = false;
float g_fUserGlow = 1.0f, g_fUserGlowTarget = 1.0f, g_fPassGlow = 0.0f, g_fPassGlowTarget = 0.0f, g_fMsgAlpha = 0.0f, g_fMsgAlphaTarget = 0.0f;
float g_fLoginBtnScale = 1.0f, g_fLoginBtnScaleTarget = 1.0f, g_fRegBtnScale = 1.0f, g_fRegBtnScaleTarget = 1.0f;
bool g_bLoginBtnHover = false, g_bRegBtnHover = false, g_bLoginBtnPress = false, g_bRegBtnPress = false;
RECT g_rcUserInput, g_rcPassInput, g_rcLoginBtn, g_rcRegBtn, g_rcRememberChk;
float g_fLoginTitleY = -50.0f;
bool g_bLoginTitleAnim = true;
DWORD g_dwLoginTitleStart = 0;
float g_fLoginFadeIn = 0.0f;
bool g_bLoginFadingIn = true;
float g_fChkAlpha = 0.0f;

struct LoadState
{
    std::vector<std::pair<int, std::wstring>> tempData;
    std::vector<std::string> fileNames;
    size_t currentIndex;
    int totalFiles, duplicates;
    bool scanningDir;
    std::string dirContent;
    size_t parsePos;
};
LoadState g_loadState;

std::wstring U2W(const std::string &s)
{
    if (s.empty())
        return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
std::string W2U(const std::wstring &w)
{
    if (w.empty())
        return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}
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
    if (i)
    {
        for (int j = i; j < 4; j++)
            c4[j] = 0;
        c3[0] = (c4[0] << 2) + ((c4[1] & 0x30) >> 4);
        c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
        for (int j = 0; j < i - 1; j++)
            r += c3[j];
    }
    return r;
}
void StartLoading();

std::string AESEnc(const std::string &pt)
{
    if (pt.empty())
        return "";
    HCRYPTPROV hp = 0;
    HCRYPTKEY hk = 0;
    HCRYPTHASH hh = 0;
    if (!CryptAcquireContextA(&hp, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return "";
    if (!CryptCreateHash(hp, CALG_SHA_256, 0, 0, &hh))
    {
        CryptReleaseContext(hp, 0);
        return "";
    }
    if (!CryptHashData(hh, (BYTE *)AES_KEY.c_str(), (DWORD)AES_KEY.length(), 0))
    {
        CryptDestroyHash(hh);
        CryptReleaseContext(hp, 0);
        return "";
    }
    if (!CryptDeriveKey(hp, CALG_AES_256, hh, CRYPT_EXPORTABLE, &hk))
    {
        CryptDestroyHash(hh);
        CryptReleaseContext(hp, 0);
        return "";
    }
    DWORD md = CRYPT_MODE_CBC;
    CryptSetKeyParam(hk, KP_MODE, (BYTE *)&md, 0);
    CryptSetKeyParam(hk, KP_IV, (BYTE *)AES_IV.c_str(), 0);
    std::string d = pt;
    DWORD dl = (DWORD)d.length(), bl = dl + 16;
    std::vector<BYTE> bf(bl);
    memcpy(bf.data(), d.c_str(), dl);
    if (CryptEncrypt(hk, 0, TRUE, 0, bf.data(), &dl, bl))
    {
        std::string r((char *)bf.data(), dl);
        CryptDestroyKey(hk);
        CryptDestroyHash(hh);
        CryptReleaseContext(hp, 0);
        return r;
    }
    CryptDestroyKey(hk);
    CryptDestroyHash(hh);
    CryptReleaseContext(hp, 0);
    return "";
}
std::string AESDec(const std::string &ct)
{
    if (ct.empty())
        return "";
    HCRYPTPROV hp = 0;
    HCRYPTKEY hk = 0;
    HCRYPTHASH hh = 0;
    if (!CryptAcquireContextA(&hp, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return "";
    if (!CryptCreateHash(hp, CALG_SHA_256, 0, 0, &hh))
    {
        CryptReleaseContext(hp, 0);
        return "";
    }
    if (!CryptHashData(hh, (BYTE *)AES_KEY.c_str(), (DWORD)AES_KEY.length(), 0))
    {
        CryptDestroyHash(hh);
        CryptReleaseContext(hp, 0);
        return "";
    }
    if (!CryptDeriveKey(hp, CALG_AES_256, hh, CRYPT_EXPORTABLE, &hk))
    {
        CryptDestroyHash(hh);
        CryptReleaseContext(hp, 0);
        return "";
    }
    DWORD md = CRYPT_MODE_CBC;
    CryptSetKeyParam(hk, KP_MODE, (BYTE *)&md, 0);
    CryptSetKeyParam(hk, KP_IV, (BYTE *)AES_IV.c_str(), 0);
    DWORD dl = (DWORD)ct.length();
    std::vector<BYTE> bf(dl);
    memcpy(bf.data(), ct.c_str(), dl);
    if (CryptDecrypt(hk, 0, TRUE, 0, bf.data(), &dl))
    {
        std::string r((char *)bf.data(), dl);
        CryptDestroyKey(hk);
        CryptDestroyHash(hh);
        CryptReleaseContext(hp, 0);
        return r;
    }
    CryptDestroyKey(hk);
    CryptDestroyHash(hh);
    CryptReleaseContext(hp, 0);
    return "";
}
std::string SH2(const std::string &in)
{
    unsigned int h = 2166136261u;
    for (size_t i = 0; i < in.length(); i++)
    {
        h ^= (unsigned char)in[i];
        h *= 16777619u;
    }
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = (h >> 16) ^ h;
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << h;
    return ss.str();
}
std::string GetHWID()
{
    std::string si;
    HKEY hk;
    char pi[256] = "";
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ | KEY_WOW64_64KEY, &hk) == ERROR_SUCCESS)
    {
        DWORD sz = sizeof(pi);
        RegQueryValueExA(hk, "ProductId", NULL, NULL, (LPBYTE)pi, &sz);
        RegCloseKey(hk);
    }
    si += pi;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ | KEY_WOW64_64KEY, &hk) == ERROR_SUCCESS)
    {
        DWORD it = 0;
        DWORD sz = sizeof(it);
        RegQueryValueExA(hk, "InstallDate", NULL, NULL, (LPBYTE)&it, &sz);
        RegCloseKey(hk);
        std::stringstream ss;
        ss << it;
        si += "-" + ss.str();
    }
    si += "-" + HWID_SEED;
    std::string h1 = SH2(si), h2 = SH2(h1 + "Round2"), h3 = SH2(h2 + "Final");
    std::stringstream r;
    r << "AZRN-" << h3.substr(0, 4) << "-" << h3.substr(4, 4) << "-" << h3.substr(4, 4);
    std::string fr = r.str();
    std::transform(fr.begin(), fr.end(), fr.begin(), ::toupper);
    return fr;
}
std::string GetCfgPath()
{
    char ad[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, ad)))
        return std::string(ad) + "\\azrien_dm_config.dat";
    return "C:\\azrien_dm_config.dat";
}
std::string HttpReq(const std::string &host, const std::string &path, const std::string &method, const std::string &postData, const std::string &token)
{
    HINTERNET hI = InternetOpenA("AzDM/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
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
    hd += "User-Agent: AzDM/1.0\r\n";
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
std::string GetContentFromJson(const std::string &json)
{
    size_t cp = json.find("\"content\":\"");
    if (cp == std::string::npos)
        return "";
    cp += 11;
    size_t ce = json.find("\"", cp);
    if (ce == std::string::npos)
        return "";
    std::string b64 = json.substr(cp, ce - cp);
    b64.erase(std::remove(b64.begin(), b64.end(), '\n'), b64.end());
    b64.erase(std::remove(b64.begin(), b64.end(), '\r'), b64.end());
    return B64Dec(b64);
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
    {
        oh = dec;
    }
    return !oh.empty();
}
struct SCred
{
    std::wstring u, p;
    bool ok;
    DWORD t;
};
void SaveCfg(const std::wstring &u, const std::wstring &p, bool r)
{
    std::string cp = GetCfgPath();
    if (!r || u.empty() || p.empty())
    {
        DeleteFileA(cp.c_str());
        return;
    }
    std::string pd = W2U(u) + "|" + W2U(p) + "|" + std::to_string(GetTickCount());
    std::string enc = AESEnc(pd);
    std::string b64 = B64Enc(enc);
    std::ofstream f(cp, std::ios::binary);
    if (f.is_open())
    {
        f.write(b64.c_str(), b64.length());
        f.close();
    }
}
SCred LoadCfg()
{
    SCred cr = {L"", L"", false, 0};
    std::string cp = GetCfgPath();
    std::ifstream f(cp, std::ios::binary);
    if (!f.is_open())
        return cr;
    std::string b64((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    if (b64.empty())
        return cr;
    std::string dec = AESDec(B64Dec(b64));
    size_t p1 = dec.find('|'), p2 = dec.find('|', p1 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos)
    {
        DeleteFileA(cp.c_str());
        return cr;
    }
    DWORD st;
    try
    {
        st = std::stoul(dec.substr(p2 + 1));
    }
    catch (...)
    {
        DeleteFileA(cp.c_str());
        return cr;
    }
    if (GetTickCount() - st > REM_DUR)
    {
        DeleteFileA(cp.c_str());
        return cr;
    }
    cr.u = U2W(dec.substr(0, p1));
    cr.p = U2W(dec.substr(p1 + 1, p2 - p1 - 1));
    cr.ok = true;
    cr.t = st;
    return cr;
}
void AutoFill()
{
    SCred cr = LoadCfg();
    if (cr.ok && !cr.u.empty() && !cr.p.empty())
    {
        g_wLoginUser = cr.u;
        g_wLoginPass = cr.p;
        g_bRememberMe = true;
    }
}
bool DoLoginCheck(const std::wstring &u, const std::wstring &p)
{
    std::string fc = HttpReq("gitee.com", "/api/v5/repos/" + GITEE_OWNER + "/" + GITEE_REPO + "/contents/users/" + W2U(u) + ".dat", "GET", "", GITEE_TOKEN);
    if (fc.empty())
    {
        g_wLoginMsg = L"Cannot connect to server";
        g_fMsgAlphaTarget = 1.0f;
        return false;
    }
    if (fc.find("\"Not Found\"") != std::string::npos)
    {
        g_wLoginMsg = L"Account does not exist";
        g_fMsgAlphaTarget = 1.0f;
        return false;
    }
    std::string storedHwid, storedRole;
    if (!ParseUserData(fc, storedHwid, storedRole))
    {
        g_wLoginMsg = L"File format error";
        g_fMsgAlphaTarget = 1.0f;
        return false;
    }
    std::string currentHwid = GetHWID();
    std::string expectedData = W2U(u) + ":" + W2U(p) + ":" + currentHwid;
    std::string expectedEncoded = B64Enc(expectedData);
    if (storedHwid == expectedEncoded)
    {
        g_wLoginMsg = L"";
        return true;
    }
    std::string decodedStored = B64Dec(storedHwid);
    if (decodedStored == expectedData)
    {
        g_wLoginMsg = L"";
        return true;
    }
    std::string userPassCheck = W2U(u) + ":" + W2U(p);
    if (decodedStored.find(userPassCheck) == 0)
    {
        g_wLoginMsg = L"Device mismatch";
        g_fMsgAlphaTarget = 1.0f;
        return false;
    }
    g_wLoginMsg = L"Invalid username or password";
    g_fMsgAlphaTarget = 1.0f;
    return false;
}
void DoLogin()
{
    if (g_wLoginUser.empty() || g_wLoginPass.empty())
    {
        g_wLoginMsg = L"Please enter username and password";
        g_fMsgAlphaTarget = 1.0f;
        return;
    }
    g_wLoginMsg = L"Logging in...";
    g_fMsgAlphaTarget = 1.0f;
    InvalidateRect(g_hWnd, NULL, FALSE);
    if (DoLoginCheck(g_wLoginUser, g_wLoginPass))
    {
        g_bLoggedIn = true;
        g_bLoginUI = false;
        g_wLoginMsg = L"";
        SaveCfg(g_wLoginUser, g_wLoginPass, g_bRememberMe);
        g_vScrollPos = 0;
        g_hScrollPos = 0;
        StartLoading();
        SetWindowPos(g_hWnd, NULL, (GetSystemMetrics(SM_CXSCREEN) - WINDOW_WIDTH) / 2, (GetSystemMetrics(SM_CYSCREEN) - WINDOW_HEIGHT) / 2, WINDOW_WIDTH, WINDOW_HEIGHT, SWP_NOZORDER);
        InvalidateRect(g_hWnd, NULL, TRUE);
    }
}
void DoRegister()
{
    if (g_wLoginUser.empty() || g_wLoginPass.empty())
    {
        g_wLoginMsg = L"Please enter username and password";
        g_fMsgAlphaTarget = 1.0f;
        return;
    }
    if (g_wLoginUser.length() < 3)
    {
        g_wLoginMsg = L"Username at least 3 chars";
        g_fMsgAlphaTarget = 1.0f;
        return;
    }
    if (g_wLoginPass.length() < 6)
    {
        g_wLoginMsg = L"Password at least 6 chars";
        g_fMsgAlphaTarget = 1.0f;
        return;
    }
    std::string hwid = GetHWID();
    std::string userData = W2U(g_wLoginUser) + ":" + W2U(g_wLoginPass) + ":" + hwid;
    std::string encodedHwid = B64Enc(userData);
    if (OpenClipboard(g_hWnd))
    {
        EmptyClipboard();
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, encodedHwid.length() + 1);
        if (h)
        {
            char *p = (char *)GlobalLock(h);
            if (p)
            {
                memcpy(p, encodedHwid.c_str(), encodedHwid.length() + 1);
                GlobalUnlock(h);
                SetClipboardData(CF_TEXT, h);
            }
        }
        CloseClipboard();
    }
    g_wLoginMsg = L"Reg code copied, send to admin";
    g_fMsgAlphaTarget = 1.0f;
}
bool HasMarker(const std::wstring &data, const std::wstring &marker)
{
    std::wstring lower = data;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    std::wstring m = marker;
    std::transform(m.begin(), m.end(), m.begin(), ::towlower);
    return lower.find(m) != std::wstring::npos;
}
void StartLoading()
{
    g_bLoading = true;
    g_fProgress = 0.0f;
    g_nTotalFiles = 0;
    g_nLoadedFiles = 0;
    g_nDuplicates = 0;
    g_wStatusText = L"Scanning directory...";
    g_vScrollPos = 0;
    g_hScrollPos = 0;
    g_loadState.tempData.clear();
    g_loadState.fileNames.clear();
    g_loadState.currentIndex = 0;
    g_loadState.totalFiles = 0;
    g_loadState.duplicates = 0;
    g_loadState.scanningDir = true;
    g_loadState.dirContent = "";
    g_loadState.parsePos = 0;
    g_loadState.dirContent = HttpReq("gitee.com", "/api/v5/repos/" + GITEE_OWNER + "/" + GITEE_REPO + "/contents/phones", "GET", "", GITEE_TOKEN);
}
void ProcessNextFile()
{
    if (!g_bLoading)
        return;
    if (g_loadState.scanningDir)
    {
        if (g_loadState.dirContent.find("\"name\"") == std::string::npos)
        {
            g_bLoading = false;
            g_wStatusText = L"No files found";
            return;
        }
        size_t pos = g_loadState.parsePos;
        while (pos < g_loadState.dirContent.length())
        {
            size_t namePos = g_loadState.dirContent.find("\"name\":\"", pos);
            if (namePos == std::string::npos)
                break;
            namePos += 8;
            size_t nameEnd = g_loadState.dirContent.find("\"", namePos);
            if (nameEnd == std::string::npos)
                break;
            std::string name = g_loadState.dirContent.substr(namePos, nameEnd - namePos);
            if (name.find(".txt") != std::string::npos)
                g_loadState.fileNames.push_back(name);
            pos = nameEnd + 1;
        }
        g_loadState.parsePos = pos;
        if (g_loadState.fileNames.empty())
        {
            g_bLoading = false;
            g_wStatusText = L"No .txt files found";
            return;
        }
        g_loadState.scanningDir = false;
        g_loadState.currentIndex = 0;
        g_loadState.totalFiles = (int)g_loadState.fileNames.size();
        g_nTotalFiles = g_loadState.totalFiles;
        g_wStatusText = L"Loading files...";
    }
    if (g_loadState.currentIndex < g_loadState.fileNames.size())
    {
        std::string &name = g_loadState.fileNames[g_loadState.currentIndex];
        try
        {
            size_t dot = name.find(".txt");
            int n = std::stoi(name.substr(0, dot));
            std::string resp = HttpReq("gitee.com", "/api/v5/repos/" + GITEE_OWNER + "/" + GITEE_REPO + "/contents/phones/" + name, "GET", "", GITEE_TOKEN);
            std::string content = GetContentFromJson(resp);
            if (!content.empty())
            {
                std::wstring wc = U2W(content);
                bool dup = false;
                for (auto &item : g_loadState.tempData)
                {
                    if (item.second == wc)
                    {
                        dup = true;
                        break;
                    }
                }
                if (!dup)
                    g_loadState.tempData.push_back({n, wc});
                else
                    g_loadState.duplicates++;
                g_nLoadedFiles++;
            }
        }
        catch (...)
        {
        }
        g_loadState.currentIndex++;
        g_fProgress = (float)g_loadState.currentIndex / (float)g_loadState.totalFiles;
        g_nDuplicates = g_loadState.duplicates;
        wchar_t buf[256];
        swprintf_s(buf, L"Loading: %d/%d files, %d duplicates removed", g_nLoadedFiles, g_nTotalFiles, g_nDuplicates);
        g_wStatusText = buf;
    }
    else
    {
        g_bLoading = false;
        std::sort(g_loadState.tempData.begin(), g_loadState.tempData.end(), [](const std::pair<int, std::wstring> &a, const std::pair<int, std::wstring> &b)
                  { return a.first < b.first; });
        g_allData.clear();
        g_fileNums.clear();
        for (auto &item : g_loadState.tempData)
        {
            g_fileNums.push_back(item.first);
            g_allData.push_back(item.second);
        }
        g_wStatusText = L"";
        g_vScrollPos = 0;
        g_hScrollPos = 0;
    }
}
std::vector<std::pair<int, std::wstring>> GetFilteredWithIndex()
{
    std::vector<std::pair<int, std::wstring>> f;
    std::wstring kw = g_searchText;
    std::transform(kw.begin(), kw.end(), kw.begin(), ::towlower);
    for (size_t i = 0; i < g_allData.size(); i++)
    {
        if (!g_searchText.empty())
        {
            std::wstring lower = g_allData[i];
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            if (lower.find(kw) == std::wstring::npos)
                continue;
        }
        if (g_bFilterTodesk)
        {
            std::wstring s = g_allData[i];
            size_t p = s.find(L"ToDesk:");
            if (p == std::wstring::npos)
                continue;
            p += 7;
            while (p < s.length() && (s[p] == L' ' || s[p] == L'\t'))
                p++;
            if (p >= s.length() || s[p] == L'\r' || s[p] == L'\n')
                continue;
        }
        if (g_bFilterBaidu)
        {
            std::wstring s = g_allData[i];
            size_t p = s.find(L"BaiduNetdisk:");
            if (p == std::wstring::npos)
                continue;
            p += 13;
            while (p < s.length() && (s[p] == L' ' || s[p] == L'\t'))
                p++;
            if (p >= s.length() || s[p] == L'\r' || s[p] == L'\n')
                continue;
        }
        if (g_bFilterBilibili)
        {
            std::wstring s = g_allData[i];
            size_t p = s.find(L"Bilibili:");
            if (p == std::wstring::npos)
                continue;
            p += 9;
            while (p < s.length() && (s[p] == L' ' || s[p] == L'\t'))
                p++;
            if (p >= s.length() || s[p] == L'\r' || s[p] == L'\n')
                continue;
        }
        f.push_back({(int)i, g_allData[i]});
    }
    return f;
}
void StartFilterAnimation()
{
    g_bFilterAnimating = true;
    g_dwFilterAnimStart = GetTickCount();
    g_fFilterAnimProgress = 0.0f;
}
float EaseOutCubic(float t) { return 1.0f - powf(1.0f - t, 3.0f); }
float EaseOutBack(float t)
{
    float c = 1.70158f;
    return 1.0f + (c + 1.0f) * powf(t - 1.0f, 3.0f) + c * powf(t - 1.0f, 2.0f);
}
float EaseOutElastic(float t)
{
    if (t == 0 || t == 1)
        return t;
    return powf(2, -10 * t) * sinf((t - 0.075f) * 6.28318f / 0.3f) + 1;
}
bool InitD2D(HWND hWnd)
{
    if (g_pF == nullptr)
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pF);
    RECT rc;
    GetClientRect(hWnd, &rc);
    if (g_pRT)
        g_pRT->Release();
    if (FAILED(g_pF->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hWnd, D2D1::SizeU(std::max(1u, (UINT32)(rc.right - rc.left)), std::max(1u, (UINT32)(rc.bottom - rc.top)))), &g_pRT)))
        return false;
    if (g_pWB)
        g_pWB->Release();
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &g_pWB);
    if (g_pDW == nullptr)
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&g_pDW);
    if (g_pSF)
        g_pSF->Release();
    if (g_pBF)
        g_pBF->Release();
    g_pDW->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"", &g_pSF);
    g_pDW->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"", &g_pBF);
    g_pSF->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return true;
}
void Cleanup()
{
    if (g_pSF)
    {
        g_pSF->Release();
        g_pSF = nullptr;
    }
    if (g_pBF)
    {
        g_pBF->Release();
        g_pBF = nullptr;
    }
    if (g_pDW)
    {
        g_pDW->Release();
        g_pDW = nullptr;
    }
    if (g_pWB)
    {
        g_pWB->Release();
        g_pWB = nullptr;
    }
    if (g_pRT)
    {
        g_pRT->Release();
        g_pRT = nullptr;
    }
    if (g_pF)
    {
        g_pF->Release();
        g_pF = nullptr;
    }
}
bool PtInR(int x, int y, RECT r) { return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom; }
void DrawPnl(RECT r, float al)
{
    D2D1_RECT_F f = {(FLOAT)r.left, (FLOAT)r.top, (FLOAT)r.right, (FLOAT)r.bottom};
    ComPtr<ID2D1SolidColorBrush> bg, bd;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.06f, 0.06f, 0.12f, 0.75f * al), &bg);
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.2f, 0.3f, 0.5f * al), &bd);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(f, 8, 8), bg.Get());
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(f, 8, 8), bd.Get(), 1.0f);
}
void DrawBtn(RECT r, bool hv, bool pr, float hs, const std::wstring &tx, D2D1::ColorF c)
{
    D2D1_RECT_F f = {(FLOAT)r.left, (FLOAT)r.top, (FLOAT)r.right, (FLOAT)r.bottom};
    float s = hv ? hs : 1.0f, cx = (f.left + f.right) / 2, cy = (f.top + f.bottom) / 2;
    float hw = (f.right - f.left) / 2 * s, hh = (f.bottom - f.top) / 2 * s;
    ComPtr<ID2D1SolidColorBrush> b;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(pr ? c.r * 0.6f : (hv ? c.r * 1.15f : c.r), pr ? c.g * 0.6f : (hv ? c.g * 1.15f : c.g), pr ? c.b * 0.6f : (hv ? c.b * 1.15f : c.b)), &b);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect({cx - hw, cy - hh, cx + hw, cy + hh}, 6, 6), b.Get());
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect({cx - hw, cy - hh, cx + hw, cy + hh}, 6, 6), g_pWB, 1.0f);
    IDWriteTextLayout *tl = nullptr;
    g_pDW->CreateTextLayout(tx.c_str(), (UINT32)tx.length(), g_pBF, hw * 2 - 10, hh * 2 - 8, &tl);
    if (tl)
    {
        tl->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        tl->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        g_pRT->DrawTextLayout({cx - hw + 5, cy - hh + 4}, tl, g_pWB);
        tl->Release();
    }
}
void DrawLoginInp(RECT r, bool active, const std::wstring &tx, bool pw, float glow)
{
    D2D1_RECT_F f = {(FLOAT)r.left, (FLOAT)r.top, (FLOAT)r.right, (FLOAT)r.bottom};
    ComPtr<ID2D1SolidColorBrush> bg, bd;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.10f, 0.15f, 0.9f), &bg);
    float rC = active ? 0.25f + 0.35f * glow : 0.4f;
    float gC = active ? 0.55f + 0.35f * glow : 0.4f;
    float bC = active ? 0.95f + 0.05f * glow : 0.8f;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(rC, gC, bC), &bd);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(f, 6, 6), bg.Get());
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(f, 6, 6), bd.Get(), 2.0f);
    std::wstring dt = tx;
    if (pw && !tx.empty())
        dt = std::wstring(tx.length(), L'*');
    if (active && (g_cursorBlink / 30) % 2 == 0)
        dt += L"|";
    if (!dt.empty())
        g_pRT->DrawText(dt.c_str(), (UINT32)dt.length(), g_pSF, D2D1::RectF(f.left + 10, f.top + 6, f.right - 10, f.bottom - 6), g_pWB);
}
void DrawLoginBtn(RECT r, bool hv, bool pr, float hs, const std::wstring &tx, D2D1::ColorF c)
{
    D2D1_RECT_F f = {(FLOAT)r.left, (FLOAT)r.top, (FLOAT)r.right, (FLOAT)r.bottom};
    float s = hs, cx = (f.left + f.right) / 2, cy = (f.top + f.bottom) / 2;
    float hw = (f.right - f.left) / 2 * s, hh = (f.bottom - f.top) / 2 * s;
    ComPtr<ID2D1SolidColorBrush> b;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(pr ? c.r * 0.6f : (hv ? c.r * 1.15f : c.r), pr ? c.g * 0.6f : (hv ? c.g * 1.15f : c.g), pr ? c.b * 0.6f : (hv ? c.b * 1.15f : c.b)), &b);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect({cx - hw, cy - hh, cx + hw, cy + hh}, 6, 6), b.Get());
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect({cx - hw, cy - hh, cx + hw, cy + hh}, 6, 6), g_pWB, 1.0f);
    IDWriteTextLayout *tl = nullptr;
    g_pDW->CreateTextLayout(tx.c_str(), (UINT32)tx.length(), g_pBF, hw * 2 - 10, hh * 2 - 8, &tl);
    if (tl)
    {
        tl->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        tl->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        g_pRT->DrawTextLayout({cx - hw + 5, cy - hh + 4}, tl, g_pWB);
        tl->Release();
    }
}
void DrawLoginChk(RECT r, bool ck, float alpha)
{
    float bs = 18.0f, bt = r.top + (r.bottom - r.top - bs) / 2;
    D2D1_RECT_F br = {(FLOAT)r.left, bt, (FLOAT)r.left + bs, bt + bs};
    ComPtr<ID2D1SolidColorBrush> bb, bbd;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.13f, 0.13f, 0.18f, alpha), &bb);
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.5f, 0.5f, 0.6f, alpha), &bbd);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(br, 3, 3), bb.Get());
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(br, 3, 3), bbd.Get(), 1.5f);
    if (ck)
    {
        ComPtr<ID2D1SolidColorBrush> cb;
        g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.8f, 0.4f, alpha), &cb);
        g_pRT->DrawLine({br.left + 3, br.top + bs / 2}, {br.left + bs / 3, br.top + bs - 3}, cb.Get(), 2.0f);
        g_pRT->DrawLine({br.left + bs / 3, br.top + bs - 3}, {br.left + bs - 2, br.top + 2}, cb.Get(), 2.0f);
    }
    ComPtr<ID2D1SolidColorBrush> lb;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.8f, 0.8f, 0.85f, alpha), &lb);
    g_pRT->DrawText(L"Remember me", 11, g_pSF, D2D1::RectF(br.right + 8, br.top, (FLOAT)r.right, br.bottom), lb.Get());
}
void DrawToggle(RECT r, bool active, bool hover, float animAlpha, const std::wstring &label)
{
    D2D1_RECT_F f = {(FLOAT)r.left, (FLOAT)r.top, (FLOAT)r.right, (FLOAT)r.bottom};
    float pillW = 30.0f, pillH = 18.0f;
    float pillX = f.left + 4, pillY = f.top + (f.bottom - f.top - pillH) / 2;
    ComPtr<ID2D1SolidColorBrush> pillBg;
    if (active)
        g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.70f, 0.40f, 0.8f + 0.2f * animAlpha), &pillBg);
    else
        g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.25f, 0.25f, 0.35f, 0.6f + 0.4f * animAlpha), &pillBg);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect({pillX, pillY, pillX + pillW, pillY + pillH}, pillH / 2, pillH / 2), pillBg.Get());
    float knobR = pillH / 2 - 2;
    float knobX = active ? pillX + pillW - knobR - 3 : pillX + knobR + 3;
    ComPtr<ID2D1SolidColorBrush> knobBrush;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 0.9f), &knobBrush);
    g_pRT->FillEllipse(D2D1::Ellipse({knobX, pillY + pillH / 2}, knobR, knobR), knobBrush.Get());
    ComPtr<ID2D1SolidColorBrush> labelBrush;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(active ? 0.85f : 0.6f, active ? 0.95f : 0.6f, active ? 0.9f : 0.7f, 0.8f + 0.2f * animAlpha), &labelBrush);
    g_pRT->DrawText(label.c_str(), (UINT32)label.length(), g_pSF, D2D1::RectF(pillX + pillW + 8, f.top + 2, f.right - 4, f.bottom - 2), labelBrush.Get());
}
void DrawProgressBar(RECT r, float progress)
{
    D2D1_RECT_F f = {(FLOAT)r.left, (FLOAT)r.top, (FLOAT)r.right, (FLOAT)r.bottom};
    ComPtr<ID2D1SolidColorBrush> bg;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.10f, 0.15f, 0.9f), &bg);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(f, 6, 6), bg.Get());
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(f, 6, 6), g_pWB, 1.0f);
    if (progress > 0.001f)
    {
        float fillW = (f.right - f.left) * progress;
        if (fillW > 12)
        {
            ComPtr<ID2D1SolidColorBrush> fill;
            g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.70f, 0.40f, 1.0f), &fill);
            g_pRT->FillRoundedRectangle(D2D1::RoundedRect({f.left, f.top, f.left + fillW, f.bottom}, 6, 6), fill.Get());
        }
    }
}
void DrawInp(RECT r, bool focused, const std::wstring &tx)
{
    D2D1_RECT_F f = {(FLOAT)r.left, (FLOAT)r.top, (FLOAT)r.right, (FLOAT)r.bottom};
    ComPtr<ID2D1SolidColorBrush> bg, bd;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.10f, 0.15f, 0.9f), &bg);
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(focused ? 0.35f : 0.4f, focused ? 0.65f : 0.4f, focused ? 1.0f : 0.8f), &bd);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(f, 6, 6), bg.Get());
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(f, 6, 6), bd.Get(), 2.0f);
    std::wstring dt = tx;
    if (focused && (g_cursorBlink / 30) % 2 == 0)
        dt += L"|";
    if (!dt.empty())
        g_pRT->DrawText(dt.c_str(), (UINT32)dt.length(), g_pSF, D2D1::RectF(f.left + 10, f.top + 6, f.right - 10, f.bottom - 6), g_pWB);
}
float CalcTextWidth(const std::wstring &text)
{
    IDWriteTextLayout *tl = nullptr;
    g_pDW->CreateTextLayout(text.c_str(), (UINT32)text.length(), g_pSF, 10000.0f, 30.0f, &tl);
    if (!tl)
        return 100.0f;
    DWRITE_TEXT_METRICS metrics;
    tl->GetMetrics(&metrics);
    tl->Release();
    return metrics.width + 20.0f;
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
    ComPtr<ID2D1SolidColorBrush> bgBrush;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.04f, 0.04f, 0.08f), &bgBrush);
    g_pRT->FillRectangle(D2D1::RectF(0, 0, W, H), bgBrush.Get());
    ID2D1GradientStopCollection *gs = nullptr;
    D2D1_GRADIENT_STOP st[2] = {{0, D2D1::ColorF(0.28f, 0.65f, 1.0f, 0.08f)}, {1, D2D1::ColorF(0.28f, 0.65f, 1.0f, 0)}};
    g_pRT->CreateGradientStopCollection(st, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &gs);
    if (gs)
    {
        ComPtr<ID2D1LinearGradientBrush> gb;
        g_pRT->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({0, 0}, {0, H}), gs, &gb);
        if (gb)
            g_pRT->FillRectangle(D2D1::RectF(0, 0, W, H), gb.Get());
        gs->Release();
    }
    if (g_bLoginUI)
    {
        int cx = (int)W / 2, sy = (int)H / 2 - 100;
        SetRect(&g_rcUserInput, cx - 150, sy, cx + 150, sy + 35);
        SetRect(&g_rcPassInput, cx - 150, sy + 50, cx + 150, sy + 85);
        SetRect(&g_rcRememberChk, cx - 150, sy + 95, cx + 150, sy + 115);
        SetRect(&g_rcLoginBtn, cx - 120, sy + 135, cx - 10, sy + 170);
        SetRect(&g_rcRegBtn, cx + 10, sy + 135, cx + 120, sy + 170);
        ComPtr<IDWriteTextFormat> titleFmt;
        g_pDW->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 22.0f, L"", &titleFmt);
        IDWriteTextLayout *titleLayout = nullptr;
        g_pDW->CreateTextLayout(L"Azrien Data Manager", 19, titleFmt.Get(), W - 40, 50.0f, &titleLayout);
        if (titleLayout)
        {
            ComPtr<ID2D1SolidColorBrush> titleBrush;
            g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.28f, 0.65f, 1.0f, 1.0f), &titleBrush);
            g_pRT->DrawTextLayout(D2D1::Point2F(22, 12 + g_fLoginTitleY), titleLayout, titleBrush.Get());
            titleLayout->Release();
        }
        if (!g_wLoginMsg.empty())
        {
            IDWriteTextLayout *tl = nullptr;
            g_pDW->CreateTextLayout(g_wLoginMsg.c_str(), (UINT32)g_wLoginMsg.length(), g_pSF, 400, 30, &tl);
            if (tl)
            {
                tl->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ComPtr<ID2D1SolidColorBrush> sb;
                g_pRT->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, g_fMsgAlpha), &sb);
                g_pRT->DrawTextLayout({(W - 400.0f) / 2, H / 2.0f - 130.0f}, tl, sb.Get());
                tl->Release();
            }
        }
        DrawLoginInp(g_rcUserInput, g_bUserActive, g_wLoginUser, false, g_fUserGlow);
        DrawLoginInp(g_rcPassInput, g_bPassActive, g_wLoginPass, true, g_fPassGlow);
        DrawLoginChk(g_rcRememberChk, g_bRememberMe, g_fChkAlpha);
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(g_hWnd, &pt);
        bool lbHov = PtInR(pt.x, pt.y, g_rcLoginBtn), rbHov = PtInR(pt.x, pt.y, g_rcRegBtn);
        DrawLoginBtn(g_rcLoginBtn, lbHov, g_bLoginBtnPress, g_fLoginBtnScale, L"Login", {0.16f, 0.42f, 0.82f});
        DrawLoginBtn(g_rcRegBtn, rbHov, g_bRegBtnPress, g_fRegBtnScale, L"Register", {0.16f, 0.42f, 0.82f});
    }
    else
    {
        const float padding = 20, sbSize = 14, itemH = 36.0f;
        ComPtr<IDWriteTextFormat> titleFmt;
        g_pDW->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"", &titleFmt);
        ComPtr<ID2D1SolidColorBrush> titleBrush;
        g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.28f, 0.65f, 1.0f, 1.0f), &titleBrush);
        g_pRT->DrawText(L"Azrien Data Manager", 19, titleFmt.Get(), D2D1::RectF(20, 12, W - 20, 40), titleBrush.Get());
        float topY = 60, btnW = 80, btnH = 30, gap = 10;
        float filterRowH = 28;
        SetRect(&g_rcSearchBox, (int)padding, (int)topY, (int)(W - padding - btnW - gap), (int)(topY + btnH));
        SetRect(&g_rcSearchBtn, (int)(W - padding - btnW), (int)topY, (int)(W - padding), (int)(topY + btnH));
        DrawInp(g_rcSearchBox, g_searchFocused, g_searchText);
        DrawBtn(g_rcSearchBtn, g_bSearchHover, g_bSearchPress, g_fSearchBtnHover, L"Search", {0.16f, 0.42f, 0.82f});
        float filterY = topY + btnH + 8;
        float toggleW = (W - padding * 2 - gap * 3) / 3;
        SetRect(&g_rcFilterTodesk, (int)padding, (int)filterY, (int)(padding + toggleW), (int)(filterY + filterRowH));
        SetRect(&g_rcFilterBaidu, (int)(padding + toggleW + gap), (int)filterY, (int)(padding + toggleW * 2 + gap), (int)(filterY + filterRowH));
        SetRect(&g_rcFilterBilibili, (int)(padding + toggleW * 2 + gap * 2), (int)filterY, (int)(padding + toggleW * 3 + gap * 2), (int)(filterY + filterRowH));
        DrawToggle(g_rcFilterTodesk, g_bFilterTodesk, g_bTodeskHover, g_fTodeskAlpha, L"Todesk");
        DrawToggle(g_rcFilterBaidu, g_bFilterBaidu, g_bBaiduHover, g_fBaiduAlpha, L"BaiduNetdisk");
        DrawToggle(g_rcFilterBilibili, g_bFilterBilibili, g_bBilibiliHover, g_fBilibiliAlpha, L"Bilibili");
        float listY = filterY + filterRowH + 10;
        float listH = H - listY - 80;
        SetRect(&g_rcList, (int)padding, (int)listY, (int)(W - padding), (int)(listY + listH));
        DrawPnl(g_rcList, 1.0f);
        if (g_bLoading)
        {
            RECT progRect = {(int)(padding + 40), (int)(listY + listH / 2 - 10), (int)(W - padding - 40), (int)(listY + listH / 2 + 16)};
            DrawProgressBar(progRect, g_fProgress);
            IDWriteTextLayout *stl = nullptr;
            g_pDW->CreateTextLayout(g_wStatusText.c_str(), (UINT32)g_wStatusText.length(), g_pSF, W - padding * 2 - 80, 30, &stl);
            if (stl)
            {
                stl->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ComPtr<ID2D1SolidColorBrush> sb;
                g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.9f, 0.9f, 0.95f, 1.0f), &sb);
                g_pRT->DrawTextLayout({padding + 40, listY + listH / 2 - 50}, stl, sb.Get());
                stl->Release();
            }
            wchar_t pct[16];
            swprintf_s(pct, L"%.0f%%", g_fProgress * 100);
            ComPtr<IDWriteTextFormat> pctFmt;
            g_pDW->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"", &pctFmt);
            ComPtr<ID2D1SolidColorBrush> pb;
            g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.28f, 0.65f, 1.0f, 1.0f), &pb);
            g_pRT->DrawText(pct, (UINT32)wcslen(pct), pctFmt.Get(), D2D1::RectF(padding + 40, listY + listH / 2 + 22, W - padding - 40, listY + listH / 2 + 44), pb.Get());
        }
        else if (g_allData.empty() && g_wStatusText.empty())
        {
            IDWriteTextLayout *etl = nullptr;
            g_pDW->CreateTextLayout(L"Click Refresh to load data", 25, g_pSF, W - padding * 2 - 40, 30, &etl);
            if (etl)
            {
                etl->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                ComPtr<ID2D1SolidColorBrush> eb;
                g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.5f, 0.5f, 0.6f, 1.0f), &eb);
                g_pRT->DrawTextLayout({padding + 20, listY + listH / 2 - 15}, etl, eb.Get());
                etl->Release();
            }
        }
        else if (!g_allData.empty())
        {
            auto filtered = GetFilteredWithIndex();
            int totalItems = (int)filtered.size();
            float viewportW = W - padding * 2 - sbSize - 4;
            float viewportH = listH - sbSize - 4;
            float maxTextWidth = 0;
            for (int i = 0; i < totalItems; i++)
            {
                std::wstring line = L"[" + std::to_wstring(g_fileNums[filtered[i].first]) + L".txt] " + filtered[i].second;
                std::replace(line.begin(), line.end(), L'\n', L' ');
                std::replace(line.begin(), line.end(), L'\r', L' ');
                float tw = CalcTextWidth(line);
                if (tw > maxTextWidth)
                    maxTextWidth = tw;
            }
            float totalContentW = maxTextWidth + 30.0f;
            float totalContentH = (float)totalItems * itemH + 8.0f;
            int maxVScroll = (int)(totalContentH - viewportH);
            if (maxVScroll < 0)
                maxVScroll = 0;
            int maxHScroll = (int)(totalContentW - viewportW);
            if (maxHScroll < 0)
                maxHScroll = 0;
            if (g_vScrollPos > maxVScroll)
                g_vScrollPos = maxVScroll;
            if (g_vScrollPos < 0)
                g_vScrollPos = 0;
            if (g_hScrollPos > maxHScroll)
                g_hScrollPos = maxHScroll;
            if (g_hScrollPos < 0)
                g_hScrollPos = 0;
            float animOffset = 0.0f;
            if (g_bFilterAnimating)
            {
                DWORD elapsed = GetTickCount() - g_dwFilterAnimStart;
                float duration = 400.0f;
                if (elapsed >= duration)
                {
                    g_bFilterAnimating = false;
                    g_fFilterAnimProgress = 1.0f;
                }
                else
                {
                    g_fFilterAnimProgress = EaseOutCubic((float)elapsed / duration);
                }
                animOffset = (1.0f - g_fFilterAnimProgress) * 60.0f;
            }
            g_pRT->PushAxisAlignedClip(D2D1::RectF(padding + 1, listY + 1, W - padding - sbSize - 1, listY + listH - sbSize - 1), D2D1_ANTIALIAS_MODE_ALIASED);
            for (int i = 0; i < totalItems; i++)
            {
                float y = listY + 4.0f + i * itemH - (float)g_vScrollPos;
                if (y + itemH < listY || y > listY + listH)
                    continue;
                float alpha = g_bFilterAnimating ? (0.3f + 0.7f * g_fFilterAnimProgress) : 1.0f;
                float offsetX = animOffset * (1.0f - (float)i / (float)(std::max)(1, totalItems));
                D2D1_RECT_F ir = D2D1::RectF(padding + 4 - (float)g_hScrollPos + offsetX, y, padding + 4 - (float)g_hScrollPos + totalContentW + offsetX, y + itemH);
                if (i == g_hoverItem)
                {
                    ComPtr<ID2D1SolidColorBrush> hb;
                    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.28f, 0.65f, 1.0f, 0.12f * alpha), &hb);
                    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(ir, 4, 4), hb.Get());
                }
                bool isSelected = false;
                for (int si : g_selectedIndices)
                    if (si == filtered[i].first)
                    {
                        isSelected = true;
                        break;
                    }
                if (isSelected)
                {
                    ComPtr<ID2D1SolidColorBrush> sb;
                    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.16f, 0.42f, 0.82f, 0.2f * alpha), &sb);
                    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(ir, 4, 4), sb.Get());
                }
                std::wstring line = L"[" + std::to_wstring(g_fileNums[filtered[i].first]) + L".txt] " + filtered[i].second;
                std::replace(line.begin(), line.end(), L'\n', L' ');
                std::replace(line.begin(), line.end(), L'\r', L' ');
                ComPtr<ID2D1SolidColorBrush> tb;
                g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.9f, 0.9f, 0.95f, alpha), &tb);
                g_pRT->DrawText(line.c_str(), (UINT32)line.length(), g_pSF, D2D1::RectF(ir.left + 12, ir.top + 2, ir.left + 12 + maxTextWidth, ir.top + itemH - 4), tb.Get());
            }
            g_pRT->PopAxisAlignedClip();
            if (maxVScroll > 0)
            {
                float sbX = W - padding - sbSize + 2, sbY = listY + 2, sbH = listH - sbSize - 4;
                ComPtr<ID2D1SolidColorBrush> trackBrush;
                g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.18f, 0.5f), &trackBrush);
                g_pRT->FillRoundedRectangle(D2D1::RoundedRect({sbX, sbY, sbX + sbSize - 4, sbY + sbH}, 5, 5), trackBrush.Get());
                float thumbH = (viewportH / totalContentH) * sbH;
                if (thumbH < 30)
                    thumbH = 30;
                float thumbY = sbY + (sbH - thumbH) * ((float)g_vScrollPos / (float)maxVScroll);
                ComPtr<ID2D1SolidColorBrush> thumbBrush;
                g_pRT->CreateSolidColorBrush(D2D1::ColorF(g_vScrollbarDragging ? 0.45f : 0.35f, g_vScrollbarDragging ? 0.45f : 0.35f, g_vScrollbarDragging ? 0.55f : 0.45f, 0.8f), &thumbBrush);
                g_pRT->FillRoundedRectangle(D2D1::RoundedRect({sbX, thumbY, sbX + sbSize - 4, thumbY + thumbH}, 5, 5), thumbBrush.Get());
            }
            if (maxHScroll > 0)
            {
                float sbY = listY + listH - sbSize + 2, sbX = padding + 2, sbW = W - padding * 2 - sbSize - 4;
                ComPtr<ID2D1SolidColorBrush> trackBrush;
                g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.18f, 0.5f), &trackBrush);
                g_pRT->FillRoundedRectangle(D2D1::RoundedRect({sbX, sbY, sbX + sbW, sbY + sbSize - 4}, 5, 5), trackBrush.Get());
                float thumbW = (viewportW / totalContentW) * sbW;
                if (thumbW < 40)
                    thumbW = 40;
                float thumbX = sbX + (sbW - thumbW) * ((float)g_hScrollPos / (float)maxHScroll);
                ComPtr<ID2D1SolidColorBrush> thumbBrush;
                g_pRT->CreateSolidColorBrush(D2D1::ColorF(g_hScrollbarDragging ? 0.45f : 0.35f, g_hScrollbarDragging ? 0.45f : 0.35f, g_hScrollbarDragging ? 0.55f : 0.45f, 0.8f), &thumbBrush);
                g_pRT->FillRoundedRectangle(D2D1::RoundedRect({thumbX, sbY, thumbX + thumbW, sbY + sbSize - 4}, 5, 5), thumbBrush.Get());
            }
        }
        float btnY = H - 60;
        float exportW = 140, refreshW = 100, exportAllW = 100;
        SetRect(&g_rcExportBtn, (int)padding, (int)btnY, (int)(padding + exportW), (int)(btnY + 36));
        SetRect(&g_rcRefreshBtn, (int)(padding + exportW + 12), (int)btnY, (int)(padding + exportW + 12 + refreshW), (int)(btnY + 36));
        SetRect(&g_rcExportAllBtn, (int)(padding + exportW + refreshW + 24), (int)btnY, (int)(padding + exportW + refreshW + 24 + exportAllW), (int)(btnY + 36));
        DrawBtn(g_rcExportBtn, g_bExportHover, g_bExportPress, g_fExportHover, L"Export Selected", {0.16f, 0.42f, 0.82f});
        DrawBtn(g_rcRefreshBtn, g_bRefreshHover, g_bRefreshPress, g_fRefreshHover, L"Refresh", {0.12f, 0.55f, 0.35f});
        DrawBtn(g_rcExportAllBtn, g_bExportAllHover, g_bExportAllPress, g_fExportAllHover, L"Export All", {0.35f, 0.30f, 0.55f});
        std::wstring cnt = L"Total: " + std::to_wstring(g_allData.size()) + L" records";
        ComPtr<ID2D1SolidColorBrush> cntB;
        g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.55f, 0.65f, 1.0f), &cntB);
        g_pRT->DrawText(cnt.c_str(), (UINT32)cnt.length(), g_pSF, D2D1::RectF(W - 250, btnY + 8, W - padding, btnY + 28), cntB.Get());
    }
    g_pRT->EndDraw();
    EndPaint(g_hWnd, &ps);
}
bool IsOnVScrollbar(int x, int y)
{
    if (g_bLoginUI)
        return false;
    RECT cr;
    GetClientRect(g_hWnd, &cr);
    float W = (float)cr.right, H = (float)cr.bottom;
    float padding = 20, sbSize = 14;
    float filterY = 60 + 30 + 8 + 28 + 10;
    float listH = H - filterY - 80;
    float sbX = W - padding - sbSize + 2;
    return x >= sbX && x <= sbX + sbSize && y >= filterY && y <= filterY + listH - sbSize;
}
bool IsOnHScrollbar(int x, int y)
{
    if (g_bLoginUI)
        return false;
    RECT cr;
    GetClientRect(g_hWnd, &cr);
    float W = (float)cr.right, H = (float)cr.bottom;
    float padding = 20, sbSize = 14;
    float filterY = 60 + 30 + 8 + 28 + 10;
    float listH = H - filterY - 80;
    float sbY = filterY + listH - sbSize + 2;
    return x >= padding && x <= W - padding - sbSize && y >= sbY && y <= sbY + sbSize;
}
void ToggleFilter(int which)
{
    switch (which)
    {
    case 0:
        g_bFilterTodesk = !g_bFilterTodesk;
        g_fTodeskTarget = g_bFilterTodesk ? 1.0f : 0.3f;
        break;
    case 1:
        g_bFilterBaidu = !g_bFilterBaidu;
        g_fBaiduTarget = g_bFilterBaidu ? 1.0f : 0.3f;
        break;
    case 2:
        g_bFilterBilibili = !g_bFilterBilibili;
        g_fBilibiliTarget = g_bFilterBilibili ? 1.0f : 0.3f;
        break;
    }
    g_vScrollPos = 0;
    g_hScrollPos = 0;
    g_selectedIndices.clear();
    StartFilterAnimation();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM lP)
{
    switch (msg)
    {
    case WM_CREATE:
        SetTimer(hWnd, 1, 16, NULL);
        g_bLoginTitleAnim = true;
        g_dwLoginTitleStart = GetTickCount();
        AutoFill();
        break;
    case WM_TIMER:
        g_cursorBlink++;
        if (g_bLoginUI)
        {
            if (g_bLoginTitleAnim)
            {
                float t = (GetTickCount() - g_dwLoginTitleStart) / 700.0f;
                if (t >= 1.0f)
                {
                    t = 1.0f;
                    g_bLoginTitleAnim = false;
                }
                g_fLoginTitleY = -50.0f * (1.0f - EaseOutElastic(std::min(t * 1.2f, 1.0f)));
            }
            if (g_bLoginFadingIn)
            {
                g_fChkAlpha += 0.05f;
                if (g_fChkAlpha >= 1.0f)
                {
                    g_fChkAlpha = 1.0f;
                    g_bLoginFadingIn = false;
                }
            }
            g_fUserGlow += (g_fUserGlowTarget - g_fUserGlow) * 0.2f;
            g_fPassGlow += (g_fPassGlowTarget - g_fPassGlow) * 0.2f;
            g_fMsgAlpha += (g_fMsgAlphaTarget - g_fMsgAlpha) * 0.15f;
            g_fLoginBtnScale += (g_fLoginBtnScaleTarget - g_fLoginBtnScale) * 0.2f;
            g_fRegBtnScale += (g_fRegBtnScaleTarget - g_fRegBtnScale) * 0.2f;
        }
        else
        {
            g_fSearchBtnHover += (g_fSearchBtnTarget - g_fSearchBtnHover) * 0.18f;
            g_fExportHover += (g_fExportTarget - g_fExportHover) * 0.18f;
            g_fRefreshHover += (g_fRefreshTarget - g_fRefreshHover) * 0.18f;
            g_fExportAllHover += (g_fExportAllTarget - g_fExportAllHover) * 0.18f;
            g_fTodeskAlpha += (g_fTodeskTarget - g_fTodeskAlpha) * 0.2f;
            g_fBaiduAlpha += (g_fBaiduTarget - g_fBaiduAlpha) * 0.2f;
            g_fBilibiliAlpha += (g_fBilibiliTarget - g_fBilibiliAlpha) * 0.2f;
            if (g_bLoading)
                ProcessNextFile();
        }
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
            if (rc.right - rc.left > 0 && rc.bottom - rc.top > 0)
                g_pRT->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
        }
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lP), y = GET_Y_LPARAM(lP);
        if (g_bLoginUI)
        {
            g_bLoginBtnHover = PtInR(x, y, g_rcLoginBtn);
            g_fLoginBtnScaleTarget = g_bLoginBtnHover ? 1.03f : 1.0f;
            g_bRegBtnHover = PtInR(x, y, g_rcRegBtn);
            g_fRegBtnScaleTarget = g_bRegBtnHover ? 1.03f : 1.0f;
            break;
        }
        if (g_vScrollbarDragging)
        {
            RECT cr;
            GetClientRect(hWnd, &cr);
            float Wf = (float)cr.right, Hf = (float)cr.bottom;
            float pad = 20, sb = 14, ih = 36.0f;
            float fy = 60 + 30 + 8 + 28 + 10;
            float lh = Hf - fy - 80;
            float sbH = lh - sb - 4, vph = lh - sb - 4;
            auto filtered = GetFilteredWithIndex();
            float tch = (float)filtered.size() * ih + 8.0f;
            int mv = (int)(tch - vph);
            if (mv < 0)
                mv = 0;
            float th = (vph / tch) * sbH;
            if (th < 30)
                th = 30;
            float ratio = (float)(y - fy - 2 - th / 2) / (sbH - th);
            g_vScrollPos = (int)(ratio * mv);
            if (g_vScrollPos < 0)
                g_vScrollPos = 0;
            if (g_vScrollPos > mv)
                g_vScrollPos = mv;
            break;
        }
        if (g_hScrollbarDragging)
        {
            RECT cr;
            GetClientRect(hWnd, &cr);
            float Wf = (float)cr.right, Hf = (float)cr.bottom;
            float pad = 20, sb = 14, fy = 60 + 30 + 8 + 28 + 10;
            float lh = Hf - fy - 80;
            float sbW = Wf - pad * 2 - sb - 4;
            auto filtered = GetFilteredWithIndex();
            float mtw = 0;
            for (auto &f : filtered)
            {
                std::wstring line = L"[" + std::to_wstring(g_fileNums[f.first]) + L".txt] " + f.second;
                std::replace(line.begin(), line.end(), L'\n', L' ');
                std::replace(line.begin(), line.end(), L'\r', L' ');
                float tw = CalcTextWidth(line);
                if (tw > mtw)
                    mtw = tw;
            }
            float tcw = mtw + 30.0f, vpw = Wf - pad * 2 - sb - 4;
            int mh = (int)(tcw - vpw);
            if (mh < 0)
                mh = 0;
            float tw = (vpw / tcw) * sbW;
            if (tw < 40)
                tw = 40;
            float ratio = (float)(x - pad - 2 - tw / 2) / (sbW - tw);
            g_hScrollPos = (int)(ratio * mh);
            if (g_hScrollPos < 0)
                g_hScrollPos = 0;
            if (g_hScrollPos > mh)
                g_hScrollPos = mh;
            break;
        }
        g_bSearchHover = PtInR(x, y, g_rcSearchBtn);
        g_fSearchBtnTarget = g_bSearchHover ? 1.03f : 1.0f;
        g_bExportHover = PtInR(x, y, g_rcExportBtn);
        g_fExportTarget = g_bExportHover ? 1.03f : 1.0f;
        g_bRefreshHover = PtInR(x, y, g_rcRefreshBtn);
        g_fRefreshTarget = g_bRefreshHover ? 1.03f : 1.0f;
        g_bExportAllHover = PtInR(x, y, g_rcExportAllBtn);
        g_fExportAllTarget = g_bExportAllHover ? 1.03f : 1.0f;
        g_bTodeskHover = PtInR(x, y, g_rcFilterTodesk);
        g_bBaiduHover = PtInR(x, y, g_rcFilterBaidu);
        g_bBilibiliHover = PtInR(x, y, g_rcFilterBilibili);
        RECT cr;
        GetClientRect(hWnd, &cr);
        float fy = 60 + 30 + 8 + 28 + 10;
        if (x >= g_rcList.left + 4 && x <= g_rcList.right - 18 && y >= fy + 4 && y <= g_rcList.bottom - 18 && !g_bLoading && !g_allData.empty())
        {
            g_hoverItem = (int)((y - fy - 4 + g_vScrollPos) / 36);
            auto filtered = GetFilteredWithIndex();
            if (g_hoverItem >= (int)filtered.size())
                g_hoverItem = -1;
        }
        else
            g_hoverItem = -1;
        break;
    }
    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lP), y = GET_Y_LPARAM(lP);
        if (g_bLoginUI)
        {
            if (PtInR(x, y, g_rcUserInput))
            {
                g_bUserActive = true;
                g_bPassActive = false;
                g_fUserGlowTarget = 1.0f;
                g_fPassGlowTarget = 0.0f;
            }
            else if (PtInR(x, y, g_rcPassInput))
            {
                g_bUserActive = false;
                g_bPassActive = true;
                g_fUserGlowTarget = 0.0f;
                g_fPassGlowTarget = 1.0f;
            }
            else if (PtInR(x, y, g_rcRememberChk))
            {
                g_bRememberMe = !g_bRememberMe;
                if (!g_bRememberMe)
                    DeleteFileA(GetCfgPath().c_str());
            }
            else if (PtInR(x, y, g_rcLoginBtn))
            {
                g_bLoginBtnPress = true;
                SetCapture(hWnd);
            }
            else if (PtInR(x, y, g_rcRegBtn))
            {
                g_bRegBtnPress = true;
                SetCapture(hWnd);
            }
            else
            {
                g_bUserActive = false;
                g_bPassActive = false;
                g_fUserGlowTarget = 0.0f;
                g_fPassGlowTarget = 0.0f;
            }
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        if (!g_bLoading && !g_allData.empty())
        {
            if (IsOnVScrollbar(x, y))
            {
                g_vScrollbarDragging = true;
                SetCapture(hWnd);
                break;
            }
            if (IsOnHScrollbar(x, y))
            {
                g_hScrollbarDragging = true;
                SetCapture(hWnd);
                break;
            }
        }
        if (PtInR(x, y, g_rcSearchBox))
        {
            g_searchFocused = true;
            SetFocus(hWnd);
        }
        else if (PtInR(x, y, g_rcSearchBtn))
            g_bSearchPress = true;
        else if (PtInR(x, y, g_rcExportBtn))
            g_bExportPress = true;
        else if (PtInR(x, y, g_rcRefreshBtn))
            g_bRefreshPress = true;
        else if (PtInR(x, y, g_rcExportAllBtn))
            g_bExportAllPress = true;
        else if (PtInR(x, y, g_rcFilterTodesk))
            ToggleFilter(0);
        else if (PtInR(x, y, g_rcFilterBaidu))
            ToggleFilter(1);
        else if (PtInR(x, y, g_rcFilterBilibili))
            ToggleFilter(2);
        else
            g_searchFocused = false;
        if (g_hoverItem >= 0 && !g_bLoading && !g_allData.empty())
        {
            RECT cr;
            GetClientRect(hWnd, &cr);
            float fy = 60 + 30 + 8 + 28 + 10;
            if (x >= g_rcList.left + 4 && x <= g_rcList.right - 18 && y >= fy + 4 && y <= g_rcList.bottom - 18)
            {
                auto filtered = GetFilteredWithIndex();
                if (g_hoverItem < (int)filtered.size())
                {
                    int realIdx = filtered[g_hoverItem].first;
                    auto it = std::find(g_selectedIndices.begin(), g_selectedIndices.end(), realIdx);
                    if (it != g_selectedIndices.end())
                        g_selectedIndices.erase(it);
                    else
                        g_selectedIndices.push_back(realIdx);
                }
            }
        }
        break;
    }
    case WM_LBUTTONUP:
    {
        int x = GET_X_LPARAM(lP), y = GET_Y_LPARAM(lP);
        if (g_bLoginUI)
        {
            if (g_bLoginBtnPress)
            {
                g_bLoginBtnPress = false;
                ReleaseCapture();
                if (PtInR(x, y, g_rcLoginBtn))
                    DoLogin();
            }
            if (g_bRegBtnPress)
            {
                g_bRegBtnPress = false;
                ReleaseCapture();
                if (PtInR(x, y, g_rcRegBtn))
                    DoRegister();
            }
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        if (g_vScrollbarDragging)
        {
            g_vScrollbarDragging = false;
            ReleaseCapture();
            break;
        }
        if (g_hScrollbarDragging)
        {
            g_hScrollbarDragging = false;
            ReleaseCapture();
            break;
        }
        if (g_bSearchPress && PtInR(x, y, g_rcSearchBtn))
        {
            g_searchFocused = true;
            g_bSearchPress = false;
        }
        if (g_bExportPress && PtInR(x, y, g_rcExportBtn))
        {
            g_bExportPress = false;
            if (!g_selectedIndices.empty())
            {
                std::wstring data;
                for (size_t i = 0; i < g_selectedIndices.size(); i++)
                {
                    int idx = g_selectedIndices[i];
                    if (i > 0)
                        data += L"\r\n=========\r\n\r\n";
                    data += L"[" + std::to_wstring(g_fileNums[idx]) + L".txt]\r\n" + g_allData[idx] + L"\r\n";
                }
                OPENFILENAMEW ofn = {sizeof(ofn)};
                wchar_t path[MAX_PATH] = L"export.txt";
                ofn.lpstrFilter = L"Text Files\0*.txt\0\0";
                ofn.lpstrFile = path;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_OVERWRITEPROMPT;
                if (GetSaveFileNameW(&ofn))
                {
                    std::string u8 = W2U(data);
                    std::ofstream f(ofn.lpstrFile, std::ios::binary);
                    if (f.is_open())
                    {
                        unsigned char bom[] = {0xEF, 0xBB, 0xBF};
                        f.write((char *)bom, 3);
                        f.write(u8.c_str(), u8.length());
                        f.close();
                        g_selectedIndices.clear();
                        MessageBoxW(hWnd, L"Export success!", L"Success", MB_OK);
                    }
                }
            }
            else
                MessageBoxW(hWnd, L"Click items in the list to select, then click Export Selected.", L"Info", MB_OK);
        }
        if (g_bRefreshPress && PtInR(x, y, g_rcRefreshBtn))
        {
            g_bRefreshPress = false;
            g_vScrollPos = 0;
            g_hScrollPos = 0;
            g_selectedIndices.clear();
            g_wStatusText = L"";
            g_bFilterTodesk = g_bFilterBaidu = g_bFilterBilibili = false;
            g_fTodeskTarget = g_fBaiduTarget = g_fBilibiliTarget = 0.3f;
            StartLoading();
        }
        if (g_bExportAllPress && PtInR(x, y, g_rcExportAllBtn))
        {
            g_bExportAllPress = false;
            if (g_allData.empty())
                MessageBoxW(hWnd, L"No data! Click Refresh first.", L"Error", MB_ICONERROR);
            else
            {
                auto filtered = GetFilteredWithIndex();
                std::wstring data;
                for (size_t i = 0; i < filtered.size(); i++)
                {
                    int idx = filtered[i].first;
                    if (i > 0)
                        data += L"\r\n=========\r\n\r\n";
                    data += L"[" + std::to_wstring(g_fileNums[idx]) + L".txt]\r\n" + g_allData[idx] + L"\r\n";
                }
                OPENFILENAMEW ofn = {sizeof(ofn)};
                wchar_t path[MAX_PATH] = L"export_all.txt";
                ofn.lpstrFilter = L"Text Files\0*.txt\0\0";
                ofn.lpstrFile = path;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_OVERWRITEPROMPT;
                if (GetSaveFileNameW(&ofn))
                {
                    std::string u8 = W2U(data);
                    std::ofstream f(ofn.lpstrFile, std::ios::binary);
                    if (f.is_open())
                    {
                        unsigned char bom[] = {0xEF, 0xBB, 0xBF};
                        f.write((char *)bom, 3);
                        f.write(u8.c_str(), u8.length());
                        f.close();
                    }
                }
            }
        }
        g_bSearchPress = g_bExportPress = g_bRefreshPress = g_bExportAllPress = false;
        break;
    }
    case WM_MOUSEWHEEL:
    {
        if (g_bLoginUI || g_bLoading || g_allData.empty())
            break;
        int delta = GET_WHEEL_DELTA_WPARAM(wP);
        if (GetKeyState(VK_SHIFT) & 0x8000)
        {
            g_hScrollPos -= delta / 3;
            if (g_hScrollPos < 0)
                g_hScrollPos = 0;
            RECT cr;
            GetClientRect(hWnd, &cr);
            float W = (float)cr.right, pad = 20, sb = 14;
            auto filtered = GetFilteredWithIndex();
            float mtw = 0;
            for (auto &f : filtered)
            {
                std::wstring line = L"[" + std::to_wstring(g_fileNums[f.first]) + L".txt] " + f.second;
                std::replace(line.begin(), line.end(), L'\n', L' ');
                std::replace(line.begin(), line.end(), L'\r', L' ');
                float tw = CalcTextWidth(line);
                if (tw > mtw)
                    mtw = tw;
            }
            int mh = (int)(mtw + 30.0f - (W - pad * 2 - sb - 4));
            if (mh < 0)
                mh = 0;
            if (g_hScrollPos > mh)
                g_hScrollPos = mh;
        }
        else
        {
            g_vScrollPos -= delta / 3;
            if (g_vScrollPos < 0)
                g_vScrollPos = 0;
            RECT cr;
            GetClientRect(hWnd, &cr);
            float H = (float)cr.bottom, pad = 20, sb = 14, ih = 36.0f;
            float fy = 60 + 30 + 8 + 28 + 10;
            float lh = H - fy - 80;
            auto filtered = GetFilteredWithIndex();
            int mv = (int)((float)filtered.size() * ih + 8.0f - (lh - sb - 4));
            if (mv < 0)
                mv = 0;
            if (g_vScrollPos > mv)
                g_vScrollPos = mv;
        }
        break;
    }
    case WM_KEYDOWN:
    {
        if (g_bLoginUI)
        {
            if (wP == VK_BACK)
            {
                if (g_bUserActive && !g_wLoginUser.empty())
                    g_wLoginUser.pop_back();
                else if (g_bPassActive && !g_wLoginPass.empty())
                    g_wLoginPass.pop_back();
            }
            else if (wP == VK_RETURN)
            {
                DoLogin();
            }
            else if (wP == VK_TAB)
            {
                g_bUserActive = !g_bUserActive;
                g_bPassActive = !g_bPassActive;
                g_fUserGlowTarget = g_bUserActive ? 1.0f : 0.0f;
                g_fPassGlowTarget = g_bPassActive ? 1.0f : 0.0f;
            }
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        if (g_searchFocused)
        {
            if (GetKeyState(VK_CONTROL) & 0x8000)
            {
                switch (wP)
                {
                case 'V':
                    if (OpenClipboard(hWnd))
                    {
                        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                        if (hData)
                        {
                            wchar_t *pszText = (wchar_t *)GlobalLock(hData);
                            if (pszText)
                            {
                                g_searchText += pszText;
                                GlobalUnlock(hData);
                            }
                        }
                        CloseClipboard();
                    }
                    g_vScrollPos = 0;
                    g_hScrollPos = 0;
                    g_selectedIndices.clear();
                    break;
                case 'C':
                    if (!g_searchText.empty() && OpenClipboard(hWnd))
                    {
                        EmptyClipboard();
                        size_t len = (g_searchText.length() + 1) * sizeof(wchar_t);
                        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
                        if (hMem)
                        {
                            memcpy(GlobalLock(hMem), g_searchText.c_str(), len);
                            GlobalUnlock(hMem);
                            SetClipboardData(CF_UNICODETEXT, hMem);
                        }
                        CloseClipboard();
                    }
                    break;
                case 'X':
                    if (!g_searchText.empty() && OpenClipboard(hWnd))
                    {
                        EmptyClipboard();
                        size_t len = (g_searchText.length() + 1) * sizeof(wchar_t);
                        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
                        if (hMem)
                        {
                            memcpy(GlobalLock(hMem), g_searchText.c_str(), len);
                            GlobalUnlock(hMem);
                            SetClipboardData(CF_UNICODETEXT, hMem);
                        }
                        CloseClipboard();
                    }
                    g_searchText.clear();
                    g_vScrollPos = 0;
                    g_hScrollPos = 0;
                    g_selectedIndices.clear();
                    break;
                case 'A':
                    break;
                }
            }
            else if (wP == VK_DELETE && !g_searchText.empty())
            {
                g_searchText.pop_back();
                g_vScrollPos = 0;
                g_hScrollPos = 0;
                g_selectedIndices.clear();
            }
        }
        break;
    }
    case WM_CHAR:
        if (g_bLoginUI)
        {
            if (wP >= 32 && wP != '|')
            {
                if (g_bUserActive && g_wLoginUser.length() < 32)
                    g_wLoginUser += (wchar_t)wP;
                else if (g_bPassActive && g_wLoginPass.length() < 32)
                    g_wLoginPass += (wchar_t)wP;
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        else if (g_searchFocused)
        {
            if (wP == VK_BACK && !g_searchText.empty())
                g_searchText.pop_back();
            else if (wP == VK_ESCAPE)
                g_searchText.clear();
            else if (wP >= 32)
                g_searchText += (wchar_t)wP;
            g_vScrollPos = 0;
            g_hScrollPos = 0;
            g_selectedIndices.clear();
        }
        break;
    case WM_GETMINMAXINFO:
        if (g_bLoginUI)
            ((MINMAXINFO *)lP)->ptMinTrackSize = {LOGIN_WIDTH, LOGIN_HEIGHT};
        else
            ((MINMAXINFO *)lP)->ptMinTrackSize = {WINDOW_WIDTH, WINDOW_HEIGHT};
        break;
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

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow)
{
    WNDCLASSEXW wc = {sizeof(wc), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInst, LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1)), LoadCursor(NULL, IDC_ARROW), NULL, NULL, L"AzDMC", NULL};
    RegisterClassExW(&wc);
    g_hWnd = CreateWindowExW(0, L"AzDMC", L"Azrien Data Manager", WS_OVERLAPPEDWINDOW | WS_VISIBLE, (GetSystemMetrics(SM_CXSCREEN) - LOGIN_WIDTH) / 2, (GetSystemMetrics(SM_CYSCREEN) - LOGIN_HEIGHT) / 2, LOGIN_WIDTH, LOGIN_HEIGHT, NULL, NULL, hInst, NULL);
    InitD2D(g_hWnd);
    ShowWindow(g_hWnd, nShow);
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0))
    {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return (int)m.wParam;
}
