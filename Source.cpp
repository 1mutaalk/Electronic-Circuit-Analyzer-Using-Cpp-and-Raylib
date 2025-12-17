/********************************************************************
 * Electronic Circuit Analyzer - CS250 Mini Project (Group 13)
 * GUI with raylib + VISUAL CIRCUIT DIAGRAMS + IMPEDANCE
 *
 * LIGHT TEAL/ORANGE THEME VERSION
 ********************************************************************/

#include "raylib.h"
#include <list>
#include <vector>
#include <queue>
#include <stack>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <complex>   // for complex impedance [web:50]

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Font customFont;

// ---------------------- Data Structures ---------------------------

enum class ComponentType { RESISTOR = 0, CAPACITOR = 1, INDUCTOR = 2 };
enum class CircuitType { SERIES = 0, PARALLEL = 1 };

struct Component {
    int id;
    ComponentType type;
    double value;
    CircuitType circuitType;

    Component(int _id, ComponentType _t, double _v, CircuitType _ct)
        : id(_id), type(_t), value(_v), circuitType(_ct) {
    }
};

struct Operation {
    std::string description;
};

std::list<Component> componentsData;
std::list<int> seriesCircuit;
std::list<int> parallelCircuit;

std::stack<std::list<Component> > undoStack;
std::queue<Operation> opQueue;
int nextId = 1;
Component* searchedComponent = NULL;
double analysisFrequencyHz = 50.0;

// complex type alias
using cd = std::complex<double>;
const cd J(0.0, 1.0);

// ---------------------- Utility -------------------------

Color MakeColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    Color c{ r,g,b,a };
    return c;
}

unsigned char ClampU8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

std::string TypeToString(ComponentType t) {
    if (t == ComponentType::RESISTOR) return "Resistor";
    if (t == ComponentType::CAPACITOR) return "Capacitor";
    return "Inductor";
}

// component accent colors stay similar
Color TypeColor(ComponentType t) {
    if (t == ComponentType::RESISTOR) return MakeColor(239, 83, 80, 255);      // soft red
    if (t == ComponentType::CAPACITOR) return MakeColor(100, 181, 246, 255);   // blue
    return MakeColor(129, 199, 132, 255);                                      // green
}

void DrawTextEx_Custom(const std::string& text, float posX, float posY, int fontSize, Color color) {
    DrawTextEx(customFont, text.c_str(), Vector2{ posX, posY }, (float)fontSize, 1.0f, color);
}

// ---------------------- Calculations -------------------------

// simple real R (used for resistance summaries)
double CalcImpedanceR(double R) { return R; }

double CalcImpedanceL(double L, double freqHz) {
    double omega = 2.0 * M_PI * freqHz;
    return omega * L;
}

double CalcImpedanceC(double C, double freqHz) {
    double omega = 2.0 * M_PI * freqHz;
    if (C == 0.0) return 1e10;
    return 1.0 / (omega * C);
}

// complex impedance for each component: R, jωL, -j/(ωC) [web:4][web:5]
cd GetComponentImpedanceComplex(Component* c, double freqHz) {
    if (!c) return cd(0.0, 0.0);
    if (c->type == ComponentType::RESISTOR) {
        return cd(c->value, 0.0);
    }
    double omega = 2.0 * M_PI * freqHz;
    if (c->type == ComponentType::INDUCTOR) {
        return J * omega * c->value;          // jωL
    }
    // capacitor
    if (c->value == 0.0) return cd(1e10, 0.0); // open circuit
    return -J / (omega * c->value);            // -j/(ωC)
}

// original real helper kept (not used for |Z| now)
double GetComponentImpedance(Component* c, double freqHz) {
    if (!c) return 0.0;
    if (c->type == ComponentType::RESISTOR) return CalcImpedanceR(c->value);
    if (c->type == ComponentType::CAPACITOR) return CalcImpedanceC(c->value, freqHz);
    return CalcImpedanceL(c->value, freqHz);
}

void PushSnapshot(const std::string& desc) {
    undoStack.push(componentsData);
    Operation op{ desc };
    opQueue.push(op);
    if (opQueue.size() > 20) opQueue.pop();
}

void AddComponent(ComponentType type, double value, CircuitType circuit) {
    componentsData.push_back(Component(nextId, type, value, circuit));
    if (circuit == CircuitType::SERIES) seriesCircuit.push_back(nextId);
    else parallelCircuit.push_back(nextId);

    std::stringstream ss;
    ss << "Added " << TypeToString(type) << " to "
        << (circuit == CircuitType::SERIES ? "SERIES" : "PARALLEL")
        << " circuit (ID=" << nextId << ", value=" << value << ")";
    PushSnapshot(ss.str());
    nextId++;
}

bool RemoveComponent(int id) {
    for (auto it = componentsData.begin(); it != componentsData.end(); ++it) {
        if (it->id == id) {
            seriesCircuit.remove(id);
            parallelCircuit.remove(id);
            std::stringstream ss;
            ss << "Removed component ID=" << id;
            PushSnapshot(ss.str());
            componentsData.erase(it);
            return true;
        }
    }
    return false;
}

