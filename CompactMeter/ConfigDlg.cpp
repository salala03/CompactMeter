#include "stdafx.h"
#include "ConfigDlg.h"
#include "Logger.h"
#include "resource.h"
#include "IniConfig.h"
#include "MeterDrawer.h"
#include "MyUtil.h"
#include "..\StartupRegister\StartupRegisterConst.h"

extern HWND        g_hConfigDlgWnd;
extern IniConfig*  g_pIniConfig;
extern HWND        g_hWnd;
extern WCHAR       g_szAppTitle[MAX_LOADSTRING];
extern MeterDrawer g_meterDrawer;

struct TRAFFIC_MAX_COMBO_DATA {
    LPCWSTR label;
    long value;
};
TRAFFIC_MAX_COMBO_DATA TRAFFIC_MAX_COMBO_VALUES[] = {
    { L"10MB", 10 * MB },
    { L"100MB", 100 * MB},
    { L"300MB", 300 * MB},
    { L"1GB", 1 * GB},
    { L"2GB", 2 * GB},
    { NULL, 0}
};

static bool initializing = false;

//--------------------------------------------------
// プロトタイプ宣言(ローカル)
//--------------------------------------------------
void DoChooseColor(HWND hDlg);
void UpdateRegisterButtons(const HWND &hDlg);
void RegisterStartup(bool doRegister, HWND hDlg);
bool GetStartupRegValue(CString& strRegValue);
int  GetSelectedMeterConfigListIndex(HWND hList);
void MoveMeterPos(const HWND &hDlg, bool moveToUp);
void SwapMeterItem(HWND hList, int iTarget1, int iTarget2);
void SetMeterConfigListViewBackgroundColor(HWND hList, COLORREF backgroundColor, int index);

INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
    {
        Logger::d(L"ConfigDlg, init");

        //--------------------------------------------------
        // 初期値設定
        //--------------------------------------------------
        initializing = true;

        // FPS
        SetDlgItemInt(hDlg, IDC_FPS_EDIT, g_pIniConfig->mFps, FALSE);

        // メーターの列数
        SetDlgItemInt(hDlg, IDC_METER_COLUMN_COUNT_EDIT, g_pIniConfig->mColumnCount, FALSE);

        // 最大通信量
        HWND hTraffixMaxCombo = GetDlgItem(hDlg, IDC_TRAFFIC_MAX_COMBO);
        int iSelected = 0;
        for (int i = 0; TRAFFIC_MAX_COMBO_VALUES[i].label != NULL; i++) {
            SendMessage(hTraffixMaxCombo, CB_ADDSTRING, NULL, (WPARAM)TRAFFIC_MAX_COMBO_VALUES[i].label);

            if (g_pIniConfig->mTrafficMax == TRAFFIC_MAX_COMBO_VALUES[i].value) {
                iSelected = i;
            }
        }
        SendMessage(hTraffixMaxCombo, CB_SETCURSEL, iSelected, 0);

        // メーター設定(リストビュー)
        {
            HWND hList = GetDlgItem(hDlg, IDC_METER_CONFIG_LIST);

            // 拡張スタイル設定
            DWORD dwStyle = ListView_GetExtendedListViewStyle(hList);
            dwStyle |= LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES;
            ListView_SetExtendedListViewStyle(hList, dwStyle);

            // カラム設定
            LVCOLUMN lvc;
            lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
            lvc.fmt = LVCFMT_LEFT;

            LPCWSTR strItem0[] = { L"メーター名", L"背景色", NULL };
            int CX[] = { 220, 100 };

            for (int i = 0; strItem0[i] != NULL; i++)
            {
                lvc.iSubItem = i;
                lvc.cx = CX[i];

                WCHAR szText[256];
                lvc.pszText = szText;
                wcscpy_s(szText, strItem0[i]);

                ListView_InsertColumn(hList, i, &lvc);
            }

            // メーター項目追加
            LVITEM item;
            item.mask = LVIF_TEXT;
            for (size_t i = 0; i < g_pIniConfig->mMeterConfigs.size(); i++)
            {
                const auto& mc = g_pIniConfig->mMeterConfigs[i];

                WCHAR szText[256];
                item.pszText = szText;
                wcscpy_s(szText, mc.getName());
                item.iItem = (int)i;
                item.iSubItem = 0;
                ListView_InsertItem(hList, &item);

                // 背景色
                SetMeterConfigListViewBackgroundColor(hList, mc.backgroundColor, i);
                
                // チェック状態反映
                ListView_SetCheckState(hList, i, mc.enable ? TRUE : FALSE);

                // 最初の項目を選択状態にする
                if (i == 0) {
                    ListView_SetItemState(hList, i, LVIS_SELECTED, LVIS_SELECTED);
                }
            }

        }

        //--------------------------------------------------
        // スタートアップ登録
        //--------------------------------------------------
        // 登録、解除ボタンの有効状態を更新する
        UpdateRegisterButtons(hDlg);

        initializing = false;

        return (INT_PTR)TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        case IDCANCEL:
            Logger::d(L"ConfigDlg, close");
            EndDialog(hDlg, LOWORD(wParam));
            g_hConfigDlgWnd = NULL;
            return (INT_PTR)TRUE;

        case IDC_TRAFFIC_MAX_COMBO:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                HWND hTraffixMaxCombo = GetDlgItem(hDlg, IDC_TRAFFIC_MAX_COMBO);
                int iSelected = (int)SendMessage(hTraffixMaxCombo, CB_GETCURSEL, 0, 0);

                g_pIniConfig->mTrafficMax = TRAFFIC_MAX_COMBO_VALUES[iSelected].value;

                Logger::d(L"selchange %d -> %d", iSelected, g_pIniConfig->mTrafficMax);

                g_pIniConfig->Save();

                // UIに反映するために変更通知
                ::PostMessage(g_hWnd, WM_CONFIG_DLG_UPDATED, 0, 0);
            }
            return (INT_PTR)TRUE;

        //case IDC_SHOW_CORE_METER_CHECK:
        //    if (HIWORD(wParam) == BN_CLICKED) {
        //        g_pIniConfig->mShowCoreMeters = IsDlgButtonChecked(hDlg, IDC_SHOW_CORE_METER_CHECK) == BST_CHECKED;
        //        g_pIniConfig->Save();
        //    }
        //    return (INT_PTR)TRUE;

        case IDC_MOVE_UP_BUTTON:
            Logger::d(L"up");
            MoveMeterPos(hDlg, true);
            return (INT_PTR)TRUE;

        case IDC_MOVE_DOWN_BUTTON:
            Logger::d(L"down");
            MoveMeterPos(hDlg, false);
            return (INT_PTR)TRUE;

        case IDC_CHOOSE_COLOR_BUTTON:
            DoChooseColor(hDlg);
            return (INT_PTR)TRUE;

        case IDC_REGISTER_STARTUP_BUTTON:
            RegisterStartup(true, hDlg);
            return (INT_PTR)TRUE;

        case IDC_UNREGISTER_STARTUP_BUTTON:
            RegisterStartup(false, hDlg);
            return (INT_PTR)TRUE;
        }
        break;

    case WM_NOTIFY:
        switch (wParam) {
        case IDC_FPS_SPIN:
            {
                LPNMUPDOWN lpnmUpdown = (LPNMUPDOWN)lParam;
                if (lpnmUpdown->hdr.code == UDN_DELTAPOS) {

                    int n = GetDlgItemInt(hDlg, IDC_FPS_EDIT, NULL, FALSE);

                    if (lpnmUpdown->iDelta > 0) {
                        Logger::d(L"down");
                        n--;
                    }
                    else {
                        Logger::d(L"up");
                        n++;
                    }

                    g_pIniConfig->mFps = n;
                    g_pIniConfig->NormalizeFps();

                    SetDlgItemInt(hDlg, IDC_FPS_EDIT, g_pIniConfig->mFps, FALSE);
                    g_pIniConfig->Save();
                }
            }
            break;

        case IDC_METER_COLUMN_COUNT_SPIN:
            {
                LPNMUPDOWN lpnmUpdown = (LPNMUPDOWN)lParam;
                if (lpnmUpdown->hdr.code == UDN_DELTAPOS) {

                    const int n0 = GetDlgItemInt(hDlg, IDC_METER_COLUMN_COUNT_EDIT, NULL, FALSE);
                    int n = n0;

                    if (lpnmUpdown->iDelta > 0) {
                        n--;
                    } else {
                        n++;
                    }

                    // 描画中にサイズ変更すると画面が乱れるためロックする
                    g_meterDrawer.criticalSection.Lock();

                    g_pIniConfig->mColumnCount = n;
                    g_pIniConfig->NormalizeColumnCount();
                    n = g_pIniConfig->mColumnCount;

                    Logger::d(L"column count : %d -> %d", n0, n);

                    SetDlgItemInt( hDlg, IDC_METER_COLUMN_COUNT_EDIT, g_pIniConfig->mColumnCount, FALSE );
                    g_pIniConfig->Save();

                    // メーターサイズを基準にしてウィンドウサイズを変更する
                    {
                        const int box = g_pIniConfig->mWindowWidth / n0;
                        const int w = __max(box * n, MAIN_WINDOW_MIN_WIDTH);
                        const int h = __max(g_pIniConfig->mWindowHeight - (n - n0) * box, MAIN_WINDOW_MIN_HEIGHT);
                        ::SetWindowPos(g_hWnd, NULL, 0, 0, w, h, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);

                        // g_pIniConfig->mWindowWidth 等の更新、Direct2D バッファのリサイズは WM_SIZE 内で行われる

                        Logger::d(L"window size updated");
                    }

                    g_meterDrawer.criticalSection.Unlock();
                }
            }
            break;

        case IDC_METER_CONFIG_LIST:
            {
                LPNMHDR lpNMHDR = (LPNMHDR)lParam;

                switch (lpNMHDR->code) {
                case LVN_ITEMCHANGED:

                    if (initializing) break;
                    {
                        HWND hList = GetDlgItem(hDlg, IDC_METER_CONFIG_LIST);

                        LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                        Logger::d(L"item changed %d:%d", pnmv->iItem, pnmv->iSubItem);

                        auto& configs = g_pIniConfig->mMeterConfigs;
                        if (0 <= pnmv->iItem && pnmv->iItem < (int)configs.size() && pnmv->iSubItem == 0) {
                            // チェック状態が変わったかもしれないので反映する
                            configs[pnmv->iItem].enable = ListView_GetCheckState(hList, pnmv->iItem) == TRUE;
                        }

                        g_pIniConfig->Save();
                    }
                    break;
                }

            }
            return (INT_PTR)TRUE;
        }
        break;

    case WM_MOVE:
        {
            // 移動 => INIに位置を保存
            RECT rect;
            GetWindowRect(hDlg, &rect);
            g_pIniConfig->mConfigDlgPosX = rect.left;
            g_pIniConfig->mConfigDlgPosY = rect.top;
            g_pIniConfig->Save();
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void DoChooseColor(HWND hDlg)
{
    HWND hList = GetDlgItem(hDlg, IDC_METER_CONFIG_LIST);

    int iSelected = GetSelectedMeterConfigListIndex(hList);
    if (iSelected == -1) {
        Logger::d(L"no selection");
        return;
    }

    if (iSelected >= (int)g_pIniConfig->mMeterConfigs.size()) {
        Logger::d(L"invalid index %d", iSelected);
        return;
    }

    CHOOSECOLOR cc = { 0 };
    static COLORREF CustColors[16] = {
        RGB(0x00, 0x00, 0x10), RGB(0x00, 0x10, 0x00), RGB(0x10, 0x00, 0x00),
        RGB(0x00, 0x00, 0x20), RGB(0x00, 0x20, 0x00), RGB(0x20, 0x00, 0x00),
        RGB(0x00, 0x10, 0x10), RGB(0x10, 0x10, 0x00), RGB(0x10, 0x00, 0x10),
        RGB(0x00, 0x20, 0x20), RGB(0x20, 0x20, 0x00), RGB(0x20, 0x00, 0x20),
        RGB(0x10, 0x20, 0x20), RGB(0x20, 0x20, 0x10), RGB(0x20, 0x10, 0x20),
    };

    cc.lStructSize = sizeof(CHOOSECOLOR);
    cc.hwndOwner = hDlg;

    // Direct2D カラー -> RGB に変更
    COLORREF c = g_pIniConfig->mMeterConfigs[iSelected].backgroundColor;
    cc.rgbResult = MyUtil::direct2DColorToRGB(c);
    Logger::d(L"color: %08x -> %08x", c, cc.rgbResult);

    cc.lpCustColors = CustColors;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    // カラー選択
    if (!ChooseColor(&cc)) {
        Logger::d(L"cancel");
        return;
    }

    // RGB -> Direct2D カラーに変更
    c = cc.rgbResult;
    c = (GetRValue(c) << 16U) | (GetGValue(c) << 8U) | GetBValue(c);
    g_pIniConfig->mMeterConfigs[iSelected].backgroundColor = c;
    Logger::d(L"selected color: %08x -> %08x", cc.rgbResult, c);

    // 保存
    g_pIniConfig->Save();

    // リストに反映
    SetMeterConfigListViewBackgroundColor(hList, c, iSelected);
}

void UpdateRegisterButtons(const HWND &hDlg)
{
    // 盾アイコン
    HWND hRegisterButton = ::GetDlgItem(hDlg, IDC_REGISTER_STARTUP_BUTTON);
    HWND hUnregisterButton = ::GetDlgItem(hDlg, IDC_UNREGISTER_STARTUP_BUTTON);
    ::SendMessage(hRegisterButton, BCM_SETSHIELD, 0, 1);
    ::SendMessage(hUnregisterButton, BCM_SETSHIELD, 0, 1);

    // 活性状態
    CString strRegValue;
    if (!GetStartupRegValue(strRegValue)) {
        EnableWindow(hRegisterButton, FALSE);
        EnableWindow(hUnregisterButton, FALSE);
    }
    else {
        if (strRegValue.IsEmpty()) {
            // 未登録
            EnableWindow(hRegisterButton, TRUE);
            EnableWindow(hUnregisterButton, FALSE);
        }
        else {
            // 登録済み
            EnableWindow(hRegisterButton, FALSE);
            EnableWindow(hUnregisterButton, TRUE);
        }
    }
}

void RegisterStartup(bool bRegister, HWND hDlg)
{
    Logger::d(L"startup");

    // ファイル名を指定する
    CString filename;
    CString registerExePath;
    {
        TCHAR modulePath[MAX_PATH];
        GetModuleFileName(NULL, modulePath, MAX_PATH);

        TCHAR drive[MAX_PATH + 1], dir[MAX_PATH + 1], fname[MAX_PATH + 1], ext[MAX_PATH + 1];
        _wsplitpath_s(modulePath, drive, dir, fname, ext);

        // ファイル名の生成
        filename.Format(L"%s%s", fname, ext);

        // 登録用プログラムのパス生成
        registerExePath.Format(L"%s%s%s", drive, dir, L"register.exe");
    }

    SHELLEXECUTEINFOW sei = { 0 };
    sei.cbSize = sizeof(SHELLEXECUTEINFO);
    sei.hwnd = hDlg;
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = NULL;
    sei.lpFile = (LPCWSTR)registerExePath;

    // 登録/解除指定
    sei.lpParameters = bRegister ? filename : L"/uninstall";

    // プロセス起動
    if (!ShellExecuteEx(&sei) || (const int)sei.hInstApp <= 32) {
        Logger::d(L"error ShellExecuteEx (%d)", GetLastError());
        return;
    }

    // 終了を待つ
    WaitForSingleObject(sei.hProcess, INFINITE);

    // 終了コード取得
    DWORD dwExitCode = STARTUP_REGISTER_EXIT_CODE_FAILED_UNKNOWN;
    GetExitCodeProcess(sei.hProcess, &dwExitCode);

    Logger::d(L"done: %d", dwExitCode);

    LPCWSTR message = NULL;
    UINT uType = MB_OK;
    switch (dwExitCode) {
    case STARTUP_REGISTER_EXIT_CODE_FAILED_OPEN_KEY:
        message = L"キーのオープンに失敗しました";
        uType |= MB_ICONERROR;
        break;
    case STARTUP_REGISTER_EXIT_CODE_FAILED_DELETE:
        message = L"キーの削除に失敗しました";
        uType |= MB_ICONERROR;
        break;
    case STARTUP_REGISTER_EXIT_CODE_FAILED_REGISTER:
        message = L"キーの登録に失敗しました";
        uType |= MB_ICONERROR;
        break;
    case STARTUP_REGISTER_EXIT_CODE_SUCCESS:
        if (bRegister) {
            message = L"スタートアップに登録しました";
        }
        else {
            message = L"スタートアップを解除しました";
        }
        break;
    }
    if (message != NULL) {
        MessageBoxW(hDlg, message, g_szAppTitle, uType);
    }

    // ボタン状態の更新
    UpdateRegisterButtons(hDlg);
}

bool GetStartupRegValue(CString& strRegValue)
{
    HKEY hKey = NULL;

    LSTATUS rval;
    rval = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &hKey);
    if (rval != ERROR_SUCCESS) {
        Logger::d(L"cannot open key");
        return false;
    }

    DWORD dwType;
    TCHAR lpData[256];
    DWORD dwDataSize;

    dwDataSize = sizeof(lpData) / sizeof(lpData[0]);

    rval = RegQueryValueEx(
        hKey,
        TEXT("CompactMeter"),
        0,
        &dwType,
        (LPBYTE)lpData,
        &dwDataSize);
    if (rval != ERROR_SUCCESS) {

        Logger::d(L"cannot query key 0x%08x", rval);
        if (rval == ERROR_FILE_NOT_FOUND) {
            // キーなし
            strRegValue = L"";
        }
        else {
            RegCloseKey(hKey);
            return false;
        }
    }
    else {
//        Logger::d(L"Query: %s", lpData);
        strRegValue = lpData;
    }

    RegCloseKey(hKey);
    return true;
}

