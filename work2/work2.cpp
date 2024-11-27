#include <windows.h>
#include <commdlg.h>
#include <string>
#include <fstream>
#include <vector>
#include <iostream>

using namespace std;

// Глобальные переменные
#define IDM_OPEN 1001

struct ImgInfo {
    std::vector<COLORREF>* pixels;
    int height, width;
    int figXMin, figXMax, figYMin, figYMax;
    COLORREF backgroundColor;
};

bool OpenBMPFile(HWND hWnd, wchar_t** selectedFile) {
    OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = L"";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"BMP Files\0*.bmp\0";
    ofn.lpstrFile = szFile;
    ofn.lpstrTitle = L"Выберите файл";
    ofn.nMaxFile = sizeof(szFile);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        *selectedFile = _wcsdup(szFile);
        return true;
    }
    return false;
}

bool isSimilarColor(COLORREF color1, COLORREF color2, int tolerance) {
    int r1 = GetRValue(color1), g1 = GetGValue(color1), b1 = GetBValue(color1);
    int r2 = GetRValue(color2), g2 = GetGValue(color2), b2 = GetBValue(color2);
    return (abs(r1 - r2) <= tolerance && abs(g1 - g2) <= tolerance && abs(b1 - b2) <= tolerance);
}

ImgInfo* readPixelColorsFromFile(const wchar_t* filename) {
    ifstream file(filename, ios::binary);
    if (!file.is_open()) return nullptr;

    char header[54];
    file.read(header, 54);

    int width = *(int*)&header[18];
    int height = *(int*)&header[22];
    int rowSize = ((width * 3 + 3) & ~3); // Учет padding
    char* rowData = new char[rowSize];

    vector<COLORREF>* pixelColors = new vector<COLORREF>();
    COLORREF backgroundColor;

    for (int y = 0; y < height; ++y) {
        file.read(rowData, rowSize);
        for (int x = 0; x < width; ++x) {
            int i = x * 3;
            COLORREF color = RGB((unsigned char)rowData[i + 2], (unsigned char)rowData[i + 1], (unsigned char)rowData[i]);
            if (y == 0 && x == 0) backgroundColor = color; // Первый пиксель — цвет фона
            pixelColors->push_back(color);
        }
    }

    delete[] rowData;

    ImgInfo* info = new ImgInfo{ pixelColors, height, width, INT_MAX, 0, INT_MAX, 0, backgroundColor };

    // Найти границы фигуры
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            COLORREF color = pixelColors->at(y * width + x);
            if (!isSimilarColor(color, backgroundColor, 10)) {
                info->figXMin = min(info->figXMin, x);
                info->figXMax = max(info->figXMax, x);
                info->figYMin = min(info->figYMin, y);
                info->figYMax = max(info->figYMax, y);
            }
        }
    }

    return info;
}

void DrawImage(const ImgInfo* imageInfo, HDC hdcWindow, HWND hwnd) {
    int figWidth = imageInfo->figXMax - imageInfo->figXMin + 1;
    int figHeight = imageInfo->figYMax - imageInfo->figYMin + 1;

    // Закрашиваем фон окна белым цветом
    HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    RECT windowRect;
    GetClientRect(hwnd, &windowRect);
    FillRect(hdcWindow, &windowRect, whiteBrush);
    DeleteObject(whiteBrush);

    // Вычисляем координаты центра окна
    int windowCenterX = (windowRect.right - windowRect.left) / 2;
    int windowBottom = windowRect.bottom;

    int positionX = windowCenterX - figWidth / 2;
    int positionY = windowBottom - figHeight;

    for (int x = 0; x < figWidth; ++x) {
        for (int y = 0; y < figHeight; ++y) {
            int flippedX = x;
            int flippedY = figHeight - 1 - y; // Вертикальное отзеркаливание

            COLORREF color = imageInfo->pixels->at((imageInfo->figYMin + flippedY) * imageInfo->width + imageInfo->figXMin + flippedX);
            if (!isSimilarColor(color, imageInfo->backgroundColor, 10)) {
                SetPixel(hdcWindow, positionX + x, positionY + y, color);
            }
        }
    }
}

// Обработчик сообщений окна
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static wchar_t* selectedFile = nullptr;
    static ImgInfo* info = nullptr;

    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDM_OPEN) {
            if (OpenBMPFile(hwnd, &selectedFile)) {
                if (info) delete info; // Удаляем предыдущее изображение
                info = readPixelColorsFromFile(selectedFile);
                if (info) {
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (info) {
            DrawImage(info, hdc, hwnd);
        }
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_CLOSE:
        if (info) delete info;
        PostQuitMessage(0);
        break;

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Точка входа
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"MyWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Пример отображения BMP",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 0;

    HMENU hMenu = CreateMenu();
    HMENU hSubMenu = CreatePopupMenu();
    AppendMenu(hSubMenu, MF_STRING, IDM_OPEN, L"Открыть  файл");
    AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, L"Файл");
    SetMenu(hwnd, hMenu);

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