Component* FindComponent(int id) {
    for (auto it = componentsData.begin(); it != componentsData.end(); ++it) {
        if (it->id == id) return &(*it);
    }
    return NULL;
}

void Undo() {
    if (!undoStack.empty()) {
        componentsData = undoStack.top();
        undoStack.pop();
        seriesCircuit.clear();
        parallelCircuit.clear();
        for (auto it = componentsData.begin(); it != componentsData.end(); ++it) {
            if (it->circuitType == CircuitType::SERIES) seriesCircuit.push_back(it->id);
            else parallelCircuit.push_back(it->id);
        }
    }
}

double CalcSeries(const std::list<int>& ids) {
    double sum = 0.0;
    for (int id : ids) {
        Component* c = FindComponent(id);
        if (c && c->type == ComponentType::RESISTOR) sum += c->value;
    }
    return sum;
}

double CalcParallel(const std::list<int>& ids) {
    double inv = 0.0;
    for (int id : ids) {
        Component* c = FindComponent(id);
        if (c && c->type == ComponentType::RESISTOR && c->value != 0.0) {
            inv += 1.0 / c->value;
        }
    }
    if (inv == 0.0) return 0.0;
    return 1.0 / inv;
}

// series/parallel impedance magnitudes using complex math [web:4][web:5]
double CalcSeriesImpedance(const std::list<int>& ids, double freqHz) {
    cd sum(0.0, 0.0);
    for (int id : ids) {
        Component* c = FindComponent(id);
        if (c) sum += GetComponentImpedanceComplex(c, freqHz);
    }
    return std::abs(sum);
}

double CalcParallelImpedance(const std::list<int>& ids, double freqHz) {
    cd inv(0.0, 0.0);
    for (int id : ids) {
        Component* c = FindComponent(id);
        if (c) {
            cd z = GetComponentImpedanceComplex(c, freqHz);
            if (z != cd(0.0, 0.0)) inv += cd(1.0, 0.0) / z;
        }
    }
    if (inv == cd(0.0, 0.0)) return 0.0;
    cd ztot = cd(1.0, 0.0) / inv;
    return std::abs(ztot);
}

// ---------------------- Symbols -------------------------

void DrawResistorSymbol(float x, float y, float size, Color color) {
    float w = size * 2.5f;
    float h = size * 0.8f;
    float zigHeight = h / 2.0f;

    DrawLine((int)(x - size), (int)y, (int)x, (int)y, color);

    float segWidth = w / 6.0f;
    for (int i = 0; i < 6; ++i) {
        float x1 = x + i * segWidth;
        float x2 = x1 + segWidth / 2.0f;
        float x3 = x2 + segWidth / 2.0f;

        float y1 = (i % 2 == 0) ? y - zigHeight : y + zigHeight;
        float y2 = (i % 2 == 0) ? y + zigHeight : y - zigHeight;

        DrawLine((int)x1, (int)y, (int)x2, (int)y1, color);
        DrawLine((int)x2, (int)y1, (int)x3, (int)y2, color);
    }

    DrawLine((int)(x + w), (int)y, (int)(x + w + size), (int)y, color);
}

void DrawCapacitorSymbol(float x, float y, float size, Color color) {
    float gap = size * 0.5f;
    DrawLine((int)(x - size), (int)y, (int)x, (int)y, color);

    Vector2 p1 = { x, y - size };
    Vector2 p2 = { x, y + size };
    DrawLineEx(p1, p2, 3.0f, color);

    Vector2 p3 = { x + gap, y - size };
    Vector2 p4 = { x + gap, y + size };
    DrawLineEx(p3, p4, 3.0f, color);

    DrawLine((int)(x + gap), (int)y, (int)(x + gap + size), (int)y, color);
}

void DrawInductorSymbol(float x, float y, float size, Color color) {
    float coilRadius = size * 0.4f;
    float spacing = size * 0.6f;

    DrawLine((int)(x - size), (int)y, (int)x, (int)y, color);

    for (int i = 0; i < 4; ++i) {
        float cx = x + i * spacing;
        DrawCircle((int)cx, (int)y, (int)coilRadius, color);
        DrawCircle((int)cx, (int)y, (int)(coilRadius - 2.0f), MakeColor(230, 245, 245, 255));
    }

    DrawLine((int)(x + size * 3.0f), (int)y, (int)(x + size * 4.0f), (int)y, color);
}

void DrawComponentSymbol(ComponentType type, float x, float y, float size, Color color) {
    if (type == ComponentType::RESISTOR) DrawResistorSymbol(x, y, size, color);
    else if (type == ComponentType::CAPACITOR) DrawCapacitorSymbol(x, y, size, color);
    else DrawInductorSymbol(x, y, size, color);
}

// ---------------------- Diagrams -------------------------

