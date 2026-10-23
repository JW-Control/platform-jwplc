#include "JWPLC_UI_Menu.h"
#include <JWPLC_TFT.h>

#include "JWPLC_Display_API.h"
#include "JWPLC_UI.h"
#include "JWPLC_UI_Pages.h"

#include <JWPLC_GlobalPeripherals.h>
#include <Arduino.h>

#include <cmath>
#include <cstdio>
#include <cstring>

// Motor del MENU declarativo. Este TU sólo se enlaza si el sketch usa
// setMenus()/JWPLC_UIMenu*: los hooks jwplcUIMenu* de abajo reemplazan a las
// versiones weak de JWPLC_UI_API.cpp.
//
// Bus SPI: todo el dibujo ocurre dentro de jwplcUIMenuDrawStatic/DrawDirty,
// que el runtime llama con el bus TFT tomado. serviceInput() no toca la TFT.

namespace
{
    static constexpr uint8_t MAX_VISIBLE_ROWS = 16;
    static constexpr int16_t ROW_PAD_X = 3;
    static constexpr int16_t ROW_PAD_Y = 2;

    struct MenuRuntime
    {
        JWPLC_UIMenu def;
        uint8_t depth;                                // 1..MAX_DEPTH
        uint8_t parent[JWPLC_UI_MENU_MAX_DEPTH];      // SUBMENU abierto por nivel
        uint8_t selected[JWPLC_UI_MENU_MAX_DEPTH];    // posición entre los hijos
        uint8_t scroll[JWPLC_UI_MENU_MAX_DEPTH];
        bool editing;
        float editFloat;
        int32_t editInt;
        bool focused;
        bool fullDirty;
        bool headerDirty;
        uint16_t dirtyRows;
        float cache[MAX_VISIBLE_ROWS];
        uint8_t changedNode;                          // índice + 1; 0 = sin cambios
    };

    MenuRuntime g_menus[JWPLC_UI_MENU_MAX_MENUS];
    uint8_t g_menuCount = 0;

    // ------------------------------------------------------------------
    // Árbol
    // ------------------------------------------------------------------

    uint8_t childCount(const MenuRuntime &m, uint8_t parent)
    {
        uint8_t count = 0;
        for (uint8_t i = 0; i < m.def.nodeCount; ++i)
        {
            if (m.def.nodes[i].parent == parent)
                count++;
        }
        return count;
    }

    // Índice del nodo que ocupa la posición pos entre los hijos de parent.
    int16_t childAt(const MenuRuntime &m, uint8_t parent, uint8_t pos)
    {
        uint8_t seen = 0;
        for (uint8_t i = 0; i < m.def.nodeCount; ++i)
        {
            if (m.def.nodes[i].parent != parent)
                continue;
            if (seen == pos)
                return i;
            seen++;
        }
        return -1;
    }

    uint8_t level(const MenuRuntime &m)
    {
        return (uint8_t)(m.depth - 1);
    }

    uint8_t currentParent(const MenuRuntime &m)
    {
        return m.parent[level(m)];
    }

    // ------------------------------------------------------------------
    // Geometría
    // ------------------------------------------------------------------

    uint8_t textScale(const MenuRuntime &m)
    {
        return (m.def.textSize >= 2) ? 2 : 1;
    }

    int16_t border(const MenuRuntime &m)
    {
        return m.def.frame ? 1 : 0;
    }

    int16_t rowHeight(const MenuRuntime &m)
    {
        return (int16_t)(8 * textScale(m) + 2 * ROW_PAD_Y);
    }

    int16_t headerHeight(const MenuRuntime &m)
    {
        return (m.def.title != nullptr) ? rowHeight(m) : 0;
    }

    uint8_t visibleRows(const MenuRuntime &m)
    {
        const int16_t avail = m.def.height - 2 * border(m) - headerHeight(m);
        int16_t rows = (avail > 0) ? avail / rowHeight(m) : 0;
        if (rows > MAX_VISIBLE_ROWS)
            rows = MAX_VISIBLE_ROWS;
        return (uint8_t)rows;
    }

    int16_t rowY(const MenuRuntime &m, uint8_t row)
    {
        return (int16_t)(m.def.y + border(m) + headerHeight(m) + row * rowHeight(m));
    }

    // ------------------------------------------------------------------
    // Valores
    // ------------------------------------------------------------------

