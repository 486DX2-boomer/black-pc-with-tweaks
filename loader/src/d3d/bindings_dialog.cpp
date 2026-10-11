// Input > Key bindings...: a modal dialog listing the game's actions with two keys each. Click a key cell (or
// select a row and press Enter), then press the new key or mouse button; Esc cancels, Delete clears. A key moved
// to another action is cleared where it was. OK saves to black.ini.
#include "input/bindings.h"

#include "common.h"
#include "settings.h"

#include <commctrl.h>

#include <string>
#include <vector>

namespace bindings {
namespace {

enum : int { ID_LIST = 100, ID_STATUS, ID_CLEAR, ID_DEFAULTS };
constexpr UINT WM_APP_START = WM_APP + 1; // start capturing (wParam row, lParam slot) after the list's click handling

struct State {
    HWND dlg, list, status;
    std::vector<uint16_t> keys; // action * kSlots + slot, edited here until OK
    const Action *acts;
    int count;
    int cap_row = -1, cap_slot = 0; // capturing a key for this cell
    bool wait_up;                   // the click that started capturing is still down
    DWORD cap_start;                // GetTickCount at the start: the left button only counts after a double-click time
    int slot = 0;                   // the key column last clicked (for Clear and Enter)
    WNDPROC list_proc;
};
State *g_s;

void set_status(const std::wstring &text) { SetWindowTextW(g_s->status, text.c_str()); }

std::wstring cell_text(int row, int slot)
{
    if (g_s->cap_row == row && g_s->cap_slot == slot) return L"press a key...";
    std::wstring t = key_name(g_s->keys[row * kSlots + slot]);
    if (fixed(row, slot)) t += L" (always)";
    return t;
}

void refresh_row(int row)
{
    for (int slot = 0; slot < kSlots; slot++) {
        std::wstring t = cell_text(row, slot);
        LVITEMW it = {};
        it.iSubItem = slot + 1;
        it.pszText = (wchar_t *)t.c_str();
        SendMessageW(g_s->list, LVM_SETITEMTEXTW, row, (LPARAM)&it);
    }
}

void refresh_all()
{
    for (int r = 0; r < g_s->count; r++) refresh_row(r);
}

int selected_row() { return (int)SendMessageW(g_s->list, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED); }

void stop_capture()
{
    int row = g_s->cap_row;
    g_s->cap_row = -1;
    if (GetCapture() == g_s->list) ReleaseCapture();
    if (row >= 0) refresh_row(row);
}

void start_capture(int row, int slot)
{
    if (row < 0) return;
    if (fixed(row, slot)) {
        set_status(L"Esc always pauses the game (and frees the mouse). Use the second key for another pause key.");
        return;
    }
    stop_capture();
    g_s->cap_row = row;
    g_s->cap_slot = slot;
    g_s->wait_up = GetKeyState(VK_LBUTTON) < 0;
    g_s->cap_start = GetTickCount();
    refresh_row(row);
    SetFocus(g_s->list);
    SetCapture(g_s->list); // mouse buttons anywhere are candidates
    set_status(L"Press a key or mouse button for \"" + std::wstring(g_s->acts[row].name) + L"\". Esc cancels.");
}

void bind(uint16_t k)
{
    int row = g_s->cap_row, slot = g_s->cap_slot;
    stop_capture();
    if (reserved(k)) {
        set_status(key_name(k) + L" is one of the port's own keys (F2-F12). Choose another key.");
        return;
    }
    std::wstring note;
    for (int r = 0; r < g_s->count; r++)
        for (int s = 0; s < kSlots; s++)
            if ((r != row || s != slot) && g_s->keys[r * kSlots + s] == k && !fixed(r, s)) {
                g_s->keys[r * kSlots + s] = 0;
                refresh_row(r);
                note = L" (taken from \"" + std::wstring(g_s->acts[r].name) + L"\")";
            }
    g_s->keys[row * kSlots + slot] = k;
    refresh_row(row);
    std::wstring menus;
    if (k == VK_UP || k == VK_DOWN || k == VK_LEFT || k == VK_RIGHT || k == VK_RETURN || k == VK_BACK)
        menus = L" In menus this key keeps navigating.";
    set_status(std::wstring(g_s->acts[row].name) + L": " + key_name(k) + note + L"." + menus);
}

uint16_t key_from_msg(WPARAM vk, LPARAM lp)
{
    UINT sc = (lp >> 16) & 0xFF;
    bool ext = (lp & (1 << 24)) != 0;
    switch (vk) {
    case VK_SHIFT: return (uint16_t)MapVirtualKeyW(sc, MAPVK_VSC_TO_VK_EX);
    case VK_CONTROL: return ext ? VK_RCONTROL : VK_LCONTROL;
    case VK_MENU: return ext ? VK_RMENU : VK_LMENU;
    }
    return (uint16_t)vk;
}

LRESULT CALLBACK list_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (g_s->cap_row >= 0) {
        switch (msg) {
        case WM_GETDLGCODE: return DLGC_WANTALLKEYS; // Esc, Enter and Tab reach us, not the dialog
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wp == VK_ESCAPE) {
                stop_capture();
                set_status(L"Unchanged.");
            } else {
                bind(key_from_msg(wp, lp));
            }
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
        case WM_CHAR:
        case WM_SYSCHAR: return 0;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: // not the second click of the double-click that may have started this
            if (!g_s->wait_up && GetTickCount() - g_s->cap_start > GetDoubleClickTime()) bind(VK_LBUTTON);
            return 0;
        case WM_LBUTTONUP:
            g_s->wait_up = false;
            return 0;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK: bind(VK_RBUTTON); return 0;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK: bind(VK_MBUTTON); return 0;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK: bind(GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2); return TRUE;
        case WM_MOUSEWHEEL: bind(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? KEY_WHEEL_UP : KEY_WHEEL_DOWN); return 0;
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        case WM_MOUSEMOVE: return 0;
        case WM_KILLFOCUS:
            stop_capture();
            set_status(L"Unchanged.");
            break;
        }
    } else if (msg == WM_KEYDOWN && wp == VK_DELETE) {
        int row = selected_row();
        if (row >= 0 && !fixed(row, g_s->slot)) {
            g_s->keys[row * kSlots + g_s->slot] = 0;
            refresh_row(row);
        }
        return 0;
    }
    return CallWindowProcW(g_s->list_proc, h, msg, wp, lp);
}