void DrawSeriesCircuitDiagram(float startX, float startY, float maxWidth) {
    if (seriesCircuit.empty()) return;

    // how much to shift the CIRCUIT (not the title)
    const float offsetX = 350.0f;   // right
    const float offsetY = 20.0f;    // down

    float currentX = startX + offsetX;
    float currentY = startY + offsetY;
    float lineHeight = 70.0f;
    float compSpacing = 80.0f;

    // title stays in original place
    DrawTextEx(customFont, "SERIES CIRCUIT",
        Vector2{ startX, startY - 30.0f },
        18.0f, 0.7f, MakeColor(0, 150, 136, 255));

    for (auto it = seriesCircuit.begin(); it != seriesCircuit.end(); ++it) {
        Component* c = FindComponent(*it);
        if (!c) continue;

        // wrap, keeping same shifted origin
        if (currentX + compSpacing > startX + maxWidth) {
            currentX = startX + offsetX;
            currentY += lineHeight;
        }

        // shifted component
        DrawComponentSymbol(c->type, currentX, currentY, 10.0f, TypeColor(c->type));

        // labels move with component
        DrawTextEx(customFont, TextFormat("ID:%d", c->id),
            Vector2{ currentX - 10.0f, currentY - 18.0f },
            11.0f, 1.0f, MakeColor(45, 55, 72, 255));
        DrawTextEx(customFont, TextFormat("%.2f", c->value),
            Vector2{ currentX - 10.0f, currentY + 18.0f },
            10.0f, 1.0f, MakeColor(100, 110, 130, 255));

        // shifted wires
        if (it != seriesCircuit.begin()) {
            DrawLine((int)(currentX - 40.0f), (int)currentY,
                (int)(currentX - 15.0f), (int)currentY,
                MakeColor(160, 174, 192, 255));
        }
        if (std::next(it) != seriesCircuit.end()) {
            DrawLine((int)(currentX + 35.0f), (int)currentY,
                (int)(currentX + 55.0f), (int)currentY,
                MakeColor(160, 174, 192, 255));
        }

        currentX += compSpacing;
    }
}


void DrawParallelCircuitDiagram(float startX, float startY, float maxWidth) {
    if (parallelCircuit.empty()) return;

    // how much to shift the circuit (not the title)
    const float offsetX = 350.0f;   // right
    const float offsetY = 20.0f;    // down

    float topRailY = startY - 20.0f + offsetY;
    float branchSpacing = 60.0f;
    float bottomRailY = startY + (float)parallelCircuit.size() * branchSpacing + 20.0f + offsetY;

    // title stays at original position
    DrawTextEx(customFont, "PARALLEL CIRCUIT",
        Vector2{ startX, startY - 40.0f },
        18.0f, 1.0f, MakeColor(255, 152, 0, 255));

    // shifted left and right rails
    DrawLine((int)(startX + offsetX), (int)topRailY,
        (int)(startX + offsetX + 80.0f), (int)topRailY, MakeColor(160, 174, 192, 255));
    DrawLine((int)(startX + offsetX), (int)bottomRailY,
        (int)(startX + offsetX + 80.0f), (int)bottomRailY, MakeColor(160, 174, 192, 255));
    DrawLine((int)(startX + offsetX + 80.0f), (int)topRailY,
        (int)(startX + offsetX + 80.0f), (int)bottomRailY, MakeColor(160, 174, 192, 255));

    float branchY = topRailY + branchSpacing;
    for (auto it = parallelCircuit.begin(); it != parallelCircuit.end(); ++it) {
        Component* c = FindComponent(*it);
        if (!c) continue;

        // horizontal wire from left rail to component
        DrawLine((int)(startX + offsetX + 80.0f), (int)branchY,
            (int)(startX + offsetX + 110.0f), (int)branchY,
            MakeColor(160, 174, 192, 255));

        // component (shifted)
        float compX = startX + offsetX + 140.0f;
        DrawComponentSymbol(c->type, compX, branchY, 10.0f, TypeColor(c->type));

        // horizontal wire from component to right vertical
        DrawLine((int)(startX + offsetX + 170.0f), (int)branchY,
            (int)(startX + offsetX + 220.0f), (int)branchY,
            MakeColor(160, 174, 192, 255));

        // vertical down to bottom rail
        DrawLine((int)(startX + offsetX + 220.0f), (int)branchY,
            (int)(startX + offsetX + 220.0f), (int)bottomRailY,
            MakeColor(160, 174, 192, 255));

        // labels (shift with circuit)
        DrawTextEx(customFont, TextFormat("ID:%d", c->id),
            Vector2{ startX + offsetX + 235.0f, branchY - 8.0f },
            11.0f, 1.0f, MakeColor(45, 55, 72, 255));
        DrawTextEx(customFont, TextFormat("%.2f", c->value),
            Vector2{ startX + offsetX + 235.0f, branchY + 6.0f },
            10.0f, 1.0f, MakeColor(100, 110, 130, 255));

        branchY += branchSpacing;
    }

    // right vertical and exit wires (shifted)
    DrawLine((int)(startX + offsetX + 220.0f), (int)topRailY,
        (int)(startX + offsetX + 220.0f), (int)bottomRailY,
        MakeColor(160, 174, 192, 255));
    DrawLine((int)(startX + offsetX + 220.0f), (int)topRailY,
        (int)(startX + offsetX + 280.0f), (int)topRailY,
        MakeColor(160, 174, 192, 255));
    DrawLine((int)(startX + offsetX + 220.0f), (int)bottomRailY,
        (int)(startX + offsetX + 280.0f), (int)bottomRailY,
        MakeColor(160, 174, 192, 255));
}


