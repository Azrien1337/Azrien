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

#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

using namespace Microsoft::WRL;

const std::string GITEE_OWNER = "名字";
const std::string GITEE_REPO = "库名";
const std::string GITEE_TOKEN = "令牌";

#define WINDOW_WIDTH 920
#define WINDOW_HEIGHT 700

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
int g_vScrollPos = 0;
int g_hScrollPos = 0;
int g_hoverItem = -1;
bool g_searchFocused = false;
int g_cursorBlink = 0;
bool g_vScrollbarDragging = false;
bool g_hScrollbarDragging = false;

bool g_bFilterTodesk = false;
bool g_bFilterBaidu = false;
bool g_bFilterBilibili = false;
bool g_bTodeskHover = false, g_bBaiduHover = false, g_bBilibiliHover = false;
float g_fTodeskAlpha = 0.3f, g_fTodeskTarget = 0.3f;
float g_fBaiduAlpha = 0.3f, g_fBaiduTarget = 0.3f;
float g_fBilibiliAlpha = 0.3f, g_fBilibiliTarget = 0.3f;

bool g_bFilterAnimating = false;
DWORD g_dwFilterAnimStart = 0;
float g_fFilterAnimProgress = 0.0f;

bool g_bLoading = false;
float g_fProgress = 0.0f;
int g_nTotalFiles = 0;
int g_nLoadedFiles = 0;
int g_nDuplicates = 0;
std::wstring g_wStatusText = L"";

RECT g_rcSearchBox, g_rcSearchBtn;
RECT g_rcFilterTodesk, g_rcFilterBaidu, g_rcFilterBilibili;
RECT g_rcList;
RECT g_rcExportBtn, g_rcRefreshBtn, g_rcExportAllBtn;
bool g_bSearchHover = false, g_bExportHover = false, g_bRefreshHover = false, g_bExportAllHover = false;
bool g_bSearchPress = false, g_bExportPress = false, g_bRefreshPress = false, g_bExportAllPress = false;
float g_fSearchBtnHover = 1.0f, g_fSearchBtnTarget = 1.0f;
float g_fExportHover = 1.0f, g_fExportTarget = 1.0f;
float g_fRefreshHover = 1.0f, g_fRefreshTarget = 1.0f;
float g_fExportAllHover = 1.0f, g_fExportAllTarget = 1.0f;

struct LoadState
{
    std::vector<std::pair<int, std::wstring>> tempData;
    std::vector<std::string> fileNames;
    size_t currentIndex;
    int totalFiles;
    int duplicates;
    bool scanningDir;
    std::string dirContent;
    size_t parsePos;
};
LoadState g_loadState;

bool HasMarker(const std::wstring &data, const std::wstring &marker)
{
    std::wstring lower = data;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    std::wstring m = marker;
    std::transform(m.begin(), m.end(), m.begin(), ::towlower);
    return lower.find(m) != std::wstring::npos;
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
        std::sort(g_loadState.tempData.begin(), g_loadState.tempData.end(), [](auto &a, auto &b)
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

std::vector<std::pair<int, std::wstring>> GetFilteredWithIndex() {
    std::vector<std::pair<int, std::wstring>> f;
    std::wstring kw = g_searchText;
    std::transform(kw.begin(), kw.end(), kw.begin(), ::towlower);
    for (size_t i = 0; i < g_allData.size(); i++) {
        if (!g_searchText.empty()) {
            std::wstring lower = g_allData[i];
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            if (lower.find(kw) == std::wstring::npos) continue;
        }
        
        // ToDesk filter - keep only if "ToDesk:" has content after it
        if (g_bFilterTodesk) {
            std::wstring s = g_allData[i];
            size_t p = s.find(L"ToDesk:");
            if (p == std::wstring::npos) continue;
            p += 7; // skip "ToDesk:"
            while (p < s.length() && (s[p] == L' ' || s[p] == L'\t')) p++;
            if (p >= s.length() || s[p] == L'\r' || s[p] == L'\n') continue;
        }
        
        // BaiduNetdisk filter
        if (g_bFilterBaidu) {
            std::wstring s = g_allData[i];
            size_t p = s.find(L"BaiduNetdisk:");
            if (p == std::wstring::npos) continue;
            p += 13; // skip "BaiduNetdisk:"
            while (p < s.length() && (s[p] == L' ' || s[p] == L'\t')) p++;
            if (p >= s.length() || s[p] == L'\r' || s[p] == L'\n') continue;
        }
        
        // Bilibili filter
        if (g_bFilterBilibili) {
            std::wstring s = g_allData[i];
            size_t p = s.find(L"Bilibili:");
            if (p == std::wstring::npos) continue;
            p += 9; // skip "Bilibili:"
            while (p < s.length() && (s[p] == L' ' || s[p] == L'\t')) p++;
            if (p >= s.length() || s[p] == L'\r' || s[p] == L'\n') continue;
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

bool InitD2D(HWND hWnd)
{
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pF);
    RECT rc;
    GetClientRect(hWnd, &rc);
    if (FAILED(g_pF->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hWnd, D2D1::SizeU(rc.right, rc.bottom)), &g_pRT)))
        return false;
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &g_pWB);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&g_pDW);
    g_pDW->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"", &g_pSF);
    g_pDW->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"", &g_pBF);
    g_pSF->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return true;
}

