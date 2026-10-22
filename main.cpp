#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int CANVAS_W = 1120;
constexpr int CANVAS_H = 760;
constexpr UINT_PTR ANIMATION_TIMER = 1;
constexpr UINT_PTR ADVANCE_TIMER = 2;
constexpr UINT ADVANCE_DELAY_MS = 1000;
constexpr double PI = 3.14159265358979323846;

enum class Screen { Home, Instructions, Levels, Challenge, Victory };
enum class Action { None, Heat, Cold, Sun, Rain };

struct Challenge {
    const wchar_t* title;
    const wchar_t* question;
    const wchar_t* explanation;
    std::array<const wchar_t*, 3> answers;
    int correct;
    Action action;
    const wchar_t* actionLabel;
    const wchar_t* actionHint;
    const wchar_t* illustration;
};

const std::array<Challenge, 10> challenges{{
    {L"1. Agua líquida", L"¿En qué estado está el agua del río?",
     L"El agua líquida fluye y toma la forma del lugar que la contiene.",
     {L"Líquido: puede fluir", L"Sólido: es hielo", L"Gaseoso: es vapor"}, 0, Action::None, L"",
     L"Observa cómo el río corre.", L"RÍO"},
    {L"2. Hielo sólido", L"¿Qué estado del agua es este cubito?",
     L"El hielo es agua sólida. Mantiene su forma mientras está congelado.",
     {L"Gas invisible", L"Hielo sólido", L"Agua líquida"}, 1, Action::None, L"",
     L"El copo de nieve da una pista.", L"HIELO"},
    {L"3. Vapor de agua", L"¿Qué pasa con el agua cuando se vuelve vapor?",
     L"El vapor de agua es un gas invisible. Las nubecitas blancas visibles son gotitas, no vapor.",
     {L"Se convierte en gas invisible", L"Se vuelve hielo", L"Se convierte en una nube visible"}, 0, Action::None, L"",
     L"El vapor de agua no se ve.", L"VAPOR"},
    {L"4. El hielo se derrite", L"¿Qué botón ayuda al hielo a derretirse?",
     L"Con calor, el hielo se derrite y pasa de sólido a líquido.",
     {L"El botón de frío", L"El botón de calor", L"El botón de lluvia"}, 1, Action::Heat, L"APLICAR CALOR",
     L"Pulsa calor y mira el cubito.", L"DERRETIR"},
    {L"5. El agua se congela", L"¿Qué le ocurre al agua cuando se enfría mucho?",
     L"Con suficiente frío, el agua líquida se congela y pasa a ser hielo sólido.",
     {L"Se convierte en vapor", L"Se transforma en nube", L"Se congela y forma hielo"}, 2, Action::Cold, L"APLICAR FRÍO",
     L"Pulsa frío y mira el agua.", L"CONGELAR"},
    {L"6. Evaporación", L"¿Qué hace el calor con el agua de un charco?",
     L"El calor ayuda al agua líquida a evaporarse y convertirse en vapor invisible.",
     {L"La convierte en vapor invisible", L"La convierte directamente en nieve", L"La hace desaparecer para siempre"}, 0,
     Action::Heat, L"APLICAR CALOR", L"Calienta el charco para ver qué ocurre.", L"EVAPORAR"},
    {L"7. Condensación", L"¿Qué ocurre cuando el vapor de agua se enfría?",
     L"Al enfriarse, el vapor se condensa y forma pequeñísimas gotas de agua líquida.",
     {L"Se convierte en fuego", L"Forma gotitas de agua", L"Se vuelve invisible para siempre"}, 1, Action::Cold,
     L"ENFRIAR VAPOR", L"Enfría el vapor invisible.", L"CONDENSAR"},
    {L"8. El Sol calienta", L"¿Qué pone en marcha la evaporación del agua?",
     L"La energía del Sol calienta mares, ríos y lagos. Parte del agua se evapora.",
     {L"El brillo de la Luna", L"El viento por sí solo", L"El calor del Sol"}, 2, Action::Sun, L"BRILLAR SOL",
     L"Pulsa el Sol para calentar el mar.", L"EL SOL"},
    {L"9. Nacen las nubes", L"¿Cómo se forman muchas nubes?",
     L"El vapor invisible sube, se enfría y se condensa en gotitas diminutas o cristales de hielo.",
     {L"Se enfría y forma gotitas o cristales", L"Son grandes bolas de vapor visible", L"El Sol las pinta en el cielo"}, 0,
     Action::Cold, L"ENFRIAR AIRE", L"Enfría el aire de la montaña.", L"NUBES"},
    {L"10. La lluvia regresa", L"¿A dónde va el agua cuando llueve?",
     L"Las gotas caen a la superficie y alimentan ríos, lagos y el suelo. El ciclo continúa.",
     {L"Al espacio para siempre", L"A ríos, lagos y al suelo", L"Se convierte siempre en nieve"}, 1,
     Action::Rain, L"¡HACER LLOVER!", L"Pulsa el botón para devolver el agua.", L"LLUVIA"}
}};

struct Rect { int left, top, right, bottom; };

HINSTANCE appInstance{};
HWND mainWindow{};
Screen screen = Screen::Home;
int currentChallenge = 0;
std::array<bool, 10> completedChallenges{};
int lastAnswer = -1;
int animationTick = 0;
int celebrationTicks = 0;
bool soundEnabled = true;
bool actionActivated = false;
bool dragging = false;
bool answerAdvancePending = false;
int draggedAnswer = -1;
POINT cursorPoint{};
POINT dragStartPoint{};
std::wstring feedback;
COLORREF colors[8]{
    RGB(22, 100, 183), RGB(69, 192, 222), RGB(253, 210, 72), RGB(255, 144, 126),
    RGB(119, 208, 160), RGB(248, 247, 239), RGB(49, 75, 111), RGB(185, 139, 224)
};
std::vector<BYTE> soundWave;

