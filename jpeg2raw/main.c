#include <Windows.h>
#include <stdio.h>
#include <setjmp.h>
#include "inc/jpeglib.h"

enum id{
	ID_BUTTON_OPEN_JEPG = 200,
	ID_BUTTON_GENERATE,
};

struct j_error_mgr {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};

static LRESULT CALLBACK MainWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static WCHAR JpegFilePath[MAX_PATH] = { 0 };

static void jpeg_error_exit(j_common_ptr cinfo)
{
	struct j_error_mgr *jerr = (struct j_error_mgr*)cinfo->err;
	longjmp(jerr->setjmp_buffer, 1);
}

static int ParseJpeg(WCHAR *OutputFilePath)
{
	errno_t err;
	FILE *JpegFile;
	FILE *OutputFile;
	JSAMPARRAY buffer;
	struct j_error_mgr jerr;
	struct jpeg_decompress_struct cinfo;
	int row_stride;
	int i;

	err = _wfopen_s(&JpegFile, JpegFilePath, TEXT("rb"));
	if (err != 0)
		return -err;

	err = _wfopen_s(&OutputFile, OutputFilePath, TEXT("wb"));
	if (err != 0)
		return -err;

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		fclose(JpegFile);
		return -1;
	}

	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, JpegFile);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	row_stride = cinfo.output_width * cinfo.output_components;
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

	while (cinfo.output_scanline < cinfo.output_height)
	{
		jpeg_read_scanlines(&cinfo, buffer, 1);
		for (i = 0; i < row_stride; i++)
		{
			fprintf_s(OutputFile, "0x%02x, ", buffer[0][i]);
			if (i % 10 == 9)
				fprintf_s(OutputFile, "\r\n");
		}
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	fclose(JpegFile);
	fclose(OutputFile);

	return 0;
}

static HFONT GetDefaultFont(void)
{
	return CreateFont(15, 7, 0, 0, 400,
		FALSE, FALSE, FALSE,
		DEFAULT_CHARSET,
		OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS,
		DEFAULT_QUALITY,
		FF_DONTCARE,
		TEXT("Courier New")
	);
}

static void InitWidgets(HINSTANCE hInstance, HWND hWnd, HFONT hFont)
{
	HWND hBtn;
	hBtn = CreateWindow(TEXT("Button"), TEXT("Open Jpeg"), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, 20, 100, 30, hWnd, (HMENU)ID_BUTTON_OPEN_JEPG, hInstance, NULL);
	SendMessage(hBtn, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));

	hBtn = CreateWindow(TEXT("Button"), TEXT("Generate"), WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED, 200, 20, 100, 30, hWnd, (HMENU)ID_BUTTON_GENERATE, hInstance, NULL);
	SendMessage(hBtn, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
}

static void CenterWindow(HWND hwnd)
{
	RECT winrect, workrect;
	int workwidth, workheight, winwidth, winheight;

	SystemParametersInfo(SPI_GETWORKAREA, 0, &workrect, 0);
	workwidth = workrect.right - workrect.left;
	workheight = workrect.bottom - workrect.top;

	GetWindowRect(hwnd, &winrect);
	winwidth = winrect.right - winrect.left;
	winheight = winrect.bottom - winrect.top;

	winwidth = min(winwidth, workwidth);
	winheight = min(winheight, workheight);

	SetWindowPos(hwnd, HWND_TOP,
		workrect.left + (workwidth - winwidth) / 2,
		workrect.top + (workheight - winheight) / 2,
		winwidth, winheight,
		SWP_SHOWWINDOW);
	SetForegroundWindow(hwnd);
}

static LRESULT ButtonClicked(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	WORD lowWord = LOWORD(wParam);
	WCHAR path[MAX_PATH] = { 0 };
	OPENFILENAME o = { 0 };

	switch (lowWord)
	{
	case ID_BUTTON_OPEN_JEPG:

		o.lStructSize = sizeof(OPENFILENAME);
		o.hwndOwner = hWnd;
		o.lpstrFilter = TEXT("Jpeg file\0*.jpeg;*.jpg\0");
		o.lpstrFileTitle = TEXT("Open jpeg file");
		o.lpstrFile = path;
		o.nMaxFile = sizeof(path);
		o.hInstance = (HINSTANCE)GetModuleHandle(NULL);
		o.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
		if (0 != GetOpenFileName(&o))
		{
			lstrcpyn(JpegFilePath, path, sizeof(path));
			HWND hDlgWnd = GetDlgItem(hWnd, ID_BUTTON_GENERATE);
			EnableWindow(hDlgWnd, TRUE);
		}
		
		break;
	case ID_BUTTON_GENERATE:
		
		o.lStructSize = sizeof(OPENFILENAME);
		o.hwndOwner = hWnd;
		o.lpstrFilter = TEXT("Text file\0*.txt\0");
		o.lpstrFileTitle = TEXT("Save text file");
		o.lpstrFile = path;
		o.nMaxFile = sizeof(path);
		if (0 != GetSaveFileName(&o))
		{
			if (0 == ParseJpeg(path))
				MessageBox(hWnd, TEXT("Generate done."), TEXT("Info"), MB_OK);
			else
				MessageBox(hWnd, TEXT("Generate fail."), TEXT("Error"), MB_OK);
		}
		break;
	default:
		break;
	}

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASS wndcls;
	HWND hwnd;
	MSG msg;

	wndcls.cbClsExtra = 0;
	wndcls.cbWndExtra = 0;
	wndcls.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndcls.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndcls.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndcls.hInstance = hInstance;
	wndcls.lpfnWndProc = MainWinProc;
	wndcls.lpszClassName = TEXT("Jpeg2Raw");
	wndcls.lpszMenuName = NULL;
	wndcls.style = CS_HREDRAW | CS_VREDRAW;
	if (!RegisterClass(&wndcls))
		return FALSE;

	hwnd = CreateWindow(TEXT("Jpeg2Raw"),
		TEXT("Jpeg2Raw"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		340,
		110,
		NULL,
		NULL,
		hInstance,
		NULL);

	ShowWindow(hwnd, SW_SHOWNORMAL);
	UpdateWindow(hwnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}

static LRESULT CALLBACK MainWinProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HFONT hFont;
	HINSTANCE hInstance;
	WORD hiWord;

	switch (uMsg)
	{
	case WM_CREATE:
		hInstance = ((LPCREATESTRUCTA)lParam)->hInstance;
		hFont = GetDefaultFont();
		CenterWindow(hWnd);
		InitWidgets(hInstance, hWnd, hFont);
		return 0;
	case WM_COMMAND:
		hiWord = HIWORD(wParam);
		if (hiWord == BN_CLICKED)
			ButtonClicked(hWnd, wParam, lParam);
		return 0;
	case WM_SIZE:
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return 0;
}