void Cleanup()
{
    if (g_pSF)
        g_pSF->Release();
    if (g_pBF)
        g_pBF->Release();
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

float EaseOutCubic(float t) { return 1.0f - powf(1.0f - t, 3.0f); }

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
    const float padding = 20, sbSize = 14, itemH = 36.0f;

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

    g_pRT->EndDraw();
    EndPaint(g_hWnd, &ps);
}

bool IsOnVScrollbar(int x, int y)
{
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
        break;
    case WM_TIMER:
        g_cursorBlink++;
        g_fSearchBtnHover += (g_fSearchBtnTarget - g_fSearchBtnHover) * 0.18f;
        g_fExportHover += (g_fExportTarget - g_fExportHover) * 0.18f;
        g_fRefreshHover += (g_fRefreshTarget - g_fRefreshHover) * 0.18f;
        g_fExportAllHover += (g_fExportAllTarget - g_fExportAllHover) * 0.18f;
        g_fTodeskAlpha += (g_fTodeskTarget - g_fTodeskAlpha) * 0.2f;
        g_fBaiduAlpha += (g_fBaiduTarget - g_fBaiduAlpha) * 0.2f;
        g_fBilibiliAlpha += (g_fBilibiliTarget - g_fBilibiliAlpha) * 0.2f;
        if (g_bLoading)
            ProcessNextFile();
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
        if (g_vScrollbarDragging)
        {
            RECT cr;
            GetClientRect(hWnd, &cr);
            float W = (float)cr.right, H = (float)cr.bottom;
            float padding = 20, sbSize = 14, itemH = 36.0f;
            float filterY = 60 + 30 + 8 + 28 + 10;
            float listH = H - filterY - 80;
            float sbH = listH - sbSize - 4, viewportH = listH - sbSize - 4;
            auto filtered = GetFilteredWithIndex();
            float totalContentH = (float)filtered.size() * itemH + 8.0f;
            int maxVScroll = (int)(totalContentH - viewportH);
            if (maxVScroll < 0)
                maxVScroll = 0;
            float thumbH = (viewportH / totalContentH) * sbH;
            if (thumbH < 30)
                thumbH = 30;
            float ratio = (float)(y - filterY - 2 - thumbH / 2) / (sbH - thumbH);
            g_vScrollPos = (int)(ratio * maxVScroll);
            if (g_vScrollPos < 0)
                g_vScrollPos = 0;
            if (g_vScrollPos > maxVScroll)
                g_vScrollPos = maxVScroll;
            break;
        }
        if (g_hScrollbarDragging)
        {
            RECT cr;
            GetClientRect(hWnd, &cr);
            float W = (float)cr.right, H = (float)cr.bottom;
            float padding = 20, sbSize = 14, filterY = 60 + 30 + 8 + 28 + 10;
            float listH = H - filterY - 80;
            float sbW = W - padding * 2 - sbSize - 4;
            auto filtered = GetFilteredWithIndex();
            float maxTextWidth = 0;
            for (auto &f : filtered)
            {
                std::wstring line = L"[" + std::to_wstring(g_fileNums[f.first]) + L".txt] " + f.second;
                std::replace(line.begin(), line.end(), L'\n', L' ');
                std::replace(line.begin(), line.end(), L'\r', L' ');
                float tw = CalcTextWidth(line);
                if (tw > maxTextWidth)
                    maxTextWidth = tw;
            }
            float totalContentW = maxTextWidth + 30.0f, viewportW = W - padding * 2 - sbSize - 4;
            int maxHScroll = (int)(totalContentW - viewportW);
            if (maxHScroll < 0)
                maxHScroll = 0;
            float thumbW = (viewportW / totalContentW) * sbW;
            if (thumbW < 40)
                thumbW = 40;
            float ratio = (float)(x - padding - 2 - thumbW / 2) / (sbW - thumbW);
            g_hScrollPos = (int)(ratio * maxHScroll);
            if (g_hScrollPos < 0)
                g_hScrollPos = 0;
            if (g_hScrollPos > maxHScroll)
                g_hScrollPos = maxHScroll;
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
        float filterY = 60 + 30 + 8 + 28 + 10;
        if (x >= g_rcList.left + 4 && x <= g_rcList.right - 18 && y >= filterY + 4 && y <= g_rcList.bottom - 18 && !g_bLoading && !g_allData.empty())
        {
            g_hoverItem = (int)((y - filterY - 4 + g_vScrollPos) / 36);
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
            float filterY = 60 + 30 + 8 + 28 + 10;
            if (x >= g_rcList.left + 4 && x <= g_rcList.right - 18 && y >= filterY + 4 && y <= g_rcList.bottom - 18)
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
        if (g_bLoading || g_allData.empty())
            break;
        int delta = GET_WHEEL_DELTA_WPARAM(wP);
        if (GetKeyState(VK_SHIFT) & 0x8000)
        {
            g_hScrollPos -= delta / 3;
            if (g_hScrollPos < 0)
                g_hScrollPos = 0;
            RECT cr;
            GetClientRect(hWnd, &cr);
            float W = (float)cr.right, padding = 20, sbSize = 14;
            auto filtered = GetFilteredWithIndex();
            float maxTextWidth = 0;
            for (auto &f : filtered)
            {
                std::wstring line = L"[" + std::to_wstring(g_fileNums[f.first]) + L".txt] " + f.second;
                std::replace(line.begin(), line.end(), L'\n', L' ');
                std::replace(line.begin(), line.end(), L'\r', L' ');
                float tw = CalcTextWidth(line);
                if (tw > maxTextWidth)
                    maxTextWidth = tw;
            }
            int maxHScroll = (int)(maxTextWidth + 30.0f - (W - padding * 2 - sbSize - 4));
            if (maxHScroll < 0)
                maxHScroll = 0;
            if (g_hScrollPos > maxHScroll)
                g_hScrollPos = maxHScroll;
        }
        else
        {
            g_vScrollPos -= delta / 3;
            if (g_vScrollPos < 0)
                g_vScrollPos = 0;
            RECT cr;
            GetClientRect(hWnd, &cr);
            float H = (float)cr.bottom, padding = 20, sbSize = 14, itemH = 36.0f;
            float filterY = 60 + 30 + 8 + 28 + 10;
            float listH = H - filterY - 80;
            auto filtered = GetFilteredWithIndex();
            int maxVScroll = (int)((float)filtered.size() * itemH + 8.0f - (listH - sbSize - 4));
            if (maxVScroll < 0)
                maxVScroll = 0;
            if (g_vScrollPos > maxVScroll)
                g_vScrollPos = maxVScroll;
        }
        break;
    }
    case WM_CHAR:
        if (wP == VK_BACK && !g_searchText.empty())
            g_searchText.pop_back();
        else if (wP == VK_ESCAPE)
            g_searchText.clear();
        else if (wP >= 32)
            g_searchText += (wchar_t)wP;
        g_vScrollPos = 0;
        g_hScrollPos = 0;
        g_selectedIndices.clear();
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
    WNDCLASSEXW wc = {sizeof(wc), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInst, NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, L"AzDMC", NULL};
    RegisterClassExW(&wc);
    g_hWnd = CreateWindowExW(0, L"AzDMC", L"Azrien Data Manager", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             (GetSystemMetrics(SM_CXSCREEN) - WINDOW_WIDTH) / 2, (GetSystemMetrics(SM_CYSCREEN) - WINDOW_HEIGHT) / 2,
                             WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, hInst, NULL);
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
