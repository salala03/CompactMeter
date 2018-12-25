﻿#include "stdafx.h"
#include "MeterDrawer.h"
#include "Worker.h"
#include "IniConfig.h"
#include "Logger.h"

extern int g_dpix;
extern int g_dpiy;
extern float g_dpiScale;
extern IniConfig* g_pIniConfig;

void MeterDrawer::Init(HWND hWnd, int width, int height) {

    //--------------------------------------------------
    // Direct2D
    //--------------------------------------------------

    // Create D2D factory
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

    //--------------------------------------------------
    // GDI+
    //--------------------------------------------------
    GdiplusStartupInput gdiSI;

    GdiplusStartup(&m_gdiToken, &gdiSI, NULL);

    // GDI+ OffScreen 作成, Direct2D 初期化
    Resize(hWnd, width, height);
}

void MeterDrawer::Resize(HWND hWnd, int width, int height) {

    //--------------------------------------------------
    // GDI+
    //--------------------------------------------------
    if (m_pOffScreenBitmap != NULL) {
        delete m_pOffScreenBitmap;
    }
    if (m_pOffScreenGraphics != NULL) {
        delete m_pOffScreenGraphics;
    }
    m_pOffScreenBitmap = new Bitmap(width, height);
    m_pOffScreenGraphics = new Graphics(m_pOffScreenBitmap);


    //--------------------------------------------------
    // Direct2D
    //--------------------------------------------------
    DiscardDeviceResources();
    CreateDeviceResources(hWnd, width, height);

}

void MeterDrawer::Shutdown() {

    GdiplusShutdown(m_gdiToken);
}

void MeterDrawer::DrawToDC(HDC hdc, HWND hWnd, CWorker * pWorker)
{
    HRESULT hr;
    RECT rc;

    GetClientRect(hWnd, &rc);

    // Create the DC render target.
    hr = CreateDeviceResources(hWnd, rc.right, rc.bottom);

    if (SUCCEEDED(hr))
    {
        if (g_pIniConfig->mDirect2DMode) {

            //--------------------------------------------------
            // 描画
            //--------------------------------------------------
            m_stopWatch1.Start();

            Draw(*m_pOffScreenGraphics, hWnd, pWorker);

            m_stopWatch1.Stop();

            //--------------------------------------------------
            // Direct2D に転送
            //--------------------------------------------------
            m_stopWatch2.Start();

            m_pRenderTarget->BeginDraw();

            HDC hdc;
            m_pInteropRenderTarget->GetDC(D2D1_DC_INITIALIZE_MODE_COPY, &hdc);

            Graphics onScreen(hdc);
            onScreen.DrawImage(m_pOffScreenBitmap, 0, 0);

            m_pInteropRenderTarget->ReleaseDC(&rc);
            m_pRenderTarget->EndDraw();

            m_stopWatch2.Stop();

        } else {

            //--------------------------------------------------
            // 描画
            //--------------------------------------------------
            m_stopWatch1.Start();

            Draw(*m_pOffScreenGraphics, hWnd, pWorker);

            m_stopWatch1.Stop();

            //--------------------------------------------------
            // 実画面に転送
            //--------------------------------------------------
            m_stopWatch2.Start();

            Graphics onScreen(hdc);
            onScreen.DrawImage(m_pOffScreenBitmap, 0, 0);

            m_stopWatch2.Stop();
        }

    }

    if (hr == D2DERR_RECREATE_TARGET)
    {
        hr = S_OK;

        Logger::d(L"D2DERR_RECREATE_TARGET");

        DiscardDeviceResources();
    }


}

