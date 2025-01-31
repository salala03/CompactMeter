﻿#pragma once

#include "stdafx.h"
#include "StopWatch.h"

struct Traffic {
    ULONGLONG in;
    ULONGLONG out;
    DWORD tick;
};

struct CpuUsage {
    // index=0 が all
    // index=1 以降が各コアのCPU使用率
    std::vector<float> usages;
};

struct DriveUsage {
    std::vector<char> letters;   // 'C' など
    std::vector<long> readUsages;
    std::vector<long> writeUsages;
};

struct GpuUsage {
    double usage;
};

struct GpuMemory {
    double usage;
};


class CWorker
{
public:
    CComCriticalSection criticalSection;

    // Network
    std::list<Traffic> traffics;

    // CPU使用率
    std::list<CpuUsage> cpuUsages;

    // Drive使用率
    std::list<DriveUsage> driveUsages;

    std::list<GpuUsage> gpuUsages;

    std::list<GpuMemory> gpuMemorys;

    StopWatch m_stopWatch;


public:
    CWorker();
    ~CWorker();

    void SetParams(HWND hWnd);
    static DWORD WINAPI ThreadFunc(LPVOID lpParameter);
    void Terminate();

    int GetCpuUsage(CpuUsage* out);
    void GetDriveUsages(DriveUsage* out);

    int GetGpuUsage(GpuUsage* out);

    int GetGpuMemory(GpuMemory* out);

private:
    HWND hWnd = nullptr;


    bool myExitFlag; // 終了指示を保持するフラグ 
    HANDLE myMutex;  // 排他制御

    DWORD WINAPI ExecThread();
    static bool InitProcessors(std::vector<PDH_HQUERY> &hQuery, const int &nProcessors, std::vector<PDH_HQUERY> &hCounter);
    static bool InitDrives(std::vector<PDH_HQUERY> &hDriveWriteQuery, std::vector<PDH_HQUERY> &hDriveWriteCounter, std::vector<PDH_HQUERY> &hDriveReadQuery, std::vector<PDH_HQUERY> &hDriveReadCounter, int &nDrives);
    void CollectCpuUsage(const int &nProcessors, std::vector<PDH_HQUERY> &hQuery, std::vector<PDH_HQUERY> &hCounter, PDH_FMT_COUNTERVALUE &fntValue);
    void CollectDriveUsage(int nDrives, std::vector<PDH_HQUERY> &hDriveReadQuery, std::vector<PDH_HCOUNTER> &hDriveReadCounter,
        PDH_FMT_COUNTERVALUE &fntValue, std::vector<PDH_HQUERY> &hDriveWriteQuery, std::vector<PDH_HCOUNTER> &hDriveWriteCounter);
    void CollectTraffic();
    void CollectGpuUsage();
};