// ---------------------- GUI State -------------------------

enum class ScreenState {
    MAIN_MENU,
    ADD_COMPONENT,
    REMOVE_COMPONENT,
    SEARCH_COMPONENT,
    DISPLAY_ALL,
    CALC_RESISTANCE
};

ScreenState currentScreen = ScreenState::MAIN_MENU;
std::string textBuffer = "";
ComponentType addType = ComponentType::RESISTOR;
CircuitType addCircuit = CircuitType::SERIES;
std::string statusMessage = "";
std::vector<int> selectedIds;

// ---------------------- Helpers -------------------------

void HandleTextInput(std::string& buf, int maxLen) {
    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && (int)buf.size() < maxLen) buf.push_back((char)key);
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !buf.empty()) buf.pop_back();
}

double StringToDoubleSafe(const std::string& s, bool& ok) {
    ok = false;
    if (s.empty()) return 0.0;
    try { double v = std::stod(s); ok = true; return v; }
    catch (...) { return 0.0; }
}

int StringToIntSafe(const std::string& s, bool& ok) {
    ok = false;
    if (s.empty()) return 0;
    try { int v = std::stoi(s); ok = true; return v; }
    catch (...) { return 0; }
}

void DrawGradientBackground(int w, int h) {
    for (int y = 0; y < h; ++y) {
        float t = (float)y / (float)h;
        Color c = MakeColor(
            (unsigned char)(245 - 20 * t),
            (unsigned char)(250 - 30 * t),
            (unsigned char)(255 - 40 * t),
            255
        );
        DrawLine(0, y, w, y, c);
    }
}

void DrawGlassPanel(Rectangle r, Color tint) {
    Color fill = MakeColor(tint.r, tint.g, tint.b, 40);
    Color border = MakeColor(tint.r, tint.g, tint.b, 140);
    DrawRectangleRounded(r, 0.1f, 16, fill);
    DrawRectangleRoundedLines(r, 0.1f, 16, border);
}

void DrawButtonEx(Rectangle r, const char* label, bool hovered, Color baseColor) {
    Color fill = baseColor;
    if (hovered) {
        fill.r = ClampU8(baseColor.r + 25);
        fill.g = ClampU8(baseColor.g + 25);
        fill.b = ClampU8(baseColor.b + 25);
    }

    DrawRectangleRounded(r, 0.3f, 12, fill);
    DrawRectangleRoundedLines(r, 0.3f, 12, MakeColor(255, 255, 255, 80));

    Vector2 textSize = MeasureTextEx(customFont, label, 16.0f, 1.0f);
    DrawTextEx(customFont, label,
        Vector2{ r.x + (r.width - textSize.x) / 2.0f, r.y + (r.height - 16.0f) / 2.0f },
        16.0f,
        1.0f,
        MakeColor(250, 252, 255, 255));
}

void DrawCommonTopBar(int w, const char* title) {
    DrawRectangle(0, 0, w, 70, MakeColor(0, 150, 136, 255));
    DrawRectangleLines(0, 70, w, 2, MakeColor(0, 121, 107, 255));
    DrawTextEx(customFont, title, Vector2{ 25, 22 }, 28.0f, 1.0f, MakeColor(250, 252, 255, 255));
}

void DrawBackButton() {
    Rectangle back = { 20.0f, 85.0f, 100.0f, 35.0f };
    Vector2 m = GetMousePosition();
    bool hover = CheckCollisionPointRec(m, back);
    DrawButtonEx(back, "Back", hover, MakeColor(0, 150, 136, 220));
    if (hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        currentScreen = ScreenState::MAIN_MENU;
        textBuffer.clear();
        statusMessage.clear();
        selectedIds.clear();
        searchedComponent = NULL;
    }
}

// ---------------------- Screens -------------------------