HWND child(const wchar_t *cls, const wchar_t *text, DWORD style, int id, int x, int y, int w, int h)
{
    RECT r = {x, y, x + w, y + h};
    MapDialogRect(g_s->dlg, &r);
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, r.left, r.top, r.right - r.left, r.bottom - r.top,
                             g_s->dlg, (HMENU)(INT_PTR)id, GetModuleHandleW(L"blackrt.dll"), nullptr);
    SendMessageW(c, WM_SETFONT, SendMessageW(g_s->dlg, WM_GETFONT, 0, 0), FALSE);
    return c;
}

void init(HWND dlg)
{
    g_s->dlg = dlg;
    child(L"STATIC",
          L"Keys and mouse buttons for the game's actions; a controller works as well. Click a key, then press the "
          L"new key or mouse button. In menus the arrow keys, Enter and Backspace always navigate.",
          0, -1, 7, 6, 296, 26);
    g_s->list = child(WC_LISTVIEWW, L"", WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS |
                      LVS_NOSORTHEADER, ID_LIST, 7, 34, 296, 192);
    g_s->status = child(L"STATIC", L"", SS_LEFT, ID_STATUS, 7, 230, 296, 18);
    child(L"BUTTON", L"&Clear", WS_TABSTOP | BS_PUSHBUTTON, ID_CLEAR, 7, 252, 56, 15);
    child(L"BUTTON", L"&Defaults", WS_TABSTOP | BS_PUSHBUTTON, ID_DEFAULTS, 67, 252, 56, 15);
    child(L"BUTTON", L"OK", WS_TABSTOP | BS_PUSHBUTTON, IDOK, 187, 252, 56, 15);
    child(L"BUTTON", L"Cancel", WS_TABSTOP | BS_PUSHBUTTON, IDCANCEL, 247, 252, 56, 15);

    SendMessageW(g_s->list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    RECT rc;
    GetClientRect(g_s->list, &rc);
    int w = rc.right - GetSystemMetrics(SM_CXVSCROLL);
    const wchar_t *titles[3] = {L"Action", L"Key", L"Second key"};
    int widths[3] = {w * 40 / 100, w * 30 / 100, w - w * 40 / 100 - w * 30 / 100};
    for (int c = 0; c < 3; c++) {
        LVCOLUMNW col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.pszText = (wchar_t *)titles[c];
        col.cx = widths[c];
        col.iSubItem = c;
        SendMessageW(g_s->list, LVM_INSERTCOLUMNW, c, (LPARAM)&col);
    }
    for (int r = 0; r < g_s->count; r++) {
        LVITEMW it = {};
        it.mask = LVIF_TEXT;
        it.iItem = r;
        it.pszText = (wchar_t *)g_s->acts[r].name;
        SendMessageW(g_s->list, LVM_INSERTITEMW, 0, (LPARAM)&it);
    }
    refresh_all();
    g_s->list_proc = (WNDPROC)SetWindowLongPtrW(g_s->list, GWLP_WNDPROC, (LONG_PTR)list_proc);
    SetFocus(g_s->list);
}

// The cell under the cursor of the last click.
void hit_cell(int *row, int *col)
{
    DWORD pos = GetMessagePos();
    LVHITTESTINFO hit = {};
    hit.pt = {(short)LOWORD(pos), (short)HIWORD(pos)};
    ScreenToClient(g_s->list, &hit.pt);
    SendMessageW(g_s->list, LVM_SUBITEMHITTEST, 0, (LPARAM)&hit);
    *row = hit.iItem;
    *col = hit.iSubItem;
}

INT_PTR CALLBACK dlg_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG:
        init(dlg);
        return FALSE; // focus set
    case WM_NOTIFY: {
        auto *nm = (NMHDR *)lp;
        if (nm->idFrom == ID_LIST && nm->code == NM_CLICK && g_s->cap_row < 0) {
            int row, col;
            hit_cell(&row, &col);
            if (row >= 0 && col >= 1) {
                g_s->slot = col - 1;
                PostMessageW(dlg, WM_APP_START, row, col - 1);
            }
        }
        return TRUE;
    }
    case WM_APP_START:
        start_capture((int)wp, (int)lp);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDOK:
            if (GetFocus() == g_s->list) { // Enter on a row: change its key
                start_capture(selected_row(), g_s->slot);
                return TRUE;
            }
            EndDialog(dlg, IDOK);
            return TRUE;
        case IDCANCEL: EndDialog(dlg, IDCANCEL); return TRUE;
        case ID_CLEAR: {
            int row = selected_row();
            if (row >= 0 && !fixed(row, g_s->slot)) {
                g_s->keys[row * kSlots + g_s->slot] = 0;
                refresh_row(row);
            }
            return TRUE;
        }
        case ID_DEFAULTS:
            for (int r = 0; r < g_s->count; r++)
                for (int s = 0; s < kSlots; s++) g_s->keys[r * kSlots + s] = g_s->acts[r].defaults[s];
            refresh_all();
            set_status(L"Default keys (OK to keep them).");
            return TRUE;
        }
        break;
    }
    return FALSE;
}

} // namespace