    float readValue(const JWPLC_UIMenuNode &node)
    {
        if (node.value == nullptr)
            return 0.0f;

        switch (node.kind)
        {
        case JWPLC_UI_MENU_VALUE_FLOAT:
            return *static_cast<const float *>(node.value);
        case JWPLC_UI_MENU_VALUE_INT32:
            return (float)*static_cast<const int32_t *>(node.value);
        case JWPLC_UI_MENU_VALUE_BOOL:
            return *static_cast<const bool *>(node.value) ? 1.0f : 0.0f;
        default:
            return 0.0f;
        }
    }

    void formatNumber(const JWPLC_UIMenuNode &node, float value, int32_t intValue, char *out, size_t size)
    {
        if (node.kind == JWPLC_UI_MENU_VALUE_INT32)
            snprintf(out, size, "%ld", (long)intValue);
        else
            snprintf(out, size, "%.*f", (int)node.decimals, (double)value);

        if (node.unit != nullptr && node.unit[0] != '\0')
        {
            const size_t len = strlen(out);
            snprintf(out + len, size - len, " %s", node.unit);
        }
    }

    void formatNode(const MenuRuntime &m, const JWPLC_UIMenuNode &node, bool editingRow, char *out, size_t size)
    {
        out[0] = '\0';

        if (node.type == JWPLC_UI_MENU_SUBMENU)
        {
            snprintf(out, size, ">");
            return;
        }

        if (node.type == JWPLC_UI_MENU_GO_IDLE)
        {
            snprintf(out, size, "<");
            return;
        }

        if (node.kind == JWPLC_UI_MENU_VALUE_BOOL)
        {
            const bool state = editingRow
                                   ? (m.editInt != 0)
                                   : (readValue(node) != 0.0f);
            const char *text = state ? node.trueText : node.falseText;
            snprintf(out, size, "%s", (text != nullptr) ? text : (state ? "ON" : "OFF"));
            return;
        }

        if (editingRow)
            formatNumber(node, m.editFloat, m.editInt, out, size);
        else
            formatNumber(node, readValue(node), (node.kind == JWPLC_UI_MENU_VALUE_INT32 && node.value != nullptr)
                                                    ? *static_cast<const int32_t *>(node.value)
                                                    : 0,
                         out, size);
    }

    // ------------------------------------------------------------------
    // Dirty
    // ------------------------------------------------------------------

    void markRow(MenuRuntime &m, uint8_t pos)
    {
        const uint8_t sc = m.scroll[level(m)];
        if (pos < sc)
            return;
        const uint8_t row = (uint8_t)(pos - sc);
        if (row < visibleRows(m))
            m.dirtyRows |= (uint16_t)(1u << row);
    }

    void markAll(MenuRuntime &m)
    {
        m.fullDirty = true;
        for (uint8_t i = 0; i < MAX_VISIBLE_ROWS; ++i)
            m.cache[i] = NAN;
    }

    void resetMenu(MenuRuntime &m)
    {
        m.depth = 1;
        for (uint8_t i = 0; i < JWPLC_UI_MENU_MAX_DEPTH; ++i)
        {
            m.parent[i] = JWPLC_UI_MENU_ROOT;
            m.selected[i] = 0;
            m.scroll[i] = 0;
        }
        m.editing = false;
        m.dirtyRows = 0;
        m.headerDirty = false;
        markAll(m);
    }

    bool anyDirty(const MenuRuntime &m)
    {
        return m.fullDirty || m.headerDirty || m.dirtyRows != 0;
    }

    // ------------------------------------------------------------------
    // Navegación
    // ------------------------------------------------------------------

    void ensureVisible(MenuRuntime &m)
    {
        const uint8_t lv = level(m);
        const uint8_t rows = visibleRows(m);
        if (rows == 0)
            return;

        const uint8_t sel = m.selected[lv];
        uint8_t sc = m.scroll[lv];

        if (sel < sc)
            sc = sel;
        else if (sel >= (uint8_t)(sc + rows))
            sc = (uint8_t)(sel - rows + 1);

        if (sc != m.scroll[lv])
        {
            m.scroll[lv] = sc;
            m.dirtyRows = (uint16_t)((1u << rows) - 1u);
            m.headerDirty = true; // flechas de scroll
            for (uint8_t i = 0; i < MAX_VISIBLE_ROWS; ++i)
                m.cache[i] = NAN;
        }
    }

    void enterLevel(MenuRuntime &m, uint8_t submenu)
    {
        if (m.depth >= JWPLC_UI_MENU_MAX_DEPTH || childCount(m, submenu) == 0)
            return;
        m.depth++;
        m.parent[level(m)] = submenu;
        m.selected[level(m)] = 0;
        m.scroll[level(m)] = 0;
        markAll(m);
    }