void DrawMainMenu(int w, int h) {
    BeginDrawing();
    DrawGradientBackground(w, h);

    Rectangle header = { 0, 0, (float)w, 100 };
    DrawRectangleRec(header, MakeColor(0, 188, 212, 255));
    DrawRectangleLines(0, 100, w, 2, MakeColor(0, 151, 167, 255));
    DrawTextEx(customFont, "Electronic Circuit Analyzer", Vector2{ 40, 18 }, 36.0f, 1.0f, MakeColor(250, 252, 255, 255));
    DrawTextEx(customFont, "CS250 - Series & Parallel Circuits with Impedance Analysis",
        Vector2{ 40, 60 }, 14.0f, 1.0f, MakeColor(230, 244, 241, 255));

    Rectangle panel = { 40.0f, 120.0f, (float)w - 80.0f, (float)h - 180.0f };
    DrawGlassPanel(panel, MakeColor(0, 150, 136, 255));

    float bx = panel.x + 30.0f;
    float by = panel.y + 30.0f;
    float bw = panel.width - 60.0f;
    float bh = 48.0f;
    float gap = 12.0f;

    Rectangle btns[6];
    for (int i = 0; i < 6; ++i) {
        btns[i] = { bx, by + i * (bh + gap), bw, bh };
    }

    const char* labels[6] = {
        "Add Component (Series/Parallel)",
        "Remove Component",
        "Search Component",
        "Display Circuit Diagrams",
        "Total Circuit Analysis",
        "Undo Last Operation"
    };

    Color btnColors[6] = {
        MakeColor(0, 150, 136, 220),
        MakeColor(244, 81, 30, 220),
        MakeColor(255, 167, 38, 220),
        MakeColor(102, 187, 106, 220),
        MakeColor(124, 77, 255, 220),
        MakeColor(3, 155, 229, 220)
    };

    Vector2 m = GetMousePosition();
    for (int i = 0; i < 6; ++i) {
        bool hover = CheckCollisionPointRec(m, btns[i]);
        DrawButtonEx(btns[i], labels[i], hover, btnColors[i]);
        if (hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            textBuffer.clear();
            statusMessage.clear();
            selectedIds.clear();
            switch (i) {
            case 0: currentScreen = ScreenState::ADD_COMPONENT; break;
            case 1: currentScreen = ScreenState::REMOVE_COMPONENT; break;
            case 2: currentScreen = ScreenState::SEARCH_COMPONENT; break;
            case 3: currentScreen = ScreenState::DISPLAY_ALL; break;
            case 4: currentScreen = ScreenState::CALC_RESISTANCE; break;
            case 5: Undo(); break;
            }
        }
    }

    int infoY = h - 45;
    DrawTextEx(customFont,
        TextFormat("Total: %d | Series: %d | Parallel: %d | Next ID: %d",
            (int)componentsData.size(),
            (int)seriesCircuit.size(),
            (int)parallelCircuit.size(),
            nextId),
        Vector2{ 40, (float)infoY }, 14.0f, 1.0f, MakeColor(55, 71, 79, 255));

    EndDrawing();
}

void DrawAddScreen(int w, int h) {
    BeginDrawing();
    DrawGradientBackground(w, h);
    DrawCommonTopBar(w, "Add Component to Circuit");
    DrawBackButton();

    Rectangle panel = { 40.0f, 120.0f, (float)w - 80.0f, (float)h - 180.0f };
    DrawGlassPanel(panel, MakeColor(0, 188, 212, 255));

    DrawTextEx(customFont, "Select Circuit Type:", Vector2{ 70, 145 }, 16.0f, 1.0f, MakeColor(38, 70, 83, 255));
    Rectangle seriesBtn = { 70.0f, 170.0f, 200.0f, 45.0f };
    Rectangle parallelBtn = { 290.0f, 170.0f, 200.0f, 45.0f };

    Vector2 m = GetMousePosition();
    bool hSeries = CheckCollisionPointRec(m, seriesBtn);
    bool hParallel = CheckCollisionPointRec(m, parallelBtn);

    Color seriesColor = (addCircuit == CircuitType::SERIES) ? MakeColor(0, 150, 136, 240) : MakeColor(0, 150, 136, 180);
    Color parallelColor = (addCircuit == CircuitType::PARALLEL) ? MakeColor(255, 167, 38, 240) : MakeColor(255, 167, 38, 180);

    DrawButtonEx(seriesBtn, "SERIES (In Line)", hSeries, seriesColor);
    DrawButtonEx(parallelBtn, "PARALLEL (Branches)", hParallel, parallelColor);

    if (hSeries && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) addCircuit = CircuitType::SERIES;
    if (hParallel && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) addCircuit = CircuitType::PARALLEL;

    DrawTextEx(customFont, "Select Component Type:", Vector2{ 70, 235 }, 16.0f, 1.0f, MakeColor(38, 70, 83, 255));

    Rectangle rBtn = { 70.0f, 265.0f, 130.0f, 70.0f };
    Rectangle cBtn = { 220.0f, 265.0f, 130.0f, 70.0f };
    Rectangle lBtn = { 370.0f, 265.0f, 130.0f, 70.0f };

    bool hR = CheckCollisionPointRec(m, rBtn);
    bool hC = CheckCollisionPointRec(m, cBtn);
    bool hL = CheckCollisionPointRec(m, lBtn);

    DrawGlassPanel(rBtn, (addType == ComponentType::RESISTOR) ? MakeColor(239, 83, 80, 255) : MakeColor(189, 189, 189, 255));
    DrawTextEx(customFont, "Resistor", Vector2{ (float)rBtn.x + 25, (float)rBtn.y + 50 }, 14.0f, 1.0f, MakeColor(211, 47, 47, 255));
    DrawResistorSymbol(rBtn.x + 65.0f, rBtn.y + 25.0f, 9.0f, MakeColor(211, 47, 47, 255));

    DrawGlassPanel(cBtn, (addType == ComponentType::CAPACITOR) ? MakeColor(100, 181, 246, 255) : MakeColor(189, 189, 189, 255));
    DrawTextEx(customFont, "Capacitor", Vector2{ (float)cBtn.x + 20, (float)cBtn.y + 50 }, 14.0f, 1.0f, MakeColor(30, 136, 229, 255));
    DrawCapacitorSymbol(cBtn.x + 65.0f, cBtn.y + 25.0f, 9.0f, MakeColor(30, 136, 229, 255));

    DrawGlassPanel(lBtn, (addType == ComponentType::INDUCTOR) ? MakeColor(129, 199, 132, 255) : MakeColor(189, 189, 189, 255));
    DrawTextEx(customFont, "Inductor", Vector2{ (float)lBtn.x + 30, (float)lBtn.y + 50 }, 14.0f, 1.0f, MakeColor(67, 160, 71, 255));
    DrawInductorSymbol(lBtn.x + 65.0f, lBtn.y + 25.0f, 7.0f, MakeColor(67, 160, 71, 255));

    if (hR && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) addType = ComponentType::RESISTOR;
    if (hC && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) addType = ComponentType::CAPACITOR;
    if (hL && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) addType = ComponentType::INDUCTOR;

    DrawTextEx(customFont, "Enter Value:", Vector2{ 70, 355 }, 16.0f, 1.0f, MakeColor(38, 70, 83, 255));
    Rectangle inputBox = { 70.0f, 380.0f, 270.0f, 38.0f };
    DrawRectangleRounded(inputBox, 0.2f, 8, MakeColor(255, 255, 255, 220));
    DrawRectangleRoundedLines(inputBox, 0.2f, 8, MakeColor(200, 230, 201, 255));
    DrawTextEx(customFont, textBuffer.c_str(), Vector2{ inputBox.x + 10, inputBox.y + 10 },
        16.0f, 1.0f, MakeColor(55, 71, 79, 255));
    HandleTextInput(textBuffer, 16);

    Rectangle addBtn = { 70.0f, 430.0f, 130.0f, 40.0f };
    bool hAdd = CheckCollisionPointRec(m, addBtn);
    DrawButtonEx(addBtn, "Add", hAdd, MakeColor(102, 187, 106, 220));

    if (hAdd && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        bool ok;
        double v = StringToDoubleSafe(textBuffer, ok);
        if (ok && v > 0.0) {
            AddComponent(addType, v, addCircuit);
            statusMessage = "Component added successfully!";
            textBuffer.clear();
        }
        else {
            statusMessage = "Invalid value. Enter a positive number.";
        }
    }

    DrawTextEx(customFont, statusMessage.c_str(), Vector2{ 70, 485 }, 14.0f, 1.0f, MakeColor(211, 47, 47, 255));

    EndDrawing();
}

