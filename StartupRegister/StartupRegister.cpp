#include "stdafx.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

    LPCWSTR caption = L"CompactMeter";

    // ��̏ꍇ�̓L�[�폜�A�w�肳��Ă���΂�����t�@�C�����Ƃ݂Ȃ��ēo�^����
    wprintf(L"cmdline: [%s]\n", lpCmdLine);

    HKEY hKey;
    LSTATUS rval;

    rval = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey);
    if (rval != ERROR_SUCCESS) {
        MessageBoxW(NULL, L"�L�[�̃I�[�v���Ɏ��s���܂���", caption, MB_OK | MB_ICONERROR);
        return 1;
    }
    wprintf(L"open: [0x%08x]\n", rval);

    LPCWSTR keyName = L"CompactMeter";
    if (wcslen(lpCmdLine) == 0) {
        // �o�^����
        rval = RegDeleteValue(hKey, keyName);
        wprintf(L"delete: [0x%08x]\n", rval);
        if (rval != ERROR_SUCCESS) {
            MessageBoxW(NULL, L"�L�[�̍폜�Ɏ��s���܂���", caption, MB_OK | MB_ICONERROR);
        }
        else {
            MessageBoxW(NULL, L"�X�^�[�g�A�b�v���������܂���", caption, MB_OK);
        }
    }
    else {
        TCHAR fullpath[MAX_PATH];
        {
            TCHAR modulePath[MAX_PATH];
            GetModuleFileName(NULL, modulePath, MAX_PATH);

            TCHAR drive[MAX_PATH + 1], dir[MAX_PATH + 1], fname[MAX_PATH + 1], ext[MAX_PATH + 1];
            _wsplitpath_s(modulePath, drive, dir, fname, ext);

            swprintf_s(fullpath, L"%s%s%s", drive, dir, L"CompactMeter.exe");
        }

        wprintf(L"path: %s\n", fullpath);

        rval = RegSetValueExW(hKey, keyName, 0, REG_SZ, (LPBYTE)fullpath, wcslen(fullpath) * sizeof(TCHAR));
        wprintf(L"set: [0x%08x]\n", rval);
        if (rval != ERROR_SUCCESS) {
            MessageBoxW(NULL, L"�L�[�̓o�^�Ɏ��s���܂���", caption, MB_OK | MB_ICONERROR);
        }
        else {
            MessageBoxW(NULL, L"�X�^�[�g�A�b�v�ɓo�^���܂���", caption, MB_OK);
        }
    }

    RegCloseKey(hKey);

    return 0;
}