COLORREF Blend(COLORREF a, COLORREF b, double t) {
    t = std::clamp(t, 0.0, 1.0);
    return RGB(
        static_cast<BYTE>(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t),
        static_cast<BYTE>(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
        static_cast<BYTE>(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}

void FillRectColor(HDC dc, int x1, int y1, int x2, int y2, COLORREF color) {
    RECT r{x1, y1, x2, y2};
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &r, brush);
    DeleteObject(brush);
}

void RoundedBox(HDC dc, int x1, int y1, int x2, int y2, COLORREF fill,
                COLORREF outline = RGB(255, 255, 255), int radius = 22, int penWidth = 2) {
    HPEN pen = CreatePen(PS_SOLID, penWidth, outline);
    HBRUSH brush = CreateSolidBrush(fill);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    RoundRect(dc, x1, y1, x2, y2, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void Text(HDC dc, const wchar_t* value, Rect r, int size, COLORREF color,
          UINT align = DT_CENTER | DT_VCENTER | DT_WORDBREAK, bool bold = false) {
    HFONT font = CreateFontW(-size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    RECT area{r.left, r.top, r.right, r.bottom};
    DrawTextW(dc, value, -1, &area, align);
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void Circle(HDC dc, int x, int y, int radius, COLORREF fill, COLORREF outline = RGB(255,255,255), int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, width, outline);
    HBRUSH brush = CreateSolidBrush(fill);
    HGDIOBJ oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, brush);
    Ellipse(dc, x - radius, y - radius, x + radius, y + radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void Line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 2) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void Gradient(HDC dc, COLORREF top, COLORREF bottom) {
    for (int y = 0; y < CANVAS_H; y += 2)
        FillRectColor(dc, 0, y, CANVAS_W, y + 2, Blend(top, bottom, static_cast<double>(y) / CANVAS_H));
}

void Cloud(HDC dc, int x, int y, int scale, COLORREF color) {
    Circle(dc, x - scale * 2 / 5, y + scale / 8, scale / 2, color, color);
    Circle(dc, x, y - scale / 8, scale * 3 / 5, color, color);
    Circle(dc, x + scale * 2 / 5, y + scale / 8, scale / 2, color, color);
    FillRectColor(dc, x - scale * 7 / 10, y, x + scale * 7 / 10, y + scale / 3, color);
}

void Rainbow(HDC dc, int cx, int cy, int radius, int thickness = 14) {
    const COLORREF band[]{RGB(255,106,117), RGB(255,177,86), RGB(255,219,89),
                          RGB(103,203,140), RGB(94,179,239), RGB(165,132,231)};
    for (int i = 0; i < 6; ++i) {
        HPEN pen = CreatePen(PS_SOLID, thickness, band[i]);
        HGDIOBJ old = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Arc(dc, cx - radius + i * thickness / 2, cy - radius + i * thickness / 2,
            cx + radius - i * thickness / 2, cy + radius - i * thickness / 2,
            cx - radius, cy, cx + radius, cy);
        SelectObject(dc, oldBrush);
        SelectObject(dc, old);
        DeleteObject(pen);
    }
}

void DrawDrop(HDC dc, int cx, int cy, int scale, COLORREF color, bool face = true) {
    POINT p[]{
        {cx,cy-scale}, {cx-scale/3,cy-scale/2}, {cx-scale*3/5,cy-scale/8},
        {cx-scale*4/5,cy+scale/3}, {cx-scale*3/5,cy+scale*3/4},
        {cx,cy+scale}, {cx+scale*3/5,cy+scale*3/4}, {cx+scale*4/5,cy+scale/3},
        {cx+scale*3/5,cy-scale/8}, {cx+scale/3,cy-scale/2}
    };
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(35, 131, 202));
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, brush);
    Polygon(dc, p, 10);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
    Circle(dc, cx - scale / 3, cy - scale / 8, std::max(2, scale / 12), RGB(255,255,255), RGB(255,255,255));
    if (face) {
        Circle(dc, cx - scale / 5, cy + scale / 8, std::max(2, scale / 14), RGB(33,68,104), RGB(33,68,104));
        Circle(dc, cx + scale / 5, cy + scale / 8, std::max(2, scale / 14), RGB(33,68,104), RGB(33,68,104));
        HPEN smile = CreatePen(PS_SOLID, std::max(2, scale / 16), RGB(33,68,104));
        HGDIOBJ old = SelectObject(dc, smile);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Arc(dc, cx - scale / 4, cy + scale / 7, cx + scale / 4, cy + scale / 2, cx - scale / 4, cy + scale / 3, cx + scale / 4, cy + scale / 3);
        SelectObject(dc, oldBrush);
        SelectObject(dc, old);
        DeleteObject(smile);
    }
}

void DrawSnowflake(HDC dc, int cx, int cy, int radius, double angle) {
    const COLORREF ice = RGB(255, 255, 255);
    for (int i = 0; i < 6; ++i) {
        double a = angle + i * PI / 3.0;
        int x = cx + static_cast<int>(std::cos(a) * radius);
        int y = cy + static_cast<int>(std::sin(a) * radius);
        Line(dc, cx, cy, x, y, ice, 3);
        for (int sign : {-1, 1}) {
            double b = a + sign * 2.5;
            Line(dc, x, y, x - static_cast<int>(std::cos(b) * radius / 3),
                 y - static_cast<int>(std::sin(b) * radius / 3), ice, 2);
        }
    }
}

void FilledPolygon(HDC dc, const POINT* points, int count, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Polygon(dc, points, count);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(brush);
}

void DrawSceneArt(HDC dc, const wchar_t* kind, int cx, int cy, int scale, bool animated) {
    std::wstring name(kind);
    if (name == L"RÍO") {
        POINT leftBank[]{{cx - scale,cy - scale / 3},{cx - scale / 4,cy - scale / 3},
                         {cx - scale / 3,cy + scale},{cx - scale,cy + scale}};
        POINT rightBank[]{{cx + scale / 4,cy - scale / 2},{cx + scale,cy - scale / 2},
                          {cx + scale,cy + scale},{cx + scale / 3,cy + scale}};
        FilledPolygon(dc, leftBank, 4, RGB(92,179,132));
        FilledPolygon(dc, rightBank, 4, RGB(115,195,141));
        FillRectColor(dc, cx - scale / 4, cy - scale / 3, cx + scale / 3, cy + scale / 2, RGB(109,211,247));
        FillRectColor(dc, cx - scale / 3, cy + scale / 2, cx + scale / 2, cy + scale * 3 / 4, RGB(71,174,228));
        for (int i = 0; i < 4; ++i) {
            int xx = cx - 10 + i * 9;
            int drift = animated ? (animationTick / 3 + i * 7) % 18 : 0;
            Line(dc, xx, cy + drift, xx, cy + scale / 2 - 4, RGB(229,250,255), 3);
        }
        Circle(dc, cx - scale / 2, cy - scale / 3, 9, RGB(120,206,128), RGB(120,206,128));
        return;
    }
    if (name == L"HIELO") {
        POINT ice[]{{cx - scale / 2,cy - scale / 2},{cx + scale / 3,cy - scale / 2},
                    {cx + scale / 2,cy - scale / 3},{cx + scale / 2,cy + scale / 2},
                    {cx - scale / 3,cy + scale / 2},{cx - scale / 2,cy + scale / 3}};
        HBRUSH b = CreateSolidBrush(RGB(181,234,255));
        HGDIOBJ old = SelectObject(dc, b);
        Polygon(dc, ice, 6);
        SelectObject(dc, old);
        DeleteObject(b);
        Line(dc, cx - 20, cy - 14, cx + 16, cy + 8, RGB(255,255,255), 4);
        Line(dc, cx + 5, cy - 33, cx + 28, cy - 10, RGB(255,255,255), 3);
        DrawSnowflake(dc, cx + scale / 2, cy - scale / 2, 17, animationTick * 0.06);
        return;
    }
    if (name == L"VAPOR" || name == L"DERRETIR" || name == L"EVAPORAR") {
        if (name == L"DERRETIR" && !animated) {
            POINT ice[]{{cx - 35,cy - 31},{cx + 27,cy - 31},{cx + 37,cy - 19},
                        {cx + 37,cy + 28},{cx - 26,cy + 28},{cx - 35,cy + 18}};
            FilledPolygon(dc, ice, 6, RGB(180,232,252));
            Line(dc, cx - 15, cy - 16, cx + 12, cy + 2, RGB(255,255,255), 3);
        } else {
            if (name == L"EVAPORAR")
                FillRectColor(dc, cx - 52, cy + 32, cx + 52, cy + 43, RGB(84,185,235));
            if (name == L"DERRETIR")
                FillRectColor(dc, cx - 54, cy + 37, cx + 54, cy + 47, RGB(84,185,235));
            if (name == L"VAPOR" || animated) {
                for (int i = 0; i < 3; ++i) {
                    int x = cx - 30 + i * 30;
                    int rise = (animationTick * 2 + i * 17) % 38;
                    Line(dc, x, cy + 22 - rise, x, cy - 5 - rise, RGB(144,215,243), 3);
                    Line(dc, x, cy - 5 - rise, x - 6, cy + 2 - rise, RGB(144,215,243), 3);
                }
            }
        }
        return;
    }
    if (name == L"CONGELAR") {
        if (animated) {
            Circle(dc, cx, cy, 49, RGB(169,232,255), RGB(255,255,255), 3);
            DrawSnowflake(dc, cx, cy, 39, animationTick * 0.07);
        } else {
            DrawDrop(dc, cx, cy, 55, RGB(79,184,239), false);
        }
        return;
    }
    if (name == L"CONDENSAR" || name == L"NUBES") {
        bool cloudFormed = animated;
        if (name == L"NUBES" && cloudFormed) {
            POINT mountain1[]{{cx - scale,cy + scale / 2},{cx - scale / 4,cy - scale},{cx + scale / 3,cy + scale / 2}};
            POINT mountain2[]{{cx - scale / 4,cy + scale / 2},{cx + scale / 3,cy - scale / 2},{cx + scale,cy + scale / 2}};
            POINT snow1[]{{cx - scale / 2,cy - scale / 2},{cx - scale / 4,cy - scale},{cx,cy - scale / 2},{cx - scale / 4,cy - scale / 4}};
            POINT snow2[]{{cx + scale / 12,cy - scale / 4},{cx + scale / 3,cy - scale / 2},{cx + scale / 2,cy - scale / 4},{cx + scale / 3,cy}};
            FilledPolygon(dc, mountain1, 3, RGB(137,177,211));
            FilledPolygon(dc, mountain2, 3, RGB(105,153,196));
            FilledPolygon(dc, snow1, 4, RGB(255,255,255));
            FilledPolygon(dc, snow2, 4, RGB(245,251,255));
        }
        if (cloudFormed) {
            for (int i = 0; i < 4; ++i) {
                int x = cx - 54 + i * 33;
                int y = cy + 32 - ((animationTick + i * 13) % 46);
                Circle(dc, x, y, 9, RGB(191,235,255), RGB(255,255,255));
            }
            Cloud(dc, cx, cy - 4, 125, RGB(255,255,255));
        } else {
            for (int i = 0; i < 3; ++i) {
                int x = cx - 30 + i * 30;
                int rise = (animationTick + i * 13) % 36;
                Line(dc, x, cy + 26 - rise, x, cy - rise, RGB(148,215,242), 3);
                Line(dc, x, cy - rise, x - 5, cy + 5 - rise, RGB(148,215,242), 3);
            }
        }
        if (name == L"NUBES" && cloudFormed) DrawSnowflake(dc, cx + 70, cy - 42, 16, animationTick * .05);
        return;
    }
    if (name == L"EL SOL") {
        int sunX = cx;
        Circle(dc, sunX, cy - 10, 42, RGB(255,215,72), RGB(255,190,54), 3);
        for (int i = 0; i < 8; ++i) {
            double a = i * PI / 4.0 + animationTick * .01;
            Line(dc, sunX + static_cast<int>(std::cos(a) * 53), cy - 10 + static_cast<int>(std::sin(a) * 53),
                 sunX + static_cast<int>(std::cos(a) * 67), cy - 10 + static_cast<int>(std::sin(a) * 67),
                 RGB(255,190,54), 4);
        }
        FillRectColor(dc, cx - scale, cy + 48, cx + scale, cy + 58, RGB(81,183,234));
        if (animated) {
            for (int i = 0; i < 3; ++i) {
                int x = cx - 28 + i * 28;
                int rise = (animationTick + i * 15) % 32;
                Line(dc, x, cy + 39 - rise, x, cy + 20 - rise, RGB(255,176,77), 3);
            }
        }
        return;
    }
    if (name == L"LLUVIA") {
        Cloud(dc, cx, cy - 24, 135, RGB(255,255,255));
        if (animated) {
            for (int i = 0; i < 6; ++i) {
                int x = cx - 52 + i * 21;
                int y = cy + 8 + ((animationTick * 5 + i * 16) % 65);
                Line(dc, x, y, x - 5, y + 13, RGB(82,179,240), 3);
            }
        }
        FillRectColor(dc, cx - 75, cy + 74, cx + 75, cy + 91, RGB(70,175,233));
        return;
    }
    if (name == L"AGUA") {
        DrawDrop(dc, cx, cy, 58, RGB(85,187,242), false);
        return;
    }
    Circle(dc, cx, cy, 42, RGB(255,213,80), RGB(255,255,255), 2);
}

void PlaySoundEffect(Action kind) {
    if (!soundEnabled) return;
    PlaySoundW(nullptr, nullptr, 0);
    constexpr unsigned sampleRate = 12000;
    const unsigned durationMs = kind == Action::Sun ? 800 : 390;
    const unsigned sampleCount = sampleRate * durationMs / 1000;
    soundWave.assign(44 + sampleCount * 2, 0);
    auto put16 = [](BYTE* p, std::uint16_t n) { p[0] = static_cast<BYTE>(n); p[1] = static_cast<BYTE>(n >> 8); };
    auto put32 = [](BYTE* p, std::uint32_t n) {
        p[0] = static_cast<BYTE>(n); p[1] = static_cast<BYTE>(n >> 8);
        p[2] = static_cast<BYTE>(n >> 16); p[3] = static_cast<BYTE>(n >> 24);
    };
    memcpy(soundWave.data(), "RIFF", 4);
    put32(soundWave.data() + 4, static_cast<std::uint32_t>(soundWave.size() - 8));
    memcpy(soundWave.data() + 8, "WAVEfmt ", 8);
    put32(soundWave.data() + 16, 16);
    put16(soundWave.data() + 20, 1);
    put16(soundWave.data() + 22, 1);
    put32(soundWave.data() + 24, sampleRate);
    put32(soundWave.data() + 28, sampleRate * 2);
    put16(soundWave.data() + 32, 2);
    put16(soundWave.data() + 34, 16);
    memcpy(soundWave.data() + 36, "data", 4);
    put32(soundWave.data() + 40, sampleCount * 2);

    double base = kind == Action::Cold ? 990.0 : kind == Action::Rain ? 420.0 :
                  kind == Action::Heat || kind == Action::Sun ? 680.0 : 760.0;
    const double melody[]{523.25, 659.25, 783.99, 1046.50};
    auto* samples = reinterpret_cast<std::int16_t*>(soundWave.data() + 44);
    for (unsigned i = 0; i < sampleCount; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        int note = std::min(3, static_cast<int>(t / 0.2));
        double noteTime = std::fmod(t, 0.2);
        double sweep = kind == Action::Cold ? 1.0 + 0.32 * std::sin(t * 8.0) :
                       kind == Action::Rain ? 1.0 + 0.16 * std::sin(t * 71.0) :
                       1.0 - 0.38 * t;
        double envelope = kind == Action::Sun
            ? std::min(1.0, noteTime * 35.0) * std::max(0.0, 1.0 - noteTime / 0.24)
            : std::min(1.0, t * 24.0) * std::max(0.0, 1.0 - t / (durationMs / 1000.0));
        double frequency = kind == Action::Sun ? melody[note] : base * sweep;
        samples[i] = static_cast<std::int16_t>(std::sin(2.0 * PI * frequency * t) * envelope * 5800.0);
    }
    PlaySoundW(reinterpret_cast<LPCWSTR>(soundWave.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

Rect ButtonRect(int x1, int y1, int x2, int y2) { return {x1, y1, x2, y2}; }

void DrawButton(HDC dc, Rect r, const wchar_t* label, COLORREF fill, bool enabled = true, int font = 23) {
    COLORREF color = enabled ? fill : RGB(195,208,218);
    RoundedBox(dc, r.left, r.top, r.right, r.bottom, color, RGB(255,255,255), 23, 2);
    Text(dc, label, {r.left + 8, r.top + 4, r.right - 8, r.bottom - 4}, font,
         enabled ? RGB(255,255,255) : RGB(235,239,241), DT_CENTER | DT_VCENTER | DT_WORDBREAK, true);
}

void DrawDecorations(HDC dc) {
    for (int i = 0; i < 9; ++i) {
        int x = (i * 147 + 68) % CANVAS_W;
        int y = 74 + (i * 91) % 610;
        int drift = (animationTick + i * 23) % 40 - 20;
        int radius = 4 + i % 6;
        Circle(dc, x + drift / 3, y - drift, radius, RGB(187,237,255), RGB(238,252,255), 1);
    }
}

int CountCompleted(int first, int last) {
    int count = 0;
    for (int i = first; i < last; ++i)
        if (completedChallenges[i]) ++count;
    return count;
}

void DrawHome(HDC dc) {
    Gradient(dc, RGB(116,204,247), RGB(241,251,255));
    DrawDecorations(dc);
    Circle(dc, 153, 144, 58, RGB(255,223,93), RGB(255,255,255), 2);
    for (int i = 0; i < 8; ++i) {
        double a = i * PI / 4;
        Line(dc, 153 + static_cast<int>(std::cos(a) * 70), 144 + static_cast<int>(std::sin(a) * 70),
             153 + static_cast<int>(std::cos(a) * 83), 144 + static_cast<int>(std::sin(a) * 83), RGB(255,194,72), 4);
    }
    Rainbow(dc, 270, 441, 220, 17);
    Cloud(dc, 240, 561, 176, RGB(255,255,255));
    Cloud(dc, 413, 623, 136, RGB(250,253,255));
    Cloud(dc, 72, 626, 130, RGB(250,253,255));
    int jump = static_cast<int>(std::sin(animationTick * 0.10) * 13);
    DrawDrop(dc, 286, 370 + jump, 82, RGB(86,192,245));
    Text(dc, L"¡Hola! Soy Lunita", {83, 494, 493, 531}, 23, RGB(43,110,159), DT_CENTER | DT_VCENTER, true);

    RoundedBox(dc, 553, 93, 1084, 671, RGB(255,255,255), RGB(219,244,253), 32, 3);
    Text(dc, L"LA AVENTURA", {597, 137, 1045, 191}, 39, RGB(38,110,172), DT_CENTER | DT_VCENTER, true);
    Text(dc, L"DE LA GOTA LUNITA", {583, 191, 1065, 253}, 39, RGB(50,167,205), DT_CENTER | DT_VCENTER, true);
    Text(dc, L"¡Acompaña a Lunita a descubrir el agua!", {588, 264, 1050, 307}, 20, RGB(78,112,140));
    DrawButton(dc, ButtonRect(661, 332, 976, 393), L"JUGAR", RGB(69,187,130), true, 26);
    DrawButton(dc, ButtonRect(661, 406, 976, 467), L"INSTRUCCIONES", RGB(76,161,225));
    DrawButton(dc, ButtonRect(661, 480, 976, 541), soundEnabled ? L"SONIDO: SÍ" : L"SONIDO: NO", RGB(157,125,212));
    DrawButton(dc, ButtonRect(661, 554, 976, 615), L"SALIR", RGB(243,126,115));
    Text(dc, L"Ríos · nieve · nubes · lluvia · ¡arcoíris!", {588, 631, 1050, 662}, 17, RGB(95,133,158));
}

void DrawInstructions(HDC dc) {
    Gradient(dc, RGB(123,209,246), RGB(244,251,255));
    RoundedBox(dc, 94, 58, 1026, 687, RGB(255,255,255), RGB(220,244,253), 30, 3);
    Text(dc, L"¡Cómo jugar con Lunita!", {188, 89, 930, 142}, 36, RGB(38,115,171), DT_CENTER | DT_VCENTER, true);
    const wchar_t* lines[]{
        L"1. Elige un mundo y ayuda a Lunita con sus desafíos.",
        L"2. Toca una respuesta o arrástrala al círculo brillante.",
        L"3. Cuando lo veas, pulsa CALOR, FRÍO, SOL o LLUVIA.",
        L"4. ¡Gana estrellas, aprende y celebra al completar todo!"
    };
    for (int i = 0; i < 4; ++i) {
        Circle(dc, 175, 206 + i * 77, 19, colors[(i + 1) % 8], colors[(i + 1) % 8]);
        Text(dc, std::to_wstring(i + 1).c_str(), {164, 193 + i * 77, 186, 218 + i * 77},
             16, RGB(255,255,255), DT_CENTER | DT_VCENTER, true);
        Text(dc, lines[i], {219, 176 + i * 77, 911, 233 + i * 77}, 21, RGB(52,88,120), DT_LEFT | DT_VCENTER);
    }
    DrawDrop(dc, 552, 535 + static_cast<int>(std::sin(animationTick * .08) * 8), 51, RGB(83,190,244));
    Text(dc, L"El agua puede ser líquida, hielo sólido o vapor invisible.\nLas nubes visibles están hechas de gotitas diminutas o cristales de hielo.",
         {226, 590, 895, 642}, 17, RGB(69,119,147));
    DrawButton(dc, ButtonRect(395, 648, 725, 697), L"VOLVER", RGB(70,163,218), true, 20);
}

void DrawLevels(HDC dc) {
    Gradient(dc, RGB(119,207,246), RGB(239,250,255));
    DrawDecorations(dc);
    Text(dc, L"¡Elige tu aventura!", {208, 62, 912, 117}, 37, RGB(42,113,166), DT_CENTER | DT_VCENTER, true);
    const int xs[]{81, 406, 731};
    const COLORREF fills[]{RGB(80,183,208), RGB(132,170,231), RGB(235,171,83)};
    const wchar_t* names[]{L"1. Los estados\n del agua", L"2. ¡Se transforma!", L"3. El viaje del agua"};
    const wchar_t* details[]{L"Líquida · sólida · gaseosa", L"Derretir · congelar\nEvaporar · condensar", L"Sol · nubes · lluvia"};
    for (int i = 0; i < 3; ++i) {
        bool unlocked = i == 0 || (i == 1 && CountCompleted(0, 3) == 3) ||
                        (i == 2 && CountCompleted(0, 7) == 7);
        RoundedBox(dc, xs[i], 191, xs[i] + 298, 496, RGB(255,255,255),
                   unlocked ? RGB(255,255,255) : RGB(202,214,224), 26, 3);
        DrawSceneArt(dc, i == 0 ? L"RÍO" : i == 1 ? L"CONGELAR" : L"EL SOL", xs[i] + 148, 286, 66, true);
        Text(dc, names[i], {xs[i] + 17, 347, xs[i] + 281, 407}, 22,
             unlocked ? RGB(47,109,155) : RGB(157,170,180), DT_CENTER | DT_VCENTER, true);
        Text(dc, details[i], {xs[i] + 14, 412, xs[i] + 284, 459}, 15, RGB(99,131,152));
        int done = i == 0 ? CountCompleted(0, 3) :
                   i == 1 ? CountCompleted(3, 7) : CountCompleted(7, 10);
        std::wstring progress = std::to_wstring(done) + (i == 1 ? L" / 4 desafíos" : L" / 3 desafíos");
        Text(dc, progress.c_str(), {xs[i] + 18, 464, xs[i] + 280, 487}, 14, RGB(73,145,116), DT_CENTER | DT_VCENTER, true);
        DrawButton(dc, ButtonRect(xs[i] + 27, 520, xs[i] + 271, 579),
                   unlocked ? L"¡A JUGAR!" : L"¡SIGUE APRENDIENDO!", fills[i], unlocked, 18);
    }
    Text(dc, L"Cada reto te enseña algo nuevo. ¡Puedes volver a visitar los mundos!", {160, 602, 960, 640}, 18,
         RGB(74,120,146));
    DrawButton(dc, ButtonRect(389, 654, 731, 704), L"VOLVER AL INICIO", RGB(91,159,211), true, 18);
}

void DrawChallenge(HDC dc) {
    const Challenge& c = challenges[currentChallenge];
    Gradient(dc, RGB(138,216,246), RGB(242,251,255));
    DrawDecorations(dc);
    RoundedBox(dc, 42, 34, 1078, 128, RGB(255,255,255), RGB(220,244,253), 23, 2);
    Text(dc, c.title, {69, 46, 650, 94}, 29, RGB(37,111,167), DT_LEFT | DT_VCENTER, true);
    Text(dc, L"ESTRELLAS", {801, 51, 920, 83}, 16, RGB(76,118,146), DT_CENTER | DT_VCENTER, true);
    int firstInLevel = currentChallenge < 3 ? 0 : currentChallenge < 7 ? 3 : 7;
    int starsInLevel = currentChallenge >= 3 && currentChallenge < 7 ? 4 : 3;
    for (int i = 0; i < starsInLevel; ++i) {
        bool earned = CountCompleted(firstInLevel, firstInLevel + starsInLevel) > i;
        const int sx = starsInLevel == 4 ? 928 + i * 34 : 940 + i * 38;
        Text(dc, earned ? L"★" : L"☆", {sx, 45, sx + 34, 87}, 29,
             earned ? RGB(255,186,48) : RGB(184,206,218), DT_CENTER | DT_VCENTER, true);
    }
    int progressWidth = static_cast<int>((currentChallenge + (actionActivated ? .55 : .0)) * 906.0 / challenges.size());
    RoundedBox(dc, 70, 101, 1050, 113, RGB(231,243,249), RGB(231,243,249), 10, 1);
    if (progressWidth > 0) RoundedBox(dc, 70, 101, 70 + progressWidth, 113, RGB(86,195,150), RGB(86,195,150), 10, 1);

    Text(dc, c.question, {69, 144, 1051, 190}, 25, RGB(42,87,126), DT_CENTER | DT_VCENTER, true);
    RoundedBox(dc, 66, 199, 1054, 475, RGB(255,255,255), RGB(227,244,252), 28, 2);
    DrawSceneArt(dc, c.illustration, 300, 333, 62, true);
    Text(dc, c.explanation, {499, 223, 968, 326}, 20, RGB(53,94,125), DT_LEFT | DT_VCENTER);
    if (c.action != Action::None) {
        DrawButton(dc, ButtonRect(539, 369, 810, 430), c.actionLabel, actionActivated ? RGB(88,193,139) : RGB(244,169,64), true, 19);
        Text(dc, actionActivated ? L"¡Bien hecho! Ya puedes elegir la respuesta." : c.actionHint,
             {503, 431, 827, 468}, 14, actionActivated ? RGB(57,147,105) : RGB(99,133,153), DT_CENTER | DT_VCENTER);
    } else {
        Text(dc, c.actionHint, {503, 369, 827, 427}, 17, RGB(76,130,156), DT_CENTER | DT_VCENTER, true);
        Text(dc, L"Toca una tarjeta o arrástrala hasta el círculo.", {503, 425, 827, 463}, 14,
             RGB(107,139,157), DT_CENTER | DT_VCENTER);
    }
    int tx = 963, ty = 410;
    Circle(dc, tx, ty, 39, RGB(241,250,255), RGB(104,193,229), 3);
    Text(dc, dragging ? L"¡SUELTA!" : L"AQUÍ", {tx - 34, ty - 13, tx + 34, ty + 13}, 14,
         RGB(48,137,184), DT_CENTER | DT_VCENTER, true);

    for (int i = 0; i < 3; ++i) {
        const int left = 55 + i * 340;
        COLORREF fill = lastAnswer == i ? (i == c.correct ? RGB(224,249,229) : RGB(255,235,229)) : RGB(255,255,255);
        COLORREF border = lastAnswer == i ? (i == c.correct ? RGB(83,194,127) : RGB(236,135,119)) : RGB(220,240,249);
        RoundedBox(dc, left, 497, left + 324, 591, fill, border, 18, 3);
        Circle(dc, left + 33, 544, 17, colors[i + 1], colors[i + 1]);
        Text(dc, std::to_wstring(i + 1).c_str(), {left + 23, 534, left + 43, 554}, 15,
             RGB(255,255,255), DT_CENTER | DT_VCENTER, true);
        Text(dc, c.answers[i], {left + 60, 506, left + 312, 582}, 17, RGB(52,87,116), DT_LEFT | DT_VCENTER, true);
    }
    if (!feedback.empty()) {
        Text(dc, feedback.c_str(), {112, 600, 1008, 641}, 17,
             lastAnswer == c.correct ? RGB(53,148,98) : RGB(197,102,80), DT_CENTER | DT_VCENTER, true);
    } else {
        Text(dc, L"¡Cada intento te ayuda a aprender!", {160, 604, 960, 638}, 16, RGB(97,136,157));
    }
    DrawButton(dc, ButtonRect(61, 663, 237, 715), L"← MUNDOS", RGB(109,151,184), true, 17);
    DrawButton(dc, ButtonRect(905, 663, 1060, 715), L"INICIO", RGB(109,151,184), true, 17);
    if (dragging && draggedAnswer >= 0) {
        RoundedBox(dc, cursorPoint.x - 115, cursorPoint.y - 23, cursorPoint.x + 115, cursorPoint.y + 23,
                   RGB(255,255,255), RGB(92,180,220), 16, 2);
        Text(dc, c.answers[draggedAnswer], {cursorPoint.x - 108, cursorPoint.y - 19, cursorPoint.x + 108, cursorPoint.y + 19},
             16, RGB(52,87,116), DT_CENTER | DT_VCENTER, true);
    }
}

void DrawVictory(HDC dc) {
    Gradient(dc, RGB(109,204,247), RGB(232,247,255));
    for (int i = 0; i < 30; ++i) {
        int x = (i * 193 + 31) % CANVAS_W;
        int y = (i * 79 + celebrationTicks * (2 + i % 3)) % CANVAS_H;
        Circle(dc, x, y, 5 + i % 8, colors[i % 8], colors[i % 8]);
    }
    Rainbow(dc, 560, 414, 304, 24);
    Cloud(dc, 159, 589, 167, RGB(255,255,255));
    Cloud(dc, 940, 589, 167, RGB(255,255,255));
    DrawDrop(dc, 557, 467 + static_cast<int>(std::sin(animationTick * .12) * 10), 83, RGB(81,190,243));
    Text(dc, L"¡LO LOGRASTE!", {252, 70, 868, 132}, 43, RGB(42,117,170), DT_CENTER | DT_VCENTER, true);
    Text(dc, L"Lunita descubrió el ciclo del agua contigo.", {227, 142, 893, 190}, 24, RGB(57,121,158));
    Text(dc, L"El agua cambia y viaja… ¡y el ciclo vuelve a empezar!", {169, 589, 951, 630}, 19, RGB(62,116,151));
    Text(dc, L"★   ★   ★   ★   ★   ★   ★   ★   ★   ★", {242, 217, 878, 265}, 30, RGB(255,190,56),
         DT_CENTER | DT_VCENTER, true);
    DrawButton(dc, ButtonRect(380, 655, 740, 713), L"¡OTRA AVENTURA!", RGB(76,183,132), true, 20);
}

void Paint(HDC target) {
    HDC buffer = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, CANVAS_W, CANVAS_H);
    HGDIOBJ oldBitmap = SelectObject(buffer, bitmap);
    switch (screen) {
        case Screen::Home: DrawHome(buffer); break;
        case Screen::Instructions: DrawInstructions(buffer); break;
        case Screen::Levels: DrawLevels(buffer); break;
        case Screen::Challenge: DrawChallenge(buffer); break;
        case Screen::Victory: DrawVictory(buffer); break;
    }
    SetStretchBltMode(target, HALFTONE);
    StretchBlt(target, 0, 0, CANVAS_W, CANVAS_H, buffer, 0, 0, CANVAS_W, CANVAS_H, SRCCOPY);
    SelectObject(buffer, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
}

bool Contains(Rect r, POINT p) {
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

void ResetChallenge() {
    KillTimer(mainWindow, ADVANCE_TIMER);
    answerAdvancePending = false;
    actionActivated = false;
    lastAnswer = -1;
    feedback.clear();
    dragging = false;
    draggedAnswer = -1;
}

void StartChallenge(int index) {
    currentChallenge = index;
    ResetChallenge();
    screen = Screen::Challenge;
}

void DoAction() {
    const Challenge& c = challenges[currentChallenge];
    if (c.action == Action::None || answerAdvancePending) return;
    actionActivated = true;
    feedback = L"¡Buen trabajo! Ahora piensa qué cambió.";
    PlaySoundEffect(c.action);
    InvalidateRect(mainWindow, nullptr, FALSE);
}

void AdvanceAfterCorrectAnswer() {
    answerAdvancePending = false;
    KillTimer(mainWindow, ADVANCE_TIMER);
    if (std::all_of(completedChallenges.begin(), completedChallenges.end(), [](bool done) { return done; })) {
        screen = Screen::Victory;
        PlaySoundEffect(Action::Sun);
        return;
    }

    if (currentChallenge + 1 < static_cast<int>(challenges.size())) {
        StartChallenge(currentChallenge + 1);
        return;
    }

    MessageBoxW(mainWindow, L"No se pudo determinar el siguiente desafío.", L"LA AVENTURA DE LUNITA",
                MB_OK | MB_ICONERROR);
    screen = Screen::Levels;
}

void Answer(int index) {
    if (answerAdvancePending || index < 0 || index >= static_cast<int>(challenges[0].answers.size())) return;
    const Challenge& c = challenges[currentChallenge];
    lastAnswer = index;
    if (c.action != Action::None && !actionActivated) {
        feedback = L"¡Primero pulsa el botón de la aventura!";
        PlaySoundEffect(Action::None);
    } else if (index == c.correct) {
        completedChallenges[currentChallenge] = true;
        feedback = L"¡Correcto, exploradora o explorador! ¡Una estrella para ti!";
        PlaySoundEffect(Action::Heat);
        answerAdvancePending = true;
        if (SetTimer(mainWindow, ADVANCE_TIMER, ADVANCE_DELAY_MS, nullptr) == 0) {
            MessageBoxW(mainWindow, L"No se pudo iniciar el avance automático. Se mostrará el siguiente desafío ahora.",
                        L"LA AVENTURA DE LUNITA", MB_OK | MB_ICONWARNING);
            AdvanceAfterCorrectAnswer();
        }
    } else {
        feedback = L"¡Casi! Mira la pista y vuelve a intentarlo. ¡Tú puedes!";
        PlaySoundEffect(Action::Cold);
    }
    InvalidateRect(mainWindow, nullptr, FALSE);
}

void ActivateLevel(int level) {
    const int starts[]{0, 3, 7};
    const int ends[]{3, 7, 10};
    const Rect buttons[]{{81, 520, 379, 580}, {406, 520, 704, 580}, {731, 520, 1029, 580}};
    bool unlocked = level == 0 || (level == 1 && CountCompleted(0, 3) == 3) ||
                    (level == 2 && CountCompleted(0, 7) == 7);
    if (!unlocked || !Contains(buttons[level], cursorPoint)) return;
    for (int index = starts[level]; index < ends[level]; ++index) {
        if (!completedChallenges[index]) {
            StartChallenge(index);
            return;
        }
    }
    StartChallenge(starts[level]);
}

void HandleClick(POINT p) {
    cursorPoint = p;
    switch (screen) {
        case Screen::Home:
            if (Contains({661, 332, 976, 393}, p)) screen = Screen::Levels;
            else if (Contains({661, 406, 976, 467}, p)) screen = Screen::Instructions;
            else if (Contains({661, 480, 976, 541}, p)) {
                soundEnabled = !soundEnabled;
                if (!soundEnabled) PlaySoundW(nullptr, nullptr, 0);
            }
            else if (Contains({661, 554, 976, 615}, p)) PostMessageW(mainWindow, WM_CLOSE, 0, 0);
            break;
        case Screen::Instructions:
            if (Contains({395, 648, 725, 697}, p)) screen = Screen::Home;
            break;
        case Screen::Levels:
            if (Contains({389, 654, 731, 704}, p)) screen = Screen::Home;
            else {
                ActivateLevel(0);
                ActivateLevel(1);
                ActivateLevel(2);
            }
            break;
        case Screen::Challenge:
            if (Contains({61, 663, 237, 715}, p)) {
                ResetChallenge();
                screen = Screen::Levels;
                break;
            }
            if (Contains({905, 663, 1060, 715}, p)) {
                ResetChallenge();
                screen = Screen::Home;
                break;
            }
            if (challenges[currentChallenge].action != Action::None &&
                Contains({539, 369, 810, 430}, p)) { DoAction(); break; }
            for (int i = 0; i < 3; ++i) {
                int left = 55 + i * 340;
                if (Contains({left, 497, left + 324, 591}, p)) { Answer(i); return; }
            }
            break;
        case Screen::Victory:
            if (Contains({380, 655, 740, 713}, p)) {
                ResetChallenge();
                completedChallenges.fill(false);
                screen = Screen::Levels;
            }
            break;
    }
    InvalidateRect(mainWindow, nullptr, FALSE);
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            SetTimer(window, ANIMATION_TIMER, 40, nullptr);
            return 0;
        case WM_TIMER:
            if (wParam == ADVANCE_TIMER) {
                if (answerAdvancePending) AdvanceAfterCorrectAnswer();
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (wParam == ANIMATION_TIMER) {
                ++animationTick;
                if (screen == Screen::Victory) ++celebrationTicks;
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONDOWN: {
            POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT client{};
            GetClientRect(window, &client);
            p.x = p.x * CANVAS_W / std::max(1L, client.right);
            p.y = p.y * CANVAS_H / std::max(1L, client.bottom);
            cursorPoint = p;
            if (screen == Screen::Challenge) {
                for (int i = 0; i < 3; ++i) {
                    int left = 55 + i * 340;
                    if (Contains({left, 497, left + 324, 591}, p)) {
                        dragging = true;
                        draggedAnswer = i;
                        dragStartPoint = p;
                        SetCapture(window);
                        return 0;
                    }
                }
            }
            HandleClick(p);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (dragging) {
                RECT client{};
                GetClientRect(window, &client);
                cursorPoint = {GET_X_LPARAM(lParam) * CANVAS_W / std::max(1L, client.right),
                               GET_Y_LPARAM(lParam) * CANVAS_H / std::max(1L, client.bottom)};
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            if (dragging) {
                RECT client{};
                GetClientRect(window, &client);
                POINT p{GET_X_LPARAM(lParam) * CANVAS_W / std::max(1L, client.right),
                        GET_Y_LPARAM(lParam) * CANVAS_H / std::max(1L, client.bottom)};
                int answer = draggedAnswer;
                bool tapped = std::abs(p.x - dragStartPoint.x) < 8 &&
                              std::abs(p.y - dragStartPoint.y) < 8;
                dragging = false;
                draggedAnswer = -1;
                ReleaseCapture();
                if (tapped || Contains({922, 367, 1004, 453}, p)) Answer(answer);
                else InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            Paint(dc);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 930;
            info->ptMinTrackSize.y = 675;
            return 0;
        }
        case WM_DESTROY:
            KillTimer(window, ANIMATION_TIMER);
            KillTimer(window, ADVANCE_TIMER);
            PlaySoundW(nullptr, nullptr, 0);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    appInstance = instance;
    const wchar_t CLASS_NAME[] = L"LunitaWaterCycleGame";
    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = appInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    if (!RegisterClassW(&wc)) return 1;
    mainWindow = CreateWindowExW(0, CLASS_NAME, L"LA AVENTURA DE LA GOTA LUNITA",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CANVAS_W, CANVAS_H,
        nullptr, nullptr, appInstance, nullptr);
    if (!mainWindow) return 1;
    ShowWindow(mainWindow, showCommand);
    UpdateWindow(mainWindow);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