void DrawRemoveScreen(int w, int h) {
    BeginDrawing();
    DrawGradientBackground(w, h);
    DrawCommonTopBar(w, "Remove Component");
    DrawBackButton();

    Rectangle panel = { 40.0f, 120.0f, 500.0f, 250.0f };
    DrawGlassPanel(panel, MakeColor(244, 81, 30, 255));

    DrawTextEx(customFont, "Enter Component ID to remove:", Vector2{ 70, 150 }, 16.0f, 1.0f, MakeColor(38, 70, 83, 255));
    Rectangle inputBox = { 70.0f, 180.0f, 270.0f, 38.0f };
    DrawRectangleRounded(inputBox, 0.2f, 8, MakeColor(255, 255, 255, 220));
    DrawRectangleRoundedLines(inputBox, 0.2f, 8, MakeColor(255, 205, 210, 255));
    DrawTextEx(customFont, textBuffer.c_str(), Vector2{ inputBox.x + 10, inputBox.y + 10 },
        16.0f, 1.0f, MakeColor(55, 71, 79, 255));
    HandleTextInput(textBuffer, 16);

    Rectangle remBtn = { 70.0f, 230.0f, 130.0f, 40.0f };
    Vector2 m = GetMousePosition();
    bool hRem = CheckCollisionPointRec(m, remBtn);
    DrawButtonEx(remBtn, "Remove", hRem, MakeColor(229, 57, 53, 220));

    if (hRem && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        bool ok;
        int id = StringToIntSafe(textBuffer, ok);
        if (ok) {
            statusMessage = RemoveComponent(id) ? "Component removed!" : "Not found.";
        }
        else {
            statusMessage = "Invalid ID.";
        }
        textBuffer.clear();
    }

    DrawTextEx(customFont, statusMessage.c_str(), Vector2{ 70, 285 }, 14.0f, 1.0f, MakeColor(198, 40, 40, 255));

    EndDrawing();
}