    void leaveLevel(MenuRuntime &m)
    {
        if (m.depth <= 1)
            return;
        m.depth--;
        markAll(m);
    }

    void startEdit(MenuRuntime &m, const JWPLC_UIMenuNode &node, uint8_t pos)
    {
        if (node.value == nullptr)
            return;
        m.editing = true;
        if (node.kind == JWPLC_UI_MENU_VALUE_INT32)
            m.editInt = *static_cast<const int32_t *>(node.value);
        else if (node.kind == JWPLC_UI_MENU_VALUE_BOOL)
            m.editInt = *static_cast<const bool *>(node.value) ? 1 : 0;
        else
            m.editFloat = *static_cast<const float *>(node.value);
        markRow(m, pos);
    }

    void commitEdit(MenuRuntime &m, uint8_t nodeIndex, uint8_t pos)
    {
        const JWPLC_UIMenuNode &node = m.def.nodes[nodeIndex];
        if (node.kind == JWPLC_UI_MENU_VALUE_INT32)
            *static_cast<int32_t *>(node.value) = m.editInt;
        else if (node.kind == JWPLC_UI_MENU_VALUE_BOOL)
            *static_cast<bool *>(node.value) = (m.editInt != 0);
        else
            *static_cast<float *>(node.value) = m.editFloat;

        m.changedNode = (uint8_t)(nodeIndex + 1);
        m.editing = false;
        markRow(m, pos);
    }

    // Índice de paso del valor editado dentro de [min, max].
    uint32_t stepCount(const JWPLC_UIMenuNode &node)
    {
        const float span = node.max - node.min;
        const float step = (node.step > 0.0f) ? node.step : 1.0f;
        if (span <= 0.0f)
            return 0;
        return (uint32_t)lroundf(span / step);
    }

    void applyStepIndex(MenuRuntime &m, const JWPLC_UIMenuNode &node, uint32_t idx)
    {
        const float step = (node.step > 0.0f) ? node.step : 1.0f;
        if (node.kind == JWPLC_UI_MENU_VALUE_INT32)
            m.editInt = (int32_t)lroundf(node.min) + (int32_t)idx * (int32_t)lroundf(step);
        else
            m.editFloat = node.min + (float)idx * step;
    }

    uint32_t currentStepIndex(const MenuRuntime &m, const JWPLC_UIMenuNode &node)
    {
        const float step = (node.step > 0.0f) ? node.step : 1.0f;
        const float value = (node.kind == JWPLC_UI_MENU_VALUE_INT32) ? (float)m.editInt : m.editFloat;
        const long idx = lroundf((value - node.min) / step);
        const uint32_t maxIdx = stepCount(node);
        if (idx < 0)
            return 0;
        return ((uint32_t)idx > maxIdx) ? maxIdx : (uint32_t)idx;
    }

    void handleEdit(MenuRuntime &m)
    {
        const uint8_t lv = level(m);
        const uint8_t pos = m.selected[lv];
        const int16_t nodeIndex = childAt(m, currentParent(m), pos);
        if (nodeIndex < 0)
        {
            m.editing = false;
            return;
        }
        const JWPLC_UIMenuNode &node = m.def.nodes[nodeIndex];

        if (JWPLC_Buttons.pressed(BTN_ESC))
        {
            m.editing = false;
            markRow(m, pos);
            JWPLC_Buttons.clearPendingInput();
            return;
        }

        if (JWPLC_Buttons.pressed(BTN_OK))
        {
            commitEdit(m, (uint8_t)nodeIndex, pos);
            JWPLC_Buttons.clearPendingInput();
            return;
        }

        // LEFT/RIGHT no tienen repeat: se consumen todas las pulsaciones
        // pendientes para que ninguna quede como navegación al confirmar.
        uint8_t left = 0;
        uint8_t right = 0;
        while (JWPLC_Buttons.pressed(BTN_LEFT))
            left++;
        while (JWPLC_Buttons.pressed(BTN_RIGHT))
            right++;

        if (node.kind == JWPLC_UI_MENU_VALUE_BOOL)
        {
            uint32_t state = (uint32_t)(m.editInt != 0);
            const bool ud = JWPLC_Buttons.applyAxis(&state, 0, 1, BTN_DOWN, BTN_UP, true, false);
            const bool lr = ((left + right) & 1u) != 0;
            if (lr)
                state ^= 1u;
            if (lr || ud)
            {
                m.editInt = (int32_t)state;
                markRow(m, pos);
            }
            return;
        }

        const uint32_t maxIdx = stepCount(node);
        uint32_t idx = currentStepIndex(m, node);
        bool changed = JWPLC_Buttons.applyAxis(&idx, 0, maxIdx, BTN_DOWN, BTN_UP, false, true);

        for (uint8_t i = 0; i < left; ++i)
        {
            idx = (idx > 10) ? idx - 10 : 0;
            changed = true;
        }
        for (uint8_t i = 0; i < right; ++i)
        {
            idx = (idx + 10 < maxIdx) ? idx + 10 : maxIdx;
            changed = true;
        }

        if (changed)
        {
            applyStepIndex(m, node, idx);
            markRow(m, pos);
        }
    }