int GetSelectedMeterConfigListIndex(HWND hList)
{
    int iSelected = -1;
    int n = ListView_GetItemCount(hList);
    for (int i = 0; i < n; i++) {
        if (ListView_GetItemState(hList, i, LVIS_SELECTED) & LVIS_SELECTED) {
            iSelected = i;
            break;
        }
    }

    return iSelected;
}

void MoveMeterPos(const HWND &hDlg, bool moveToUp)
{
    HWND hList = GetDlgItem(hDlg, IDC_METER_CONFIG_LIST);

    int iSelected = GetSelectedMeterConfigListIndex(hList);
    if (iSelected == -1) {
        Logger::d(L"no selection");
        return;
    }

    if (moveToUp) {
        // 上に移動
        SwapMeterItem(hList, iSelected - 1, iSelected);
    }
    else {
        // 下に移動
        SwapMeterItem(hList, iSelected, iSelected + 1);
    }
}

void SwapMeterItem(HWND hList, int iTarget1, int iTarget2) {

    const int n = ListView_GetItemCount(hList);

    if (iTarget1 < 0 || iTarget2 < 0 || iTarget1 >= n || iTarget2 >= n) {
        // 範囲外
        Logger::d(L"範囲外 %d, %d", iTarget1, iTarget2);
        return;
    }

    // UI 変更
    WCHAR szText1[256], szText2[256];
    ListView_GetItemText(hList, iTarget1, 0, szText1, 256);
    ListView_GetItemText(hList, iTarget2, 0, szText2, 256);
    ListView_SetItemText(hList, iTarget1, 0, szText2);
    ListView_SetItemText(hList, iTarget2, 0, szText1);

    initializing = true;
    int mask = LVIS_SELECTED | LVIS_STATEIMAGEMASK;
    int iState1 = ListView_GetItemState(hList, iTarget1, mask);
    int iState2 = ListView_GetItemState(hList, iTarget2, mask);
    ListView_SetItemState(hList, iTarget1, iState2, mask);
    ListView_SetItemState(hList, iTarget2, iState1, mask);
    initializing = false;

    // データ変更
    auto w = g_pIniConfig->mMeterConfigs[iTarget1];
    g_pIniConfig->mMeterConfigs[iTarget1] = g_pIniConfig->mMeterConfigs[iTarget2];
    g_pIniConfig->mMeterConfigs[iTarget2] = w;

    // 保存
    g_pIniConfig->Save();
}

void SetMeterConfigListViewBackgroundColor(HWND hList, COLORREF backgroundColor, int index)
{
    LVITEM item;
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.iSubItem = 1;

    WCHAR szText[256];
    item.pszText = szText;
    COLORREF c = MyUtil::direct2DColorToRGB(backgroundColor);
    swprintf_s(szText, L"#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));

    ListView_SetItem(hList, &item);
}