void DrawSearchScreen(int w, int h) {
    BeginDrawing();
    DrawGradientBackground(w, h);
    DrawCommonTopBar(w, "Search Component");
    DrawBackButton();

    Rectangle panel = { 40.0f, 120.0f, (float)w - 80.0f, (float)h - 180.0f };
    DrawGlassPanel(panel, MakeColor(255, 167, 38, 255));

    DrawTextEx(customFont, "Enter Component ID to search:", Vector2{ 70, 150 }, 16.0f, 1.0f, MakeColor(38, 70, 83, 255));
    Rectangle inputBox = { 70.0f, 180.0f, 270.0f, 38.0f };
    DrawRectangleRounded(inputBox, 0.2f, 8, MakeColor(255, 255, 255, 220));
    DrawRectangleRoundedLines(inputBox, 0.2f, 8, MakeColor(255, 224, 178, 255));
    DrawTextEx(customFont, textBuffer.c_str(), Vector2{ inputBox.x + 10, inputBox.y + 10 },
        16.0f, 1.0f, MakeColor(55, 71, 79, 255));
    HandleTextInput(textBuffer, 16);

    Rectangle searchBtn = { 70.0f, 230.0f, 130.0f, 40.0f };
    Vector2 m = GetMousePosition();
    bool hSearch = CheckCollisionPointRec(m, searchBtn);
    DrawButtonEx(searchBtn, "Search", hSearch, MakeColor(255, 143, 0, 220));

    if (hSearch && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        bool ok;
        int id = StringToIntSafe(textBuffer, ok);
        searchedComponent = ok ? FindComponent(id) : NULL;
        statusMessage = searchedComponent ? "Found!" : "Not found.";
        textBuffer.clear();
    }

    DrawTextEx(customFont, statusMessage.c_str(), Vector2{ 70, 285 }, 14.0f, 1.0f, MakeColor(230, 81, 0, 255));

    if (searchedComponent) {
        int y = 340;
        DrawTextEx(customFont, TextFormat("ID: %d", searchedComponent->id),
            Vector2{ 70, (float)y }, 14.0f, 1.0f, MakeColor(38, 70, 83, 255));
        DrawTextEx(customFont, TypeToString(searchedComponent->type).c_str(),
            Vector2{ 150, (float)y }, 14.0f, 1.0f, TypeColor(searchedComponent->type));
        DrawTextEx(customFont, TextFormat("%.6f", searchedComponent->value),
            Vector2{ 250, (float)y }, 14.0f, 1.0f, MakeColor(55, 71, 79, 255));
        DrawTextEx(customFont,
            (searchedComponent->circuitType == CircuitType::SERIES ? "SERIES" : "PARALLEL"),
            Vector2{ 380, (float)y }, 14.0f, 1.0f,
            (searchedComponent->circuitType == CircuitType::SERIES ?
                MakeColor(0, 150, 136, 255) : MakeColor(102, 187, 106, 255)));

        DrawComponentSymbol(searchedComponent->type, 70.0f, (float)y + 40.0f,
            12.0f, TypeColor(searchedComponent->type));
    }

    EndDrawing();
}

void DrawDisplayScreen(int w, int h) {
    BeginDrawing();
    DrawGradientBackground(w, h);
    DrawCommonTopBar(w, "Circuit Diagrams");
    DrawBackButton();

    Rectangle seriesPanel = { 40.0f, 120.0f, (float)w - 80.0f, 200.0f };
    DrawGlassPanel(seriesPanel, MakeColor(0, 188, 212, 255));

    if (!seriesCircuit.empty()) {
        DrawSeriesCircuitDiagram(seriesPanel.x + 20.0f, seriesPanel.y + 50.0f, seriesPanel.width - 40.0f);

        double seriesR = CalcSeries(seriesCircuit);
        double seriesZ = CalcSeriesImpedance(seriesCircuit, analysisFrequencyHz);

        // Bigger, separated results line
        float textY = seriesPanel.y + 165.0f;
        DrawTextEx(customFont, "SERIES RESULTS:",
            Vector2{ seriesPanel.x + 20, textY }, 18.0f, 1.0f, MakeColor(0, 77, 64, 255));
        textY += 22.0f;
        DrawTextEx(customFont,
            TextFormat("R = %.3f Ohm    |Z| = %.3f Ohm    f = %.0f Hz",
                seriesR, seriesZ, analysisFrequencyHz),
            Vector2{ seriesPanel.x + 20, textY }, 16.0f, 1.0f, MakeColor(13, 71, 161, 255));
    }
    else {
        DrawTextEx(customFont, "Series Circuit: EMPTY",
            Vector2{ seriesPanel.x + 20, seriesPanel.y + 60 },
            16.0f, 1.0f, MakeColor(55, 71, 79, 255));
    }

    Rectangle parallelPanel = { 40.0f, 330.0f, (float)w - 80.0f, 200.0f };
    DrawGlassPanel(parallelPanel, MakeColor(102, 187, 106, 255));

    if (!parallelCircuit.empty()) {
        DrawParallelCircuitDiagram(parallelPanel.x + 20.0f, parallelPanel.y + 50.0f, parallelPanel.width - 40.0f);

        double parallelR = CalcParallel(parallelCircuit);
        double parallelZ = CalcParallelImpedance(parallelCircuit, analysisFrequencyHz);

        float textY = parallelPanel.y + 165.0f;
        DrawTextEx(customFont, "PARALLEL RESULTS:",
            Vector2{ parallelPanel.x + 20, textY }, 18.0f, 1.0f, MakeColor(104, 66, 0, 255));
        textY += 22.0f;
        DrawTextEx(customFont,
            TextFormat("R = %.3f Ohm    |Z| = %.3f Ohm    f = %.0f Hz",
                parallelR, parallelZ, analysisFrequencyHz),
            Vector2{ parallelPanel.x + 20, textY }, 16.0f, 1.0f, MakeColor(27, 94, 32, 255));
    }
    else {
        DrawTextEx(customFont, "Parallel Circuit: EMPTY",
            Vector2{ parallelPanel.x + 20, parallelPanel.y + 60 },
            16.0f, 1.0f, MakeColor(33, 53, 64, 255));
    }

    EndDrawing();
}
//this portion reamended