    void handleNavigate(MenuRuntime &m)
    {
        const uint8_t lv = level(m);
        const uint8_t parent = currentParent(m);
        const uint8_t count = childCount(m, parent);

        if (count > 0)
        {
            uint32_t sel = m.selected[lv];
            if (sel >= count)
                sel = (uint32_t)(count - 1);
            const uint8_t before = (uint8_t)sel;
            if (JWPLC_Buttons.applyAxis(&sel, 0, (uint32_t)(count - 1), BTN_UP, BTN_DOWN, true, false) &&
                (uint8_t)sel != before)
            {
                markRow(m, before);
                m.selected[lv] = (uint8_t)sel;
                markRow(m, (uint8_t)sel);
                ensureVisible(m);
            }
        }

        const bool ok = JWPLC_Buttons.pressed(BTN_OK);
        const bool right = JWPLC_Buttons.pressed(BTN_RIGHT);
        const bool left = JWPLC_Buttons.pressed(BTN_LEFT);

        // ESC sólo se consume fuera de la raíz: en la raíz pertenece al
        // sistema (PAGE_SELECT / retorno a IDLE).
        const bool esc = (m.depth > 1) && JWPLC_Buttons.pressed(BTN_ESC);

        if (esc || (left && m.depth > 1))
        {
            leaveLevel(m);
            JWPLC_Buttons.clearPendingInput();
            return;
        }

        if (count == 0 || !(ok || right))
            return;

        const uint8_t pos = m.selected[lv];
        const int16_t nodeIndex = childAt(m, parent, pos);
        if (nodeIndex < 0)
            return;
        const JWPLC_UIMenuNode &node = m.def.nodes[nodeIndex];

        if (node.type == JWPLC_UI_MENU_SUBMENU)
        {
            enterLevel(m, (uint8_t)nodeIndex);
            JWPLC_Buttons.clearPendingInput();
        }
        else if (ok &&
                 (node.type == JWPLC_UI_MENU_EDIT_NUM || node.type == JWPLC_UI_MENU_EDIT_BOOL))
        {
            startEdit(m, node, pos);
            JWPLC_Buttons.clearPendingInput();
        }
        else if (ok && node.type == JWPLC_UI_MENU_GO_IDLE)
        {
            // El refresco revisa el modo después de atender la entrada, así
            // que la IDLE se dibuja en este mismo ciclo.
            JWPLC_Buttons.clearPendingInput();
            JWPLC_Display.goIdle();
        }
    }

    // Repinta las filas cuyo valor vinculado cambió desde el último dibujo.
    void pollValues(MenuRuntime &m)
    {
        if (m.fullDirty)
            return;

        const uint8_t lv = level(m);
        const uint8_t rows = visibleRows(m);
        const uint8_t parent = currentParent(m);

        for (uint8_t r = 0; r < rows; ++r)
        {
            const uint8_t pos = (uint8_t)(m.scroll[lv] + r);
            const int16_t nodeIndex = childAt(m, parent, pos);
            if (nodeIndex < 0)
                break;
            const JWPLC_UIMenuNode &node = m.def.nodes[nodeIndex];
            if (node.type == JWPLC_UI_MENU_SUBMENU || node.type == JWPLC_UI_MENU_GO_IDLE)
                continue;
            if (m.editing && pos == m.selected[lv])
                continue;

            const float v = readValue(node);
            if (!(v == m.cache[r]))
                m.dirtyRows |= (uint16_t)(1u << r);
        }
    }

    MenuRuntime *activeMenu(uint8_t page)
    {
        for (uint8_t i = 0; i < g_menuCount; ++i)
        {
            if (g_menus[i].def.page == page)
                return &g_menus[i];
        }
        return nullptr;
    }

    // ------------------------------------------------------------------
    // Dibujo (bus TFT ya tomado por el runtime)
    // ------------------------------------------------------------------

    void drawText(JWPLC_TFTClass &tft, const MenuRuntime &m, int16_t x, int16_t y, const char *text, uint16_t fg, uint16_t bg)
    {
        tft.setTextSize(textScale(m));
        tft.setTextColor(fg, bg);
        tft.setCursor(x, y);
        tft.print(text);
    }

