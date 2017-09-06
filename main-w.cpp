#ifdef _WIN32

#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <cstdint>

#include <shlwapi.h>
#pragma comment(lib,"shlwapi")

#include <commctrl.h>
#pragma comment(lib,"comctl32")

#include <vector>

using namespace std;

#include <tchar.h>

#include "./pxtone/pxtnService.h"
#include "./pxtone/pxtnError.h"

#include "convert.hpp"

static const TCHAR* _app_name = _T("ptmidi");

#include <CommDlg.h>

#define _CHANNEL_NUM           2
#define _SAMPLE_PER_SECOND 44100
#define _BUFFER_PER_SEC    (0.3f)

static bool _SelFile_OpenLoad(HWND hwnd, TCHAR *path_get, const TCHAR *path_def_dir, const TCHAR *title, const TCHAR *ext, const TCHAR *filter)
{
	OPENFILENAME ofn = { 0 };

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = path_get;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = path_def_dir;
	ofn.lpstrTitle = title;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = ext;

	if (GetOpenFileName(&ofn)) return true;

	return false;
}


static bool _load_ptcop(pxtnService* pxtn, const TCHAR* path_src, pxtnERR* p_pxtn_err)
{
	bool           b_ret = false;
	pxtnDescriptor desc;
	pxtnERR        pxtn_err = pxtnERR_VOID;
	FILE*          fp = NULL;
	int32_t        event_num = 0;

	if (!(fp = _tfopen(path_src, _T("rb")))) goto term;
	if (!desc.set_file_r(fp)) goto term;

	pxtn_err = pxtn->read(&desc); if (pxtn_err != pxtnOK) goto term;
	pxtn_err = pxtn->tones_ready(); if (pxtn_err != pxtnOK) goto term;

	b_ret = true;
term:

	if (fp) fclose(fp);
	if (!b_ret) pxtn->evels->Release();

	if (p_pxtn_err) *p_pxtn_err = pxtn_err;

	return b_ret;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nCmd)
{
	InitCommonControls();
	if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) return 0;

	bool           b_ret = false;
	pxtnService*   pxtn = NULL;
	pxtnERR        pxtn_err = pxtnERR_VOID;
	TCHAR          path_src[MAX_PATH] = { 0 };

	static const TCHAR* title = _T("load ptcop");
	static const TCHAR* ext = _T("ptcop");
	static const TCHAR* filter =
		_T("ptcop {*.ptcop}\0*.ptcop*\0")
		_T("pttune {*.pttune}\0*.pttune*\0")
		_T("All files {*.*}\0*.*\0\0");

	// INIT PXTONE.
	pxtn = new pxtnService();
	pxtn_err = pxtn->init(); if (pxtn_err != pxtnOK) goto term;

	// SELECT MUSIC FILE.
	if (!_SelFile_OpenLoad(NULL, path_src, NULL, title, ext, filter))
	{
		b_ret = true;
		goto term;
	};

	// LOAD MUSIC FILE.
	if (!_load_ptcop(pxtn, path_src, &pxtn_err)) goto term;

	/// DO THE CONVERSION, AND WRITE
	pxtn_to_midi(*pxtn).write(string(path_src) + ".mid");

	// GET MUSIC NAME AND MESSAGE-BOX.
	{
		TCHAR         text[250] = { 0 };
		TCHAR*        name_t = NULL;
		int32_t       buf_size = 0;

		_stprintf_s(text, 250, _T("created %s.mid\r\n"), PathFindFileName(path_src));

		if (name_t) free(name_t);
		MessageBox(NULL, text, _app_name, MB_OK);
	}

	b_ret = true;
term:

	if (!b_ret)
	{
		// ERROR MESSAGE.
		TCHAR err_msg[100] = { 0 };
		_stprintf_s(err_msg, 100, _T("ERROR: pxtnERR[ %s ]"), pxtnError_get_string(pxtn_err));
		MessageBox(NULL, err_msg, _app_name, MB_OK);
	}

	SAFE_DELETE(pxtn);
	return 1;
}

#endif // WIN32