HRESULT MeterDrawer::CreateDeviceResources(HWND hWnd, int width, int height)
{
    HRESULT hr = S_OK;

    if (!m_pRenderTarget)
    {
        FLOAT dpiX, dpiY;
        m_pD2DFactory->GetDesktopDpi(&dpiX, &dpiY);
        Logger::d(L"DPI: %.2f,%.2f", dpiX, dpiY);

        const D2D1_PIXEL_FORMAT format =
            D2D1::PixelFormat(
                DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED);

        const D2D1_RENDER_TARGET_PROPERTIES properties =
            D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                format,
                dpiX,
                dpiY,
                D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE);

        auto ScreenSize = D2D1::SizeU(width, height);

        HRESULT hr = m_pD2DFactory->CreateHwndRenderTarget(
            properties,
            D2D1::HwndRenderTargetProperties(hWnd, ScreenSize),
            &m_pRenderTarget
        );

        if (SUCCEEDED(hr)) {
            hr = m_pRenderTarget->QueryInterface(
                __uuidof(ID2D1GdiInteropRenderTarget),
                (void**)&m_pInteropRenderTarget
            );
        }

    }

    return hr;
}

void MeterDrawer::DiscardDeviceResources()
{
    SafeRelease(&m_pRenderTarget);
    SafeRelease(&m_pBlackBrush);
    SafeRelease(&m_pInteropRenderTarget);
}

void MeterDrawer::Draw(Graphics& g, HWND hWnd, CWorker* pWorker)
{
    //--------------------------------------------------
    // Draw
    //--------------------------------------------------
    SolidBrush backgroundBrush(Color(255, 10, 10, 10));
    float screenWidth = (float)g_pIniConfig->mWindowWidth;
    float screenHeight = (float)g_pIniConfig->mWindowHeight;
    Gdiplus::RectF rect = Gdiplus::RectF(0.0f, 0.0f, screenWidth, screenHeight);

    g.FillRectangle(&backgroundBrush, rect);

    if (pWorker != NULL && pWorker->traffics.size() >= 2) {

        pWorker->criticalSection.Lock();

        DrawMeters(g, hWnd, pWorker, screenWidth, screenHeight);

        pWorker->criticalSection.Unlock();
    }
}