    void drawHeader(JWPLC_TFTClass &tft, MenuRuntime &m)
    {
        m.headerDirty = false;
        if (m.def.title == nullptr)
            return;

        const JWPLC_UIMenuColors &c = m.def.colors;
        const int16_t b = border(m);
        const int16_t x = m.def.x + b;
        const int16_t y = m.def.y + b;
        const int16_t w = m.def.width - 2 * b;
        const int16_t h = rowHeight(m);
        const int16_t cw = 6 * textScale(m);

        tft.fillRect(x, y, w, h, c.titleBackground);

        const char *title = m.def.title;
        if (m.depth > 1)
            title = m.def.nodes[currentParent(m)].label;

        char text[48];
        snprintf(text, sizeof(text), "%s%s", (m.depth > 1) ? "< " : "", (title != nullptr) ? title : "");

        // Flechas de scroll a la derecha (0x1E / 0x1F de la fuente GLCD).
        const uint8_t lv = level(m);
        const uint8_t count = childCount(m, currentParent(m));
        const bool up = m.scroll[lv] > 0;
        const bool down = (uint8_t)(m.scroll[lv] + visibleRows(m)) < count;
        const int16_t arrowsW = (up || down) ? 2 * cw : 0;

        int16_t maxChars = (int16_t)((w - 2 * ROW_PAD_X - arrowsW) / cw);
        if (maxChars < 0)
            maxChars = 0;
        if ((int16_t)strlen(text) > maxChars)
            text[maxChars] = '\0';

        drawText(tft, m, x + ROW_PAD_X, y + ROW_PAD_Y, text, c.title, c.titleBackground);

        if (arrowsW > 0)
        {
            char arrows[3] = {up ? '\x1E' : ' ', down ? '\x1F' : ' ', '\0'};
            drawText(tft, m, x + w - ROW_PAD_X - arrowsW, y + ROW_PAD_Y, arrows, c.title, c.titleBackground);
        }
    }

    void drawRow(JWPLC_TFTClass &tft, MenuRuntime &m, uint8_t row)
    {
        const JWPLC_UIMenuColors &c = m.def.colors;
        const uint8_t lv = level(m);
        const uint8_t pos = (uint8_t)(m.scroll[lv] + row);
        const int16_t b = border(m);
        const int16_t x = m.def.x + b;
        const int16_t y = rowY(m, row);
        const int16_t w = m.def.width - 2 * b;
        const int16_t h = rowHeight(m);
        const int16_t cw = 6 * textScale(m);

        const int16_t nodeIndex = childAt(m, currentParent(m), pos);
        const bool selected = (nodeIndex >= 0) && (pos == m.selected[lv]);
        const bool editingRow = selected && m.editing;

        uint16_t bg = c.background;
        uint16_t fg = c.text;
        uint16_t vfg = c.value;

        if (editingRow)
        {
            bg = c.editBackground;
            fg = c.editText;
            vfg = c.editText;
        }
        else if (selected && m.focused)
        {
            bg = c.selectBackground;
            fg = c.selectText;
            vfg = c.selectText;
        }

        tft.fillRect(x, y, w, h, bg);

        if (selected && !m.focused && !editingRow)
            tft.drawRect(x, y, w, h, c.selectBackground);

        if (nodeIndex < 0)
        {
            m.cache[row] = NAN;
            return;
        }

        const JWPLC_UIMenuNode &node = m.def.nodes[nodeIndex];

        char value[32];
        formatNode(m, node, editingRow, value, sizeof(value));
        if (editingRow && node.kind != JWPLC_UI_MENU_VALUE_BOOL)
        {
            char framed[34];
            snprintf(framed, sizeof(framed), "<%s>", value);
            strncpy(value, framed, sizeof(value) - 1);
            value[sizeof(value) - 1] = '\0';
        }

        int16_t valueChars = (int16_t)strlen(value);
        int16_t maxValueChars = (int16_t)((w - 2 * ROW_PAD_X) / cw);
        if (valueChars > maxValueChars)
        {
            valueChars = maxValueChars;
            value[valueChars] = '\0';
        }
        const int16_t valueW = valueChars * cw;

        char label[40];
        snprintf(label, sizeof(label), "%s", (node.label != nullptr) ? node.label : "");
        int16_t labelChars = (int16_t)((w - 2 * ROW_PAD_X - valueW - (valueW > 0 ? cw : 0)) / cw);
        if (labelChars < 0)
            labelChars = 0;
        if ((int16_t)strlen(label) > labelChars)
            label[labelChars] = '\0';

        drawText(tft, m, x + ROW_PAD_X, y + ROW_PAD_Y, label, fg, bg);
        if (valueW > 0)
            drawText(tft, m, x + w - ROW_PAD_X - valueW, y + ROW_PAD_Y, value, vfg, bg);

        m.cache[row] = (node.type == JWPLC_UI_MENU_SUBMENU || node.type == JWPLC_UI_MENU_GO_IDLE) ? 0.0f : readValue(node);
    }