void DrawCalcScreen(int w, int h) {
    BeginDrawing();
    DrawGradientBackground(w, h);
    DrawCommonTopBar(w, "Total Circuit Analysis");
    DrawBackButton();

    // Very dark panel
    Rectangle panel = { 40.0f, 120.0f, (float)w - 80.0f, (float)h - 180.0f };
    DrawRectangleRounded(panel, 0.1f, 16, MakeColor(33, 33, 33, 255));          // near‑black
    DrawRectangleRoundedLines(panel, 0.1f, 16, MakeColor(66, 66, 66, 255));

    double seriesTotal = CalcSeries(seriesCircuit);
    double parallelTotal = CalcParallel(parallelCircuit);
    double seriesZ = CalcSeriesImpedance(seriesCircuit, analysisFrequencyHz);
    double parallelZ = CalcParallelImpedance(parallelCircuit, analysisFrequencyHz);

    int y = (int)panel.y + 20;

    Color textMain = MakeColor(245, 245, 245, 255);   // almost white
    Color textSub = MakeColor(200, 200, 200, 255);
    Color textSeries = MakeColor(129, 212, 250, 255);   // light blue
    Color textPar = MakeColor(165, 214, 167, 255);   // light green
    Color textWarn = MakeColor(255, 241, 118, 255);   // yellow

    DrawTextEx(customFont, "Circuit Analysis Report",
        Vector2{ panel.x + 20, (float)y }, 16.0f, 1.0f, textMain);
    y += 35;

    DrawTextEx(customFont, TextFormat("Frequency: %.1f Hz", analysisFrequencyHz),
        Vector2{ panel.x + 20, (float)y }, 13.0f, 1.0f, textSub);
    y += 25;

    DrawTextEx(customFont,
        TextFormat("Series: %d components | R = %.3f Ohm | Z = %.3f Ohm",
            (int)seriesCircuit.size(), seriesTotal, seriesZ),
        Vector2{ panel.x + 20, (float)y }, 13.0f, 1.0f, textSeries);
    y += 25;

    DrawTextEx(customFont,
        TextFormat("Parallel: %d components | R = %.3f Ohm | Z = %.3f Ohm",
            (int)parallelCircuit.size(), parallelTotal, parallelZ),
        Vector2{ panel.x + 20, (float)y }, 13.0f, 1.0f, textPar);
    y += 35;

    if (!seriesCircuit.empty() && !parallelCircuit.empty()) {
        DrawTextEx(customFont, "COMBINED (Series + Parallel):",
            Vector2{ panel.x + 20, (float)y }, 14.0f, 1.0f, textWarn);
        y += 25;
        double combined = seriesTotal + parallelTotal;
        double combinedZ = seriesZ + parallelZ;
        DrawTextEx(customFont,
            TextFormat("Total R = %.3f Ohm | Total Z = %.3f Ohm", combined, combinedZ),
            Vector2{ panel.x + 20, (float)y }, 13.0f, 1.0f, textWarn);
    }
    else if (!seriesCircuit.empty()) {
        DrawTextEx(customFont, "Only Series Circuit (Resistance dominates)",
            Vector2{ panel.x + 20, (float)y }, 13.0f, 1.0f, textSeries);
    }
    else if (!parallelCircuit.empty()) {
        DrawTextEx(customFont, "Only Parallel Circuit (Resistance dominates)",
            Vector2{ panel.x + 20, (float)y }, 13.0f, 1.0f, textPar);
    }
    else {
        DrawTextEx(customFont, "No components added yet!",
            Vector2{ panel.x + 20, (float)y }, 13.0f, 1.0f, textMain);
    }

    EndDrawing();
}


// ---------------------- MAIN -------------------------

int main() {
    const int screenWidth = 1280;
    const int screenHeight = 900;

    InitWindow(screenWidth, screenHeight, "Electronic Circuit Analyzer - Group 13 (Light Theme)");
    // instead of LoadFont("f1.ttf");
    customFont = LoadFontEx("f1.ttf", 32, nullptr, 0);   // bigger base size [web:70]

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        switch (currentScreen) {
        case ScreenState::MAIN_MENU:      DrawMainMenu(screenWidth, screenHeight); break;
        case ScreenState::ADD_COMPONENT:  DrawAddScreen(screenWidth, screenHeight); break;
        case ScreenState::REMOVE_COMPONENT: DrawRemoveScreen(screenWidth, screenHeight); break;
        case ScreenState::SEARCH_COMPONENT: DrawSearchScreen(screenWidth, screenHeight); break;
        case ScreenState::DISPLAY_ALL:    DrawDisplayScreen(screenWidth, screenHeight); break;
        case ScreenState::CALC_RESISTANCE: DrawCalcScreen(screenWidth, screenHeight); break;
        }
    }

    UnloadFont(customFont);
    CloseWindow();
    return 0;
}