void MeterDrawer::DrawMeters(Graphics& g, HWND hWnd, CWorker* pWorker, float screenWidth, float screenHeight)
{
    Gdiplus::RectF rect;

    CString str;
    RECT rectWindow;
    GetClientRect(hWnd, &rectWindow);
    // 各ウィジェットのサイズ(width, height)
    float size = rectWindow.right / 2.0f;


    //--------------------------------------------------
    // CPU+Memory タコメーター描画
    //--------------------------------------------------
    static MeterColor cpuColors[] = {
        { 90.0, Color(255, 64, 64) },
        { 80.0, Color(255, 128, 64) },
        { 70.0, Color(192, 192, 64) },
        {  0.0, Color(192, 192, 192) }
    };
    static MeterGuide cpuGuides[] = {
        { 100.0, Color(255,  64,  64), L"" },
        {  90.0, Color(255,  64,  64), L"" },
        {  80.0, Color(255,  64,  64), L"" },
        {  70.0, Color(255,  64,  64), L"" },
        {  60.0, Color(192, 192, 192), L"" },
        {  50.0, Color(192, 192, 192), L"" },
        {  40.0, Color(192, 192, 192), L"" },
        {  30.0, Color(192, 192, 192), L"" },
        {  20.0, Color(192, 192, 192), L"" },
        {  10.0, Color(192, 192, 192), L"" },
        {   0.0, Color(192, 192, 192), L"" },
    };

    CpuUsage cpuUsage;
    int nCore = pWorker->GetCpuUsage(&cpuUsage);

    float y = 0;
    float height = size;

    // 全コアの合計
    {
        rect = Gdiplus::RectF(0, 0, size, size);
        float percent = cpuUsage.usages[0];
        str.Format(L"CPU (%.0f%%)", percent);
        DrawMeter(g, rect, percent, str, cpuColors, cpuGuides, 1);
    }

    // メモリ使用量
    {
        MEMORYSTATUSEX ms;

        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);

        //printf("dwMemoryLoad     %d\n", ms.dwMemoryLoad);
        //printf("ullTotalPhys     %I64d\n", ms.ullTotalPhys);         // 物理メモリの搭載容量
        //printf("ullAvailPhys     %I64d\n", ms.ullAvailPhys);         // 物理メモリの空き容量
        //printf("ullTotalPageFile %I64d\n", ms.ullTotalPageFile);     // ページングの搭載容量
        //printf("ullAvailPageFile %I64d\n", ms.ullAvailPageFile);     // ページングの空き容量
        //printf("ullTotalVirtual  %I64d\n", ms.ullTotalVirtual);      // 仮想メモリの搭載容量
        //printf("ullAvailVirtual  %I64d\n", ms.ullAvailVirtual);      // 仮想メモリの空き容量

        rect = Gdiplus::RectF(size, 0, size, size);
        DWORDLONG ullUsing = ms.ullTotalPhys - ms.ullAvailPhys;
        float percent = ullUsing * 100.0f / ms.ullTotalPhys;
        str.Format(L"Memory (%.0f%%)\n%I64d / %I64d MB", percent, ullUsing / 1024 / 1024, ms.ullTotalPhys / 1024 / 1024);
        DrawMeter(g, rect, percent, str, cpuColors, cpuGuides, 1);
    }
    y += height;

    //--------------------------------------------------
    // 各Core
    //--------------------------------------------------
    if (g_pIniConfig->mShowCoreMeters) {
        int div = 4;
        float scale = 2.0f / div;    // 1.0 or 0.5
        float coreSize = size * scale;
        float x = 0;
        for (int i = 0; i < nCore; i++) {
            if (i % div == 0) {
                // 左側
                x = 0;
                if (i != 0) {
                    y += coreSize;
                }
                if (y + coreSize >= screenHeight) {
                    return;
                }

                rect = Gdiplus::RectF(x, y, coreSize, coreSize);
            }
            else {
                // 右側
                x += coreSize;
                rect = Gdiplus::RectF(x, y, coreSize, coreSize);
            }
            float percent = cpuUsage.usages[i + 1];
            //          str.Format(L"Core%d (%.0f%%)", i + 1, percent);
            str.Format(L"Core%d", i + 1);
            DrawMeter(g, rect, percent, str, cpuColors, cpuGuides, 1.4f);
        }
        y += coreSize;
    }
    if (y + size >= screenHeight) {
        return;
    }


    //--------------------------------------------------
    // Network タコメーター描画
    //--------------------------------------------------
    DWORD maxTrafficBytes = g_pIniConfig->mTrafficMax;
    float percent = 0.0f;

    const Traffic& t = pWorker->traffics.back();    // 一番新しいもの
    const Traffic& t0 = pWorker->traffics.front();  // 一番古いもの

    DWORD duration = t.tick - t0.tick;

    // duration が ms なので *1000 してから除算
    float inb = (t.in - t0.in) * 1000.0f / duration;
    float outb = (t.out - t0.out) * 1000.0f / duration;

    // KB単位
    maxTrafficBytes /= 1000;
    inb /= 1000;
    outb /= 1000;

    MeterColor netColors[] = {
        { KbToPercent(1000, maxTrafficBytes), Color(255,  64,  64) },
        { KbToPercent( 100, maxTrafficBytes), Color(255, 128,  64) },
        { KbToPercent(  10, maxTrafficBytes), Color(192, 192,  64) },
        {                                0.0, Color(192, 192, 192) }
    };
    MeterGuide netGuides[] = {
        { KbToPercent(1000000, maxTrafficBytes), Color(255,  64,  64), L"1G"   },
        { KbToPercent( 100000, maxTrafficBytes), Color(255,  64,  64), L"100M" },
        { KbToPercent(  10000, maxTrafficBytes), Color(255,  64,  64), L"10M"  },
        { KbToPercent(   1000, maxTrafficBytes), Color(255,  64,  64), L"1M"   },
        { KbToPercent(    100, maxTrafficBytes), Color(255, 128,  64), L"100K" },
        { KbToPercent(     10, maxTrafficBytes), Color(192, 192,  64), L"10K"  },
        {                                   0.0, Color(192, 192, 192), L""     },
    };

    // Up(KB単位)
    rect = Gdiplus::RectF(0, y, size, size);
    percent = outb == 0 ? 0.0f : KbToPercent(outb, maxTrafficBytes);
    percent = percent < 0.0f ? 0.0f : percent;
    str.Format(L"▲ %.1f KB/s", outb);
    DrawMeter(g, rect, percent, str, netColors, netGuides, 1);

    // Down(KB単位)
    rect = Gdiplus::RectF(size, y, size, size);
    percent = inb == 0 ? 0.0f : KbToPercent(inb, maxTrafficBytes);
    percent = percent < 0.0f ? 0.0f : percent;
    str.Format(L"▼ %.1f KB/s", inb);
    DrawMeter(g, rect, percent, str, netColors, netGuides, 1);
    y += height * 1;


    //--------------------------------------------------
    // デバッグ表示
    //--------------------------------------------------
    static int iCalled = 0;
    iCalled++;

    // FPS カウント
    m_fpsCounter.CountOnDraw();

    if (g_pIniConfig->mDebugMode) {
        Font fontTahoma(L"ＭＳゴシック", 19 / g_dpiScale * size / 300.0f);
        StringFormat format;
        format.SetAlignment(StringAlignmentNear);

        // 開始 Y 座標
        rect = Gdiplus::RectF(0, y, screenWidth, screenHeight - y);

        //str.Format(L"Up=%lld(%lld), Down=%lld(%lld)",
        //  t.out, t.out - t0.out,
        //  t.in, t.in - t0.in);
        //g.DrawString(str, str.GetLength(), &fontTahoma, rect, &format, &mainBrush);

        //str.Format(L"Up=%.0f[b/s], Down=%.0f[b/s], %ldms", outb, inb, duration);
        //rect.Offset(0, 30);
        //g.DrawString(str, str.GetLength(), &fontTahoma, rect, &format, &mainBrush);

        //str.Format(L"Up=%.1f[kb/s], Down=%.1f[kb/s]", outb / 1024, inb / 1024);
        //rect.Offset(0, 30);
        //g.DrawString(str, str.GetLength(), &fontTahoma, rect, &format, &mainBrush);

        SolidBrush mainBrush(Color(255, 192, 192, 192));

        CString strDateTime;
        SYSTEMTIME st;
        GetLocalTime(&st);
        strDateTime.Format(L"%d/%02d/%02d %d:%02d:%02d.%03d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        // デバッグ用計測器
        DWORD duration1 = m_stopWatch1.GetAverageDurationMicroseconds();
        DWORD duration2 = m_stopWatch2.GetAverageDurationMicroseconds();
        DWORD durationWorker = pWorker->m_stopWatch.GetAverageDurationMicroseconds();

        str.Format(L"i=%d, FPS=%d/%d, "
                   L"n=%d size=%dx%d \n"
                   L"%s %.0f DPI=%d,%d(%.2f)\n"
                   L"描画=%5.1fms\n転送=%5.1fms\n計測=%5.1fms\n"
                   L"RenderMode=%s\n"
            ,
            iCalled, m_fpsCounter.GetAverageFps(), g_pIniConfig->mFps,
            pWorker->traffics.size(), rectWindow.right, rectWindow.bottom,
            (LPCTSTR)strDateTime, size,
            g_dpix, g_dpiy, g_dpiScale,
            duration1/1000.0, duration2/1000.0, durationWorker/1000.0,
            g_pIniConfig->mDirect2DMode ? L"Direct2D" : L"GDI"
        );
        g.DrawString(str, str.GetLength(), &fontTahoma, rect, &format, &mainBrush);
    }
}