    void drawFull(JWPLC_TFTClass &tft, MenuRuntime &m)
    {
        const JWPLC_UIMenuColors &c = m.def.colors;
        tft.fillRect(m.def.x, m.def.y, m.def.width, m.def.height, c.background);
        if (m.def.frame)
            tft.drawRect(m.def.x, m.def.y, m.def.width, m.def.height, c.frame);

        drawHeader(tft, m);
        const uint8_t rows = visibleRows(m);
        for (uint8_t r = 0; r < rows; ++r)
            drawRow(tft, m, r);

        m.fullDirty = false;
        m.dirtyRows = 0;
    }

    void drawPending(JWPLC_TFTClass &tft, MenuRuntime &m)
    {
        if (m.fullDirty)
        {
            drawFull(tft, m);
            return;
        }
        if (m.headerDirty)
            drawHeader(tft, m);

        const uint8_t rows = visibleRows(m);
        for (uint8_t r = 0; r < rows; ++r)
        {
            if (m.dirtyRows & (uint16_t)(1u << r))
                drawRow(tft, m, r);
        }
        m.dirtyRows = 0;
    }

    // ------------------------------------------------------------------
    // Validación
    // ------------------------------------------------------------------

    bool validMenu(const JWPLC_UIMenu &menu)
    {
        if (menu.nodes == nullptr || menu.nodeCount == 0 ||
            menu.nodeCount > JWPLC_UI_MENU_MAX_NODES ||
            menu.width <= 0 || menu.height <= 0)
        {
            return false;
        }

        for (uint8_t i = 0; i < menu.nodeCount; ++i)
        {
            const JWPLC_UIMenuNode &node = menu.nodes[i];

            // El padre debe ser un SUBMENU declarado antes (sin ciclos).
            uint8_t depth = 1;
            uint8_t p = node.parent;
            while (p != JWPLC_UI_MENU_ROOT)
            {
                if (p >= i || menu.nodes[p].type != JWPLC_UI_MENU_SUBMENU)
                    return false;
                depth++;
                p = menu.nodes[p].parent;
            }
            if (depth > JWPLC_UI_MENU_MAX_DEPTH)
                return false;

            if (node.type != JWPLC_UI_MENU_SUBMENU && node.type != JWPLC_UI_MENU_GO_IDLE && node.value == nullptr)
                return false;
            if (node.type == JWPLC_UI_MENU_EDIT_NUM && node.max < node.min)
                return false;
        }
        return true;
    }
}

// ----------------------------------------------------------------------
// Constructores públicos
// ----------------------------------------------------------------------

JWPLC_UIMenuColors::JWPLC_UIMenuColors(
    uint16_t textValue,
    uint16_t backgroundValue,
    uint16_t valueValue,
    uint16_t selectTextValue,
    uint16_t selectBackgroundValue,
    uint16_t editTextValue,
    uint16_t editBackgroundValue,
    uint16_t titleValue,
    uint16_t titleBackgroundValue,
    uint16_t frameValue)
    : text(textValue),
      background(backgroundValue),
      value(valueValue),
      selectText(selectTextValue),
      selectBackground(selectBackgroundValue),
      editText(editTextValue),
      editBackground(editBackgroundValue),
      title(titleValue),
      titleBackground(titleBackgroundValue),
      frame(frameValue)
{
}

static JWPLC_UIMenuNode makeNode(JWPLC_UIMenuNodeType type, JWPLC_UIMenuValueKind kind, uint8_t parent, const char *label, void *value)
{
    JWPLC_UIMenuNode node;
    node.type = type;
    node.kind = kind;
    node.parent = parent;
    node.decimals = 0;
    node.label = label;
    node.value = value;
    node.min = 0.0f;
    node.max = 0.0f;
    node.step = 1.0f;
    node.unit = nullptr;
    node.falseText = "OFF";
    node.trueText = "ON";
    return node;
}

JWPLC_UIMenuNode JWPLC_UIMenuSubmenu(uint8_t parent, const char *label)
{
    return makeNode(JWPLC_UI_MENU_SUBMENU, JWPLC_UI_MENU_VALUE_NONE, parent, label, nullptr);
}

