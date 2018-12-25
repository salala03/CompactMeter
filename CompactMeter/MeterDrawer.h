﻿#pragma once

#include "Worker.h"
#include "Const.h"
#include "FpsCounter.h"
#include "StopWatch.h"

using namespace Gdiplus;

struct MeterColor {
    float percent;
    Color color;
};
struct MeterGuide {
    float percent;
    Color color;
    LPCWSTR text;
};

class MeterDrawer
{
public:
    ULONG_PTR   m_gdiToken;
    Bitmap*     m_pOffScreenBitmap;
    Graphics*   m_pOffScreenGraphics;
    FpsCounter  m_fpsCounter;

    // デバッグ用計測器
    StopWatch   m_stopWatch1;
    StopWatch   m_stopWatch2;

public:
    MeterDrawer()
        : m_gdiToken(NULL), m_pOffScreenBitmap(NULL), m_pOffScreenGraphics(NULL) {
    }

    ~MeterDrawer();

    void Init(int width, int height);
    void Resize(int width, int height);
    void Shutdown();

    void Draw(HWND hWnd, CWorker* pWorker);


private:
    void DrawMeters(Graphics& g, HWND hWnd, CWorker* pWorker, float screenWidth, float screenHeight);
    void DrawMeter(Graphics& g, Gdiplus::RectF& rect, float percent, const WCHAR* str, MeterColor colors[], MeterGuide guideLines[], float fontScale);

    inline float KbToPercent(float outb, const DWORD &maxTrafficBytes)
    {
        return (log10f((float)outb) / log10f((float)maxTrafficBytes))*100.0f;
    }

    inline void DrawLineByAngle(Graphics& g, Pen* p, Gdiplus::PointF& center, float angle, float length1, float length2)
    {
        float rad = PI * angle / 180;
        float c1 = cosf(rad);
        float s1 = sinf(rad);
        g.DrawLine(p,
            center.X - (length1 == 0 ? 0 : length1 * c1), center.Y - (length1 == 0 ? 0 : length1 * s1),
            center.X - length2 * c1, center.Y - length2 * s1
        );
    }

};