/**
 * メーターを描画する
 *
 * colors, guideLines の最後は必ず percent=0.0 にすること
 */
void MeterDrawer::DrawMeter(Graphics& g, Gdiplus::RectF& rect, float percent, const WCHAR* str, MeterColor colors[], MeterGuide guideLines[], float fontScale)
{
    auto size = rect.Width;

    if (percent < 0.0f) {
        percent = 0.0f;
    }
    else if (percent > 100.0f) {
        percent = 100.0f;
    }

    Color color;
    for (int i = 0; ; i++) {
        if (percent >= colors[i].percent) {
            color = Color(colors[i].color);
            break;
        }

        if (colors[i].percent == 0.0f) {
            break;
        }
    }

    //--------------------------------------------------
    // 枠線
    //--------------------------------------------------
    if (g_pIniConfig->mDrawBorder) {

        Pen pen1(Color(64, 64, 64), 1);
        g.DrawRectangle(&pen1, rect);
    }

    float margin = size / 50;
    rect.Offset(margin, margin);
    rect.Width -= margin * 2;
    rect.Height -= margin * 2;

    // ペン
    Pen p(color, 1.2f);
    SolidBrush mainBrush(color);

    //--------------------------------------------------
    // ラベル
    //--------------------------------------------------
    float scale = 1 / g_dpiScale * size / 300.0f * fontScale;
    Font fontTahoma(L"Tahoma", 19.0f * scale);
    StringFormat format;
    format.SetAlignment(StringAlignmentNear);
    g.DrawString(str, (int)wcslen(str), &fontTahoma, rect, &format, &mainBrush);
    rect.Offset(0, size / 5.0f);

    //--------------------------------------------------
    // メーター描画
    //--------------------------------------------------

    // アンチエイリアス
//  g.SetSmoothingMode(SmoothingModeNone);
    g.SetSmoothingMode(SmoothingModeHighQuality);

    // 左下、右下の角度
    float PMIN = -30;
    float PMAX = 180 - PMIN;

    // 外枠
    Gdiplus::PointF center(rect.X + rect.Width / 2, rect.Y + rect.Height / 2);
    g.DrawArc(&p, rect, -180 + PMIN, PMAX - PMIN);

    float length0 = rect.Width / 2;

    // 真ん中から左下へ。
    DrawLineByAngle(g, &p, center, PMIN, 0, length0);

    // 真ん中から右下へ。
    DrawLineByAngle(g, &p, center, PMAX, 0, length0);

    // 凡例の線
    p.SetWidth(1.9f * scale);
    Font font(L"Tahoma", 11.5f * scale);
    StringFormat format1;
    format1.SetAlignment(StringAlignmentCenter);
    format1.SetLineAlignment(StringAlignmentCenter);
    for (int i = 0; guideLines[i].percent != 0.0f; i++) {

        if (guideLines[i].percent > 100) {
            continue;
        }

        p.SetColor(guideLines[i].color);

        float angle = guideLines[i].percent / 100.0f * (PMAX - PMIN) + PMIN;
        DrawLineByAngle(g, &p, center, angle, length0 * 0.85f, length0);

        LPCWSTR text = guideLines[i].text;
        if (wcslen(text) >= 1) {
            float rad = PI * angle / 180;
            float w = length0 / 3;
            float h = length0 / 5;
            float length = length0 * 0.72f;
            Gdiplus::RectF rect1(center.X - length * cosf(rad), center.Y - length * sinf(rad), w, h);
            rect1.Offset(-w / 2, -h / 2);
            SolidBrush fontBrush(guideLines[i].color);
            g.DrawString(text, (int)wcslen(text), &font, rect1, &format1, &fontBrush);
//            g.DrawRectangle(&p, rect1);
        }
    }


    // 針を描く
    p.SetColor(color);
    p.SetWidth(5 * scale);
    DrawLineByAngle(g, &p, center, percent / 100.0f * (PMAX - PMIN) + PMIN, 0, length0 * 0.9f);
}