JWPLC_UIMenuNode JWPLC_UIMenuGoIdle(uint8_t parent, const char *label)
{
    return makeNode(JWPLC_UI_MENU_GO_IDLE, JWPLC_UI_MENU_VALUE_NONE, parent, label, nullptr);
}

JWPLC_UIMenuNode JWPLC_UIMenuView(uint8_t parent, const char *label, const float *value, uint8_t decimals, const char *unit)
{
    JWPLC_UIMenuNode node = makeNode(JWPLC_UI_MENU_VIEW, JWPLC_UI_MENU_VALUE_FLOAT, parent, label, const_cast<float *>(value));
    node.decimals = decimals;
    node.unit = unit;
    return node;
}

JWPLC_UIMenuNode JWPLC_UIMenuView(uint8_t parent, const char *label, const int32_t *value, const char *unit)
{
    JWPLC_UIMenuNode node = makeNode(JWPLC_UI_MENU_VIEW, JWPLC_UI_MENU_VALUE_INT32, parent, label, const_cast<int32_t *>(value));
    node.unit = unit;
    return node;
}

JWPLC_UIMenuNode JWPLC_UIMenuView(uint8_t parent, const char *label, const bool *value, const char *falseText, const char *trueText)
{
    JWPLC_UIMenuNode node = makeNode(JWPLC_UI_MENU_VIEW, JWPLC_UI_MENU_VALUE_BOOL, parent, label, const_cast<bool *>(value));
    node.falseText = falseText;
    node.trueText = trueText;
    return node;
}

JWPLC_UIMenuNode JWPLC_UIMenuEditNum(uint8_t parent, const char *label, float *value, float min, float max, float step, uint8_t decimals, const char *unit)
{
    JWPLC_UIMenuNode node = makeNode(JWPLC_UI_MENU_EDIT_NUM, JWPLC_UI_MENU_VALUE_FLOAT, parent, label, value);
    node.min = min;
    node.max = max;
    node.step = (step > 0.0f) ? step : 1.0f;
    node.decimals = decimals;
    node.unit = unit;
    return node;
}

JWPLC_UIMenuNode JWPLC_UIMenuEditNum(uint8_t parent, const char *label, int32_t *value, int32_t min, int32_t max, int32_t step, const char *unit)
{
    JWPLC_UIMenuNode node = makeNode(JWPLC_UI_MENU_EDIT_NUM, JWPLC_UI_MENU_VALUE_INT32, parent, label, value);
    node.min = (float)min;
    node.max = (float)max;
    node.step = (float)((step > 0) ? step : 1);
    node.unit = unit;
    return node;
}

JWPLC_UIMenuNode JWPLC_UIMenuEditBool(uint8_t parent, const char *label, bool *value, const char *falseText, const char *trueText)
{
    JWPLC_UIMenuNode node = makeNode(JWPLC_UI_MENU_EDIT_BOOL, JWPLC_UI_MENU_VALUE_BOOL, parent, label, value);
    node.falseText = falseText;
    node.trueText = trueText;
    return node;
}

JWPLC_UIMenu JWPLC_UIMenuField(
    uint8_t id,
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    const char *title,
    const JWPLC_UIMenuNode *nodes,
    uint8_t nodeCount,
    uint8_t page,
    uint8_t textSize,
    bool frame,
    JWPLC_UIMenuColors colors)
{
    JWPLC_UIMenu menu;
    menu.id = id;
    menu.page = page;
    menu.x = x;
    menu.y = y;
    menu.width = width;
    menu.height = height;
    menu.title = title;
    menu.nodes = nodes;
    menu.nodeCount = nodeCount;
    menu.textSize = textSize;
    menu.frame = frame;
    menu.colors = colors;
    return menu;
}

// ----------------------------------------------------------------------
// Registro
// ----------------------------------------------------------------------

namespace JWPLCUI
{
    bool setMenus(const JWPLC_UIMenu *menus, size_t count)
    {
        if ((menus == nullptr && count != 0) || count > JWPLC_UI_MENU_MAX_MENUS)
            return false;

        for (size_t i = 0; i < count; ++i)
        {
            if (!validMenu(menus[i]))
                return false;
            for (size_t j = 0; j < i; ++j)
            {
                if (menus[i].id == menus[j].id)
                    return false;
            }
        }

        g_menuCount = 0;
        for (size_t i = 0; i < count; ++i)
        {
            g_menus[i] = MenuRuntime();
            g_menus[i].def = menus[i];
            g_menus[i].focused = false;
            g_menus[i].changedNode = 0;
            resetMenu(g_menus[i]);
            g_menuCount++;
        }

        invalidateAll(true);
        return true;
    }