void dialog(void *owner_hwnd)
{
    static bool loaded;
    if (!loaded) { // the list view (common controls 6 through the UI thread's activation context, host.cpp)
        loaded = true;
        using Init = BOOL(WINAPI *)(const INITCOMMONCONTROLSEX *);
        INITCOMMONCONTROLSEX icc = {sizeof icc, ICC_LISTVIEW_CLASSES};
        if (auto init = (Init)GetProcAddress(LoadLibraryW(L"comctl32.dll"), "InitCommonControlsEx")) init(&icc);
    }
    State s;
    s.acts = actions(&s.count);
    for (int r = 0; r < s.count; r++)
        for (int k = 0; k < kSlots; k++) s.keys.push_back(key(r, k));
    g_s = &s;

    std::vector<WORD> t; // DLGTEMPLATE without items (the controls are made in WM_INITDIALOG), then title and font
    auto dw = [&](DWORD v) {
        t.push_back(LOWORD(v));
        t.push_back(HIWORD(v));
    };
    auto str = [&](const wchar_t *p) {
        while (*p) t.push_back(*p++);
        t.push_back(0);
    };
    dw(DS_MODALFRAME | DS_SETFONT | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    dw(0);
    t.push_back(0); // no items
    t.push_back(0);
    t.push_back(0);
    t.push_back(310); // size in dialog units
    t.push_back(274);
    t.push_back(0); // no menu
    t.push_back(0); // standard class
    str(L"Key bindings");
    t.push_back(9);
    str(L"Segoe UI");
    INT_PTR r = DialogBoxIndirectParamW(GetModuleHandleW(L"blackrt.dll"), (const DLGTEMPLATE *)t.data(), (HWND)owner_hwnd,
                                        dlg_proc, 0);
    g_s = nullptr;
    if (r != IDOK) return;
    for (int row = 0; row < s.count; row++)
        for (int k = 0; k < kSlots; k++) set_key(row, k, s.keys[row * kSlots + k]);
    settings_save();
    logf("[input] key bindings changed");
}

} // namespace bindings