    void clearMenus()
    {
        g_menuCount = 0;
        invalidateAll(true);
    }

    size_t menuCount()
    {
        return g_menuCount;
    }

    bool menuChanged(uint8_t menuId, uint8_t *nodeIndex)
    {
        for (uint8_t i = 0; i < g_menuCount; ++i)
        {
            MenuRuntime &m = g_menus[i];
            if (m.def.id != menuId)
                continue;
            if (m.changedNode == 0)
                return false;
            if (nodeIndex != nullptr)
                *nodeIndex = (uint8_t)(m.changedNode - 1);
            m.changedNode = 0;
            return true;
        }
        return false;
    }

    bool menuEditing()
    {
        for (uint8_t i = 0; i < g_menuCount; ++i)
        {
            if (g_menus[i].editing)
                return true;
        }
        return false;
    }
}

bool JWPLC_DisplayClass::setMenus(const JWPLC_UIMenu *menus, size_t count)
{
    return JWPLCUI::setMenus(menus, count);
}

void JWPLC_DisplayClass::clearMenus()
{
    JWPLCUI::clearMenus();
}

size_t JWPLC_DisplayClass::menuCount() const
{
    return JWPLCUI::menuCount();
}

bool JWPLC_DisplayClass::menuChanged(uint8_t menuId, uint8_t *nodeIndex)
{
    return JWPLCUI::menuChanged(menuId, nodeIndex);
}

bool JWPLC_DisplayClass::menuEditing() const
{
    return JWPLCUI::menuEditing();
}

// ----------------------------------------------------------------------
// Hooks strong (reemplazan los weak de JWPLC_UI_API.cpp)
// ----------------------------------------------------------------------

extern "C" void jwplcUIMenuServiceInput(void)
{
    if (g_menuCount == 0)
        return;

    const uint8_t page = JWPLCUI::currentPage();
    const bool focused = !JWPLCUIPages::selectionMode();
    bool dirty = false;

    for (uint8_t i = 0; i < g_menuCount; ++i)
    {
        MenuRuntime &m = g_menus[i];
        if (m.def.page != page)
        {
            // Salir de la página cancela una edición sin confirmar.
            m.editing = false;
            continue;
        }
        if (m.focused != focused)
        {
            m.focused = focused;
            markRow(m, m.selected[level(m)]);
        }
    }

    MenuRuntime *m = activeMenu(page);
    if (m == nullptr)
        return;

    if (focused && JWPLCButtons::isReady())
    {
        if (m->editing)
            handleEdit(*m);
        else
            handleNavigate(*m);
    }

    pollValues(*m);

    for (uint8_t i = 0; i < g_menuCount; ++i)
    {
        if (g_menus[i].def.page == page && anyDirty(g_menus[i]))
            dirty = true;
    }

    if (dirty)
        JWPLCUI::requestRefresh();
}

extern "C" void jwplcUIMenuPrepareEnter(void)
{
    bool menuOnPage = false;
    const uint8_t page = JWPLCUI::currentPage();
    for (uint8_t i = 0; i < g_menuCount; ++i)
    {
        resetMenu(g_menus[i]);
        menuOnPage = menuOnPage || g_menus[i].def.page == page;
    }

    // Un MENU en la página de entrada es dueño de la botonera (con una sola
    // página tiene el foco al instante): la tecla que despertó la IDLE no debe
    // editar ni moverse en él. Sin MENU los latches siguen siendo del sketch
    // (Alpha8).
    if (menuOnPage)
        JWPLCButtons::clearPendingInput();
}

extern "C" void jwplcUIMenuInvalidateAll(void)
{
    for (uint8_t i = 0; i < g_menuCount; ++i)
        markAll(g_menus[i]);
}

extern "C" void jwplcUIMenuDrawStatic(JWPLC_TFTClass *tft)
{
    if (tft == nullptr)
        return;
    const uint8_t page = JWPLCUI::currentPage();
    for (uint8_t i = 0; i < g_menuCount; ++i)
    {
        if (g_menus[i].def.page == page)
            drawFull(*tft, g_menus[i]);
    }
}

extern "C" void jwplcUIMenuDrawDirty(JWPLC_TFTClass *tft)
{
    if (tft == nullptr)
        return;
    const uint8_t page = JWPLCUI::currentPage();
    for (uint8_t i = 0; i < g_menuCount; ++i)
    {
        if (g_menus[i].def.page == page)
            drawPending(*tft, g_menus[i]);
    }
}
