
// gui.c  –  Windows 3.1-style TUI desktop for Matthew OS
// VGA text mode 80x25, CP437, no VESA required.
// ─────────────────────────────────────────────────────────────────────────────

// ══════════════════════════════════════════════════════════════════════════════
// SECTION 1 – Color palette & attribute macros
// ══════════════════════════════════════════════════════════════════════════════
#define GA(fg,bg)  ((u8)(((bg)<<4)|(fg)))

#define CK   0   // black
#define CB   1   // blue
#define CG   2   // green
#define CC   3   // cyan
#define CR   4   // red
#define CM   5   // magenta
#define CW   6   // brown/olive
#define CLG  7   // light gray
#define CDG  8   // dark gray
#define CLB  9   // light blue
#define CLG2 10  // light green
#define CLC  11  // light cyan
#define CLR  12  // light red
#define CLM  13  // light magenta
#define CY   14  // yellow
#define CWH  15  // white

// Win3.1 palette
#define A_DESKTOP      GA(CLB, CB)      // teal desktop
#define A_TASKBAR      GA(CK,  CLG)     // gray taskbar
#define A_TASKBAR_CLK  GA(CWH, CK)     // clock on taskbar
#define A_TITLEBAR_ACT GA(CWH, CB)     // active title  (blue)
#define A_TITLEBAR_INA GA(CLG, CDG)    // inactive title (gray)
#define A_TITLE_TEXT   GA(CWH, CB)     // title text
#define A_WIN_BODY     GA(CK,  CLG)    // window client area
#define A_WIN_BORDER   GA(CWH, CLG)    // window border
#define A_MENUBAR      GA(CK,  CLG)    // menu bar
#define A_MENUBAR_SEL  GA(CWH, CB)     // selected menu item
#define A_MENU_DROP    GA(CK,  CLG)    // dropdown body
#define A_MENU_SEL     GA(CWH, CB)     // dropdown selected item
#define A_MENU_SEP     GA(CDG, CLG)    // separator
#define A_BTN          GA(CK,  CLG)    // button face
#define A_BTN_FOCUS    GA(CWH, CB)     // focused button
#define A_BTN_SHADOW   GA(CDG, CK)     // button shadow
#define A_SCROLLBAR    GA(CDG, CLG)    // scrollbar track
#define A_SCROLLTHUMB  GA(CK,  CLG)    // scrollbar thumb
#define A_SHADOW       GA(CDG, CK)     // window shadow
#define A_STATUSBAR    GA(CK,  CLG)    // status bar
#define A_EDIT         GA(CK,  CWH)    // edit box (white bg)
#define A_EDIT_FOCUS   GA(CK,  CWH)
#define A_LISTBOX      GA(CK,  CWH)    // list box
#define A_LISTBOX_SEL  GA(CWH, CB)
#define A_ICON_FACE    GA(CWH, CB)     // icon glyph
#define A_ICON_LABEL   GA(CWH, CB)     // icon label
#define A_ICON_SEL     GA(CK,  CY)     // selected icon
#define A_ICON_LSEL    GA(CK,  CY)
#define A_PROGBAR      GA(CWH, CB)     // progress bar fill
#define A_PROGBAR_BG   GA(CDG, CLG)    // progress bar bg
#define A_ERR_TITLE    GA(CWH, CR)     // error dialog title
#define A_WARN_TITLE   GA(CK,  CY)     // warning dialog title

// ══════════════════════════════════════════════════════════════════════════════
// SECTION 2 – CP437 box-drawing & special characters
// ══════════════════════════════════════════════════════════════════════════════
#define CH_TL    0xDA  // ┌
#define CH_TR    0xBF  // ┐
#define CH_BL    0xC0  // └
#define CH_BR    0xD9  // ┘
#define CH_H     0xC4  // ─
#define CH_V     0xB3  // │
#define CH_TLC   0xC9  // ╔  (double)
#define CH_TRC   0xBB  // ╗
#define CH_BLC   0xC8  // ╚
#define CH_BRC   0xBC  // ╝
#define CH_DH    0xCD  // ═
#define CH_DV    0xBA  // ║
#define CH_FILL  0xB0  // ░  desktop fill
#define CH_HALF  0xB1  // ▒  shadow
#define CH_SOLID 0xDB  // █
#define CH_UP    0x1E  // ▲
#define CH_DN    0x1F  // ▼
#define CH_LT    0x11  // ◄
#define CH_RT    0x10  // ►
#define CH_BULL  0x07  // •
#define CH_CHCK  0xFB  // √
#define CH_DIAM  0x04  // ♦
#define CH_SMILEY 0x01 // ☺
#define CH_NOTE  0x0E  // ♫
#define CH_SUN   0x0F  // ☼
#define CH_SPADE 0x06  // ♠
#define CH_CLUB  0x05  // ♣
#define CH_HEART 0x03  // ♥
#define CH_CLOSE 0xFE  // ■  (close button glyph)
#define CH_MAX   0x18  // ↑  (maximize)
#define CH_REST  0x12  // ↕  (restore)
#define CH_MIN   0x19  // ↓  (minimize)

// ══════════════════════════════════════════════════════════════════════════════
// SECTION 3 – Primitive drawing
// ══════════════════════════════════════════════════════════════════════════════
static void gp(int x, int y, u8 ch, u8 at) {
    if ((unsigned)x >= COLS || (unsigned)y >= ROWS) return;
    dispchar(chr(ch, at), (u16)((y * COLS + x) * 2));
}
static void gfill(int x, int y, int w, int h, u8 ch, u8 at) {
    for (int r = y; r < y+h; r++)
        for (int c = x; c < x+w; c++)
            gp(c, r, ch, at);
}
static void gstr(int x, int y, const char *s, u8 at) {
    for (int i = 0; s[i]; i++) gp(x+i, y, (u8)s[i], at);
}
static void gstrc(int x, int y, int w, const char *s, u8 at) {
    int l = 0; while (s[l]) l++;
    int pad = (w - l) / 2; if (pad < 0) pad = 0;
    gfill(x, y, w, 1, ' ', at);
    gstr(x + pad, y, s, at);
}
static int gstrlen(const char *s) { int n=0; while(s[n]) n++; return n; }

// ══════════════════════════════════════════════════════════════════════════════
// SECTION 4 – Window frame (Win3.1 style)
// ══════════════════════════════════════════════════════════════════════════════
// Draws a complete window frame:
//   - Title bar (active/inactive color)
//   - [■] close button on left, [▲][▼] on right
//   - Single-line border
//   - Drop shadow
//   - Client area filled with A_WIN_BODY
static void gwin(int x, int y, int w, int h, const char *title, int active) {
    u8 ta = active ? A_TITLEBAR_ACT : A_TITLEBAR_INA;
    u8 ba = A_WIN_BORDER;
    u8 ca = A_WIN_BODY;

    // Shadow (draw first, behind window)
    for (int r = 1; r < h+1; r++) gp(x+w, y+r, CH_HALF, A_SHADOW);
    for (int c = 1; c <= w;   c++) gp(x+c, y+h, CH_HALF, A_SHADOW);

    // Client area
    gfill(x+1, y+1, w-2, h-2, ' ', ca);

    // Border
    for (int c = 1; c < w-1; c++) { gp(x+c, y,   CH_H, ba); gp(x+c, y+h-1, CH_H, ba); }
    for (int r = 1; r < h-1; r++) { gp(x,   y+r, CH_V, ba); gp(x+w-1, y+r, CH_V, ba); }
    gp(x,     y,     CH_TL, ba);
    gp(x+w-1, y,     CH_TR, ba);
    gp(x,     y+h-1, CH_BL, ba);
    gp(x+w-1, y+h-1, CH_BR, ba);

    // Title bar (row y, inside border)
    gfill(x+1, y, w-2, 1, ' ', ta);

    // Close button [■] at left of title bar
    gp(x+1, y, '[', ta);
    gp(x+2, y, CH_CLOSE, ta);
    gp(x+3, y, ']', ta);

    // Maximize [▲] and minimize [▼] at right
    gp(x+w-4, y, '[', ta);
    gp(x+w-3, y, CH_MAX, ta);
    gp(x+w-2, y, ']', ta);

    // Title text (centered between buttons)
    int title_area_x = x + 5;
    int title_area_w = w - 10;
    if (title_area_w > 0) gstrc(title_area_x, y, title_area_w, title, ta);
}

// Thin dialog frame (double border, no title buttons)
static void gdialog(int x, int y, int w, int h, const char *title, u8 ta) {
    u8 ba = ta;
    u8 ca = A_WIN_BODY;

    // Shadow
    for (int r = 1; r < h+1; r++) gp(x+w, y+r, CH_HALF, A_SHADOW);
    for (int c = 1; c <= w;   c++) gp(x+c, y+h, CH_HALF, A_SHADOW);

    gfill(x+1, y+1, w-2, h-2, ' ', ca);

    for (int c = 1; c < w-1; c++) { gp(x+c, y,   CH_DH, ba); gp(x+c, y+h-1, CH_DH, ba); }
    for (int r = 1; r < h-1; r++) { gp(x,   y+r, CH_DV, ba); gp(x+w-1, y+r, CH_DV, ba); }
    gp(x,     y,     CH_TLC, ba);
    gp(x+w-1, y,     CH_TRC, ba);
    gp(x,     y+h-1, CH_BLC, ba);
    gp(x+w-1, y+h-1, CH_BRC, ba);

    if (title) gstrc(x+1, y, w-2, title, ta);
}

// ══════════════════════════════════════════════════════════════════════════════
// SECTION 5 – Button widget
// ══════════════════════════════════════════════════════════════════════════════
static void gbtn(int x, int y, const char *label, int focused) {
    u8 fa = focused ? A_BTN_FOCUS : A_BTN;
    int l = gstrlen(label);
    // [ label ]
    gp(x,     y, '[', fa);
    gp(x+1,   y, ' ', fa);
    gstr(x+2, y, label, fa);
    gp(x+2+l, y, ' ', fa);
    gp(x+3+l, y, ']', fa);
    // shadow
    for (int i = 0; i < l+4; i++) gp(x+1+i, y+1, CH_HALF, A_BTN_SHADOW);
}

// ══════════════════════════════════════════════════════════════════════════════
// SECTION 6 – Scrollbar
// ══════════════════════════════════════════════════════════════════════════════
static void gscrollbar_v(int x, int y, int h, int pos, int total) {
    gp(x, y,     CH_UP, A_SCROLLBAR);
    gp(x, y+h-1, CH_DN, A_SCROLLBAR);
    for (int i = 1; i < h-1; i++) gp(x, y+i, CH_HALF, A_SCROLLBAR);
    if (total > 0 && h > 2) {
        int thumb = 1 + (pos * (h-3)) / total;
        if (thumb < 1) thumb = 1;
        if (thumb > h-2) thumb = h-2;
        gp(x, y+thumb, CH_SOLID, A_SCROLLTHUMB);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// SECTION 7 – Progress bar
// ══════════════════════════════════════════════════════════════════════════════
static void gprogbar(int x, int y, int w, int pct) {
    int filled = (w * pct) / 100;
    for (int i = 0; i < w; i++)
        gp(x+i, y, CH_SOLID, i < filled ? A_PROGBAR : A_PROGBAR_BG);
}

// ══════════════════════════════════════════════════════════════════════════════
// SECTION 8 – Desktop & taskbar
// ══════════════════════════════════════════════════════════════════════════════
static void gui_draw_desktop(void) {
    for (int r = 0; r < ROWS-1; r++)
        for (int c = 0; c < COLS; c++)
            gp(c, r, CH_FILL, A_DESKTOP);
}

// Taskbar: bottom row with real-time clock from time_ms
static void gui_draw_taskbar(void) {
    gfill(0, ROWS-1, COLS, 1, ' ', A_TASKBAR);
    gstr(0, ROWS-1, "[Start]", GA(CK, CLG));
    gp(7, ROWS-1, CH_V, GA(CDG, CLG));
    gstr(9, ROWS-1, "Matthew OS  |  Program Manager", A_TASKBAR);

    // Real-time clock: derive HH:MM:SS from time_ms (wraps at 24h)
    u32 total_s = time_ms / 1000;
    u32 hh = (total_s / 3600) % 24;
    u32 mm = (total_s / 60) % 60;
    u32 ss = total_s % 60;
    char clk[10];
    clk[0] = '0' + hh/10; clk[1] = '0' + hh%10;
    clk[2] = ':';
    clk[3] = '0' + mm/10; clk[4] = '0' + mm%10;
    clk[5] = ':';
    clk[6] = '0' + ss/10; clk[7] = '0' + ss%10;
    clk[8] = 0;
    gstr(71, ROWS-1, clk, A_TASKBAR_CLK);
}

// ══════════════════════════════════════════════════════════════════════════════
// SECTION 9 – Desktop icons (Win3.1 Program Manager style)
// ══════════════════════════════════════════════════════════════════════════════
#define MAX_ICONS 10
typedef struct { int x, y; u8 glyph; const char *label; } GIcon;
static GIcon icons[MAX_ICONS] = {
    {  2,  3, CH_DIAM,  "FileMgr"  },
    { 11,  3, CH_NOTE,  "Notepad"  },
    { 20,  3, CH_BULL,  "Calc"     },
    { 29,  3, CH_SUN,   "Sysinfo"  },
    { 38,  3, CH_HEART, "About"    },
    { 47,  3, CH_SPADE, "Snake"    },
    { 56,  3, CH_CLUB,  "HexView"  },
    { 65,  3, CH_CHCK,  "AscView"  },
    {  2, 10, CH_DIAM,  "MemView"  },
    { 11, 10, CH_RT,    "Reboot"   },
};

static void gdraw_icon(int i, int sel) {
    GIcon *ic = &icons[i];
    u8 ga = sel ? A_ICON_SEL  : A_ICON_FACE;
    u8 la = sel ? A_ICON_LSEL : A_ICON_LABEL;
    // 3-wide icon box
    gfill(ic->x, ic->y, 7, 2, ' ', ga);
    gp(ic->x + 3, ic->y, ic->glyph, ga);
    // label (up to 8 chars, centered in 7)
    gstrc(ic->x, ic->y+2, 7, ic->label, la);
}
static void gdraw_all_icons(int sel) {
    for (int i = 0; i < MAX_ICONS; i++) gdraw_icon(i, i==sel);
}

// ══════════════════════════════════════════════════════════════════════════════
// SECTION 10 – Menu bar & dropdown (Win3.1 style)
// ══════════════════════════════════════════════════════════════════════════════
// Menu bar is drawn inside a window's client area at y=1 (relative to window)
// For the Program Manager we draw it at absolute row 6 (inside the main window)

#define MENU_ITEMS 4
static const char *menu_labels[MENU_ITEMS] = { " File ", " Options ", " Window ", " Help " };
static int menu_x[MENU_ITEMS] = { 1, 8, 19, 28 };

static void gdraw_menubar(int wy, int sel) {
    gfill(1, wy, COLS-2, 1, ' ', A_MENUBAR);
    for (int i = 0; i < MENU_ITEMS; i++) {
        u8 a = (i == sel) ? A_MENUBAR_SEL : A_MENUBAR;
        gstr(1 + menu_x[i], wy, menu_labels[i], a);
    }
}

// Dropdown for "File" menu
static const char *file_menu[] = {
    " Run...        ",
    " Open          ",
    "---------------",
    " Exit          ",
    0
};
// Dropdown for "Help" menu
static const char *help_menu[] = {
    " About...      ",
    " Contents      ",
    0
};

static void gdraw_dropdown(int x, int y, const char **items, int sel) {
    int n = 0; while (items[n]) n++;
    int w = 17;
    // shadow
    for (int r = 1; r <= n+2; r++) gp(x+w, y+r, CH_HALF, A_SHADOW);
    for (int c = 1; c <= w;   c++) gp(x+c, y+n+2, CH_HALF, A_SHADOW);
    // box
    gfill(x, y, w, n+2, ' ', A_MENU_DROP);
    gp(x,   y,     CH_TL, A_WIN_BORDER);
    gp(x+w-1, y,   CH_TR, A_WIN_BORDER);
    gp(x,   y+n+1, CH_BL, A_WIN_BORDER);
    gp(x+w-1, y+n+1, CH_BR, A_WIN_BORDER);
    for (int c = 1; c < w-1; c++) { gp(x+c, y, CH_H, A_WIN_BORDER); gp(x+c, y+n+1, CH_H, A_WIN_BORDER); }
    for (int r = 1; r <= n;  r++) { gp(x, y+r, CH_V, A_WIN_BORDER); gp(x+w-1, y+r, CH_V, A_WIN_BORDER); }
    // items
    for (int i = 0; i < n; i++) {
        u8 a = (i == sel) ? A_MENU_SEL : A_MENU_DROP;
        if (items[i][1] == '-') {
            // separator line
            for (int c = 1; c < w-1; c++) gp(x+c, y+1+i, CH_H, A_MENU_SEP);
        } else {
            gstr(x+1, y+1+i, items[i], a);
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// SECTION 11 – Built-in Applications
// ══════════════════════════════════════════════════════════════════════════════

// ── 11a. File Manager ─────────────────────────────────────────────────────────
static void app_filemanager(void) {
    int wx=2, wy=5, ww=76, wh=17;
    gwin(wx, wy, ww, wh, "File Manager - A:\\", 1);

    // Column headers
    u8 ha = GA(CK, CLG);
    gfill(wx+1, wy+1, ww-2, 1, ' ', ha);
    gstr(wx+2,  wy+1, "Name           Size      Date      Time  Attr", ha);
    // Separator line
    for (int c = wx+1; c < wx+ww-1; c++) gp(c, wy+2, CH_H, ha);

    // Status bar inside window
    gfill(wx+1, wy+wh-2, ww-2, 1, ' ', A_STATUSBAR);
    gstr(wx+2, wy+wh-2, "Reading disk...", A_STATUSBAR);

    // Scrollbar
    gscrollbar_v(wx+ww-2, wy+3, wh-5, 0, 10);

    // Load real directory
    int n = fat_list_dir();
    gfill(wx+1, wy+wh-2, ww-2, 1, ' ', A_STATUSBAR);
    if (n < 0) {
        gstr(wx+2, wy+wh-2, "Disk read error!", GA(CLR, CLG));
    } else {
        char sbuf[40];
        // print count into sbuf
        int si = 0;
        sbuf[si++] = '0' + (n / 10 % 10);
        sbuf[si++] = '0' + (n % 10);
        sbuf[si++] = ' '; sbuf[si++] = 'f'; sbuf[si++] = 'i';
        sbuf[si++] = 'l'; sbuf[si++] = 'e'; sbuf[si++] = '(';
        sbuf[si++] = 's'; sbuf[si++] = ')'; sbuf[si++] = 0;
        gstr(wx+2, wy+wh-2, sbuf, A_STATUSBAR);
    }

    int sel = 0;
    int scroll = 0;
    int visible = wh - 6;  // rows available for listing

    // Draw list
    #define FM_REDRAW() do { \
        gfill(wx+1, wy+3, ww-3, visible, ' ', A_LISTBOX); \
        for (int _i = 0; _i < visible && (scroll+_i) < n; _i++) { \
            FatEntry *fe = &fat_entries[scroll+_i]; \
            u8 _a = (scroll+_i == sel) ? A_LISTBOX_SEL : A_LISTBOX; \
            gfill(wx+1, wy+3+_i, ww-3, 1, ' ', _a); \
            if (fe->attr & FAT_ATTR_DIRECTORY) { \
                gp(wx+2, wy+3+_i, '[', _a); \
                gstr(wx+3, wy+3+_i, fe->name, _a); \
                int _l = gstrlen(fe->name); \
                gp(wx+3+_l, wy+3+_i, ']', _a); \
                gstr(wx+17, wy+3+_i, "<DIR>", _a); \
            } else { \
                gstr(wx+2, wy+3+_i, fe->name, _a); \
                char _sb[12]; int _si=0; u32 _sv=fe->size; \
                if(_sv==0){_sb[0]='0';_sb[1]=0;} \
                else{while(_sv>0&&_si<10){_sb[_si++]='0'+(_sv%10);_sv/=10;}_sb[_si]=0;my_reverse(_sb,_si);} \
                gstr(wx+17, wy+3+_i, _sb, _a); \
                char _dt[10]; fat_format_date(fe->date, _dt); \
                gstr(wx+27, wy+3+_i, _dt, _a); \
                char _tm[8]; fat_format_time(fe->time, _tm); \
                gstr(wx+37, wy+3+_i, _tm, _a); \
            } \
        } \
        gscrollbar_v(wx+ww-2, wy+3, visible+2, sel, n > 0 ? n-1 : 0); \
    } while(0)

    FM_REDRAW();

    while (1) {
        char k = get_key();
        if (k == 27) break;
        if (k == 'k' || k == 'K' || k == KEY_UP || k == 'w' || k == 'W') {
            if (sel > 0) { sel--;
                if (sel < scroll) scroll = sel;
                FM_REDRAW(); }
        } else if (k == 'j' || k == 'J' || k == KEY_DOWN || k == 's' || k == 'S') {
            if (n > 0 && sel < n-1) { sel++;
                if (sel >= scroll + visible) scroll = sel - visible + 1;
                FM_REDRAW(); }
        } else if (k == '\n' || k == '\r') {
            // TYPE the selected file
            if (n > 0 && sel < n) {
                FatEntry *fe = &fat_entries[sel];
                if (!(fe->attr & FAT_ATTR_DIRECTORY)) {
                    // Open a viewer sub-window
                    int vx=5, vy=6, vw=70, vh=14;
                    gwin(vx, vy, vw, vh, fe->name, 1);
                    // redirect output into window
                    int saved_x = cur_x, saved_y = cur_y;
                    cur_x = vx+1; cur_y = vy+1;
                    int ret = fat_cmd_type(fe->name);
                    if (ret != 0) {
                        gstr(vx+2, vy+2, "Cannot display binary file.", A_WIN_BODY);
                    }
                    cur_x = saved_x; cur_y = saved_y;
                    gbtn(vx+vw/2-4, vy+vh-2, "Close", 1);
                    while (1) { char k2 = get_key(); if (k2==27||k2=='\n'||k2=='\r') break; }
                    // Redraw file manager
                    gwin(wx, wy, ww, wh, "File Manager - A:\\", 1);
                    gfill(wx+1, wy+1, ww-2, 1, ' ', ha);
                    gstr(wx+2, wy+1, "Name           Size      Date      Time  Attr", ha);
                    for (int c = wx+1; c < wx+ww-1; c++) gp(c, wy+2, CH_H, ha);
                    FM_REDRAW();
                }
            }
        }
    }
    #undef FM_REDRAW
}

// ── 11b. Notepad ──────────────────────────────────────────────────────────────
// Notepad line buffer at fixed address (above FAT buffers)
// FAT uses 0xA000-0xB3FF, notepad uses 0xB400 (below video at 0xB8000)
#define NP_BUF_BASE  0xB400
#define NP_LINES     10
#define NP_COLS      66

static void app_notepad(void) {
    int wx=5, wy=4, ww=70, wh=17;
    gwin(wx, wy, ww, wh, "Notepad - (untitled)", 1);
    gfill(wx+1, wy+1, ww-2, 1, ' ', A_MENUBAR);
    gstr(wx+2, wy+1, " File  Edit  Help ", A_MENUBAR);
    gfill(wx+1, wy+2, ww-2, wh-4, ' ', A_EDIT);
    gfill(wx+1, wy+wh-2, ww-2, 1, ' ', A_STATUSBAR);
    gstr(wx+2, wy+wh-2, "ESC=Close  ENTER=newline  BKSP=delete", A_STATUSBAR);

    // Use fixed memory for line storage
    char (*lines)[NP_COLS] = (char (*)[NP_COLS])NP_BUF_BASE;
    // Zero out the buffer
    for (int i = 0; i < NP_LINES * NP_COLS; i++)
        ((char*)NP_BUF_BASE)[i] = 0;

    int num_lines = 1, cur_line = 0, cur_col = 0;

    while (1) {
        // Redraw edit area
        gfill(wx+1, wy+2, ww-2, wh-4, ' ', A_EDIT);
        for (int l = 0; l < num_lines && l < wh-4; l++)
            gstr(wx+1, wy+2+l, lines[l], A_EDIT);
        // Cursor
        u8 cc = (cur_col < NP_COLS-1 && lines[cur_line][cur_col]) ?
                (u8)lines[cur_line][cur_col] : ' ';
        gp(wx+1+cur_col, wy+2+cur_line, cc, GA(CWH, CK));

        char k = get_key();
        if (k == 27) break;
        if (k == '\n' || k == '\r') {
            if (cur_line < NP_LINES-1) {
                cur_line++; cur_col = 0;
                if (cur_line >= num_lines) num_lines = cur_line + 1;
            }
        } else if (k == '\b') {
            if (cur_col > 0) { cur_col--; lines[cur_line][cur_col] = 0; }
        } else if (k >= 32 && k <= 126 && cur_col < NP_COLS-2) {
            lines[cur_line][cur_col++] = k;
        }
    }
}

// ── 11c. Calculator ───────────────────────────────────────────────────────────
static void app_calc(void) {
    int wx=28, wy=6, ww=24, wh=14;
    gwin(wx, wy, ww, wh, "Calculator", 1);

    // Display
    gfill(wx+2, wy+2, ww-4, 1, ' ', A_EDIT);

    // Button layout (4x5 grid)
    static const char *btns[5][4] = {
        {"MC", "MR", "MS", "M+"},
        {"7",  "8",  "9",  "/"},
        {"4",  "5",  "6",  "*"},
        {"1",  "2",  "3",  "-"},
        {"0",  ".",  "=",  "+"},
    };
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 4; c++)
            gbtn(wx+2 + c*5, wy+4 + r*2, btns[r][c], 0);

    u32 acc = 0, operand = 0;
    char op = 0;
    int entering = 0;

    #define CALC_DISP() do { \
        gfill(wx+2, wy+2, ww-4, 1, ' ', A_EDIT); \
        u32 _v = entering ? operand : acc; \
        char _b[12]; int _i=0; \
        if(_v==0){_b[0]='0';_b[1]=0;} \
        else{while(_v>0&&_i<10){_b[_i++]='0'+(_v%10);_v/=10;}_b[_i]=0;my_reverse(_b,_i);} \
        int _l=gstrlen(_b); \
        gstr(wx+2+(ww-4-_l), wy+2, _b, A_EDIT); \
    } while(0)

    CALC_DISP();

    while (1) {
        char k = get_key();
        if (k == 27) break;
        if (k >= '0' && k <= '9') {
            if (!entering) { operand = 0; entering = 1; }
            operand = operand * 10 + (k - '0');
            CALC_DISP();
        } else if (k == '+' || k == '-' || k == '*' || k == '/') {
            if (entering) { acc = operand; entering = 0; }
            op = k;
        } else if (k == '\n' || k == '\r' || k == '=') {
            if (op && entering) {
                if      (op=='+') acc = acc + operand;
                else if (op=='-') acc = acc > operand ? acc - operand : 0;
                else if (op=='*') acc = acc * operand;
                else if (op=='/' && operand) acc = acc / operand;
                op = 0; entering = 0; CALC_DISP();
            }
        } else if (k == 'c' || k == 'C') {
            acc = 0; operand = 0; op = 0; entering = 0; CALC_DISP();
        }
    }
    #undef CALC_DISP
}

// ── 11d. System Info (enhanced) ───────────────────────────────────────────────
static void app_sysinfo(void) {
    int wx=4, wy=2, ww=72, wh=20;
    gwin(wx, wy, ww, wh, "System Information", 1);

    u8 la = GA(CY,  CLG);
    u8 va = GA(CK,  CLG);
    u8 ha = GA(CWH, CB);

    gstr(wx+2, wy+1, "[ CPU ]", ha);
    gstr(wx+3, wy+2,  "Architecture :", la); gstr(wx+20, wy+2,  "x86 (Intel 486, Real Mode 16-bit)", va);
    gstr(wx+3, wy+3,  "Mode         :", la); gstr(wx+20, wy+3,  "Real Mode (no protected mode)", va);
    gstr(wx+3, wy+4,  "FPU          :", la); gstr(wx+20, wy+4,  "Present (fsin/fcos/fsqrt used)", va);
    gstr(wx+3, wy+5,  "Timer        :", la); gstr(wx+20, wy+5,  "PIT 8253 @ 1000 Hz", va);
    gstr(wx+3, wy+6,  "Uptime       :", la);
    u32 us = time_ms / 1000;
    char ubuf[16]; int ui=0;
    if(us==0){ubuf[0]='0';ubuf[1]=0;}
    else{while(us>0&&ui<14){ubuf[ui++]='0'+(us%10);us/=10;}ubuf[ui]=0;my_reverse(ubuf,ui);}
    gstr(wx+20, wy+6, ubuf, va); gstr(wx+20+gstrlen(ubuf), wy+6, " seconds", va);

    gstr(wx+2, wy+8, "[ MEMORY ]", ha);
    gstr(wx+3, wy+9,  "Conventional :", la); gstr(wx+20, wy+9,  "640 KB (0x00000 - 0x9FFFF)", va);
    gstr(wx+3, wy+10, "Kernel base  :", la); gstr(wx+20, wy+10, "0x1000  (loaded by FAT12 bootloader)", va);
    gstr(wx+3, wy+11, "Stack        :", la); gstr(wx+20, wy+11, "0x7000 - 0x8000  (4 KB)", va);
    gstr(wx+3, wy+12, "FAT buffers  :", la); gstr(wx+20, wy+12, "0xD000  (sector buf + FAT table)", va);
    gstr(wx+3, wy+13, "Video RAM    :", la); gstr(wx+20, wy+13, "0xB8000 (VGA text 80x25)", va);
    gstr(wx+3, wy+14, "BIOS ROM     :", la); gstr(wx+20, wy+14, "0xE0000 - 0xFFFFF", va);

    gstr(wx+2, wy+15, "[ DISK ]", ha);
    gstr(wx+3, wy+16, "Type         :", la); gstr(wx+20, wy+16, "FAT12, 1.44 MB Floppy (A:)", va);
    gstr(wx+3, wy+17, "Geometry     :", la); gstr(wx+20, wy+17, "80 cyl x 2 heads x 18 sec/track", va);

    gstr(wx+3, wy+18, "RAM usage ~12%:", la);
    gprogbar(wx+18, wy+18, 40, 12);

    gbtn(wx+ww/2-4, wy+wh-2, "OK", 1);
    while (1) { char k = get_key(); if (k==27||k=='\n'||k=='\r') break; }
}

// ── 11e. About ────────────────────────────────────────────────────────────────
static void app_about(void) {
    int wx=18, wy=6, ww=44, wh=12;
    gdialog(wx, wy, ww, wh, " About Matthew OS ", A_TITLEBAR_ACT);

    gstrc(wx+1, wy+2, ww-2, "Matthew OS  v0.3", GA(CY, CLG));
    gstrc(wx+1, wy+3, ww-2, "x86 Real-Mode Operating System", GA(CK, CLG));
    gstrc(wx+1, wy+4, ww-2, "Copyright (C) 2026 Matthew Ahn", GA(CK, CLG));
    gstrc(wx+1, wy+5, ww-2, "Built with GCC -m16 + NASM", GA(CK, CLG));
    gstrc(wx+1, wy+6, ww-2, "No VESA. No libc. Pure real mode.", GA(CK, CLG));

    // Tiny logo
    gstr(wx+3, wy+8, "[ M A T T H E W   O S ]", GA(CB, CLG));

    gbtn(wx+ww/2-4, wy+wh-2, "OK", 1);
    while (1) { char k = get_key(); if (k==27||k=='\n'||k=='\r') break; }
}

// ── 11f. Allison easter egg ───────────────────────────────────────────────────
static void app_allison(void) {
    int wx=12, wy=2, ww=56, wh=20;
    gwin(wx, wy, ww, wh, " Allison ", 1);

    u8 ha = GA(CLR, CLG);
    static const char *heart[] = {
        "    ****       ****    ",
        "  ********   ********  ",
        " *********** *********** ",
        " ************************* ",
        " ************************* ",
        "  ***********************  ",
        "   *********************   ",
        "    *******************    ",
        "      ***************      ",
        "        ***********        ",
        "          *******          ",
        "            ***            ",
        "              *            ",
    };
    for (int i = 0; i < 13; i++)
        gstrc(wx+1, wy+2+i, ww-2, heart[i], ha);

    gstrc(wx+1, wy+16, ww-2, "A  L  L  I  S  O  N", GA(CK, CLG));
    gbtn(wx+ww/2-4, wy+wh-2, "OK", 1);
    while (1) { char k = get_key(); if (k==27||k=='\n'||k=='\r') break; }
}

// ── 11g. Snake game ───────────────────────────────────────────────────────────
#define SNAKE_BUF  0xE000
#define SNAKE_MAX  64
#define SB_W  36
#define SB_H  14

static void app_snake(void) {
    int wx=18, wy=3, ww=42, wh=19;
    gwin(wx, wy, ww, wh, "Snake", 1);

    u8 ba = GA(CK,  CLG);
    u8 sa = GA(CLG2,CLG);
    u8 hca= GA(CWH, CLG);
    u8 fa = GA(CLR, CLG);
    u8 wa = GA(CDG, CLG);

    int bx = wx+2, by = wy+2;

    // Walls
    for (int c = 0; c < SB_W; c++) {
        gp(bx+c, by-1,    CH_H, wa);
        gp(bx+c, by+SB_H, CH_H, wa);
    }
    for (int r = 0; r < SB_H; r++) {
        gp(bx-1,    by+r, CH_V, wa);
        gp(bx+SB_W, by+r, CH_V, wa);
    }
    gp(bx-1,    by-1,    CH_TL, wa); gp(bx+SB_W, by-1,    CH_TR, wa);
    gp(bx-1,    by+SB_H, CH_BL, wa); gp(bx+SB_W, by+SB_H, CH_BR, wa);

    gfill(wx+1, wy+wh-2, ww-2, 1, ' ', A_STATUSBAR);
    gstr(wx+2, wy+wh-2, "Arrows=move  ESC=quit  Score:0", A_STATUSBAR);

    typedef struct { u8 x, y; } Pt;
    Pt *snake = (Pt *)SNAKE_BUF;
    int slen = 3, dx = 1, dy = 0, score = 0;

    snake[0].x = SB_W/2+2; snake[0].y = SB_H/2;
    snake[1].x = SB_W/2+1; snake[1].y = SB_H/2;
    snake[2].x = SB_W/2;   snake[2].y = SB_H/2;

    u8 fx = (u8)((time_ms * 7 + 3) % SB_W);
    u8 fy = (u8)((time_ms * 13 + 5) % SB_H);

    for (int i = 0; i < slen; i++)
        gp(bx+snake[i].x, by+snake[i].y, CH_SOLID, i==0 ? hca : sa);
    gp(bx+fx, by+fy, CH_BULL, fa);

    u32 last_tick = time_ms;
    u32 speed = 150;

    while (1) {
        char k = get_key_nb();
        if (k == 27) break;
        if ((k == KEY_UP    || k == 'w' || k == 'W') && dy == 0) { dx= 0; dy=-1; }
        if ((k == KEY_DOWN  || k == 's' || k == 'S') && dy == 0) { dx= 0; dy= 1; }
        if ((k == KEY_LEFT  || k == 'a' || k == 'A') && dx == 0) { dx=-1; dy= 0; }
        if ((k == KEY_RIGHT || k == 'd' || k == 'D') && dx == 0) { dx= 1; dy= 0; }

        if (time_ms - last_tick < speed) continue;
        last_tick = time_ms;

        u8 nx = (u8)(snake[0].x + dx);
        u8 ny = (u8)(snake[0].y + dy);

        if (nx >= SB_W || ny >= SB_H) break;
        int hit = 0;
        for (int i = 0; i < slen-1; i++)
            if (snake[i].x == nx && snake[i].y == ny) { hit=1; break; }
        if (hit) break;

        gp(bx+snake[slen-1].x, by+snake[slen-1].y, ' ', ba);
        for (int i = slen-1; i > 0; i--) snake[i] = snake[i-1];
        snake[0].x = nx; snake[0].y = ny;

        if (nx == fx && ny == fy) {
            score++;
            if (slen < SNAKE_MAX) slen++;
            fx = (u8)((time_ms * 7 + score * 3) % SB_W);
            fy = (u8)((time_ms * 13 + score * 5) % SB_H);
            gp(bx+fx, by+fy, CH_BULL, fa);
            if (speed > 60) speed -= 5;
            gfill(wx+1, wy+wh-2, ww-2, 1, ' ', A_STATUSBAR);
            gstr(wx+2, wy+wh-2, "Arrows=move  ESC=quit  Score:", A_STATUSBAR);
            char sc_buf[8]; int si=0; int sv=score;
            if(sv==0){sc_buf[0]='0';sc_buf[1]=0;}
            else{while(sv>0&&si<6){sc_buf[si++]='0'+(sv%10);sv/=10;}sc_buf[si]=0;my_reverse(sc_buf,si);}
            gstr(wx+31, wy+wh-2, sc_buf, A_STATUSBAR);
        }
        gp(bx+snake[0].x, by+snake[0].y, CH_SOLID, hca);
        if (slen > 1) gp(bx+snake[1].x, by+snake[1].y, CH_SOLID, sa);
    }

    gstrc(wx+1, wy+wh/2, ww-2, "GAME OVER! Press any key...", GA(CLR, CLG));
    get_key();
}

// ── 11h. Hex Viewer ───────────────────────────────────────────────────────────
static void app_hexviewer(void) {
    int n = fat_list_dir();
    if (n <= 0) { return; }

    int wx=10, wy=4, ww=58, wh=15;
    gwin(wx, wy, ww, wh, "Hex Viewer - Select File", 1);
    gfill(wx+1, wy+1, ww-2, 1, ' ', GA(CK,CLG));
    gstr(wx+2, wy+1, "Select file (Enter=open, ESC=cancel)", GA(CK,CLG));

    int sel=0, scroll=0, vis=wh-4;
    #define HV_LIST() do { \
        gfill(wx+1, wy+2, ww-2, vis, ' ', A_LISTBOX); \
        for(int _i=0;_i<vis&&(scroll+_i)<n;_i++){ \
            u8 _a=(scroll+_i==sel)?A_LISTBOX_SEL:A_LISTBOX; \
            gfill(wx+1,wy+2+_i,ww-2,1,' ',_a); \
            gstr(wx+2,wy+2+_i,fat_entries[scroll+_i].name,_a); \
        } \
    } while(0)
    HV_LIST();

    char chosen[13]; chosen[0]=0;
    while(1) {
        char k=get_key();
        if(k==27) return;
        if((k==KEY_UP||k=='w'||k=='W')&&sel>0){sel--;if(sel<scroll)scroll=sel;HV_LIST();}
        if((k==KEY_DOWN||k=='s'||k=='S')&&sel<n-1){sel++;if(sel>=scroll+vis)scroll=sel-vis+1;HV_LIST();}
        if(k=='\n'||k=='\r'){
            if(fat_entries[sel].attr&FAT_ATTR_DIRECTORY) continue;
            for(int i=0;i<13;i++) chosen[i]=fat_entries[sel].name[i];
            break;
        }
    }
    #undef HV_LIST
    if(!chosen[0]) return;

    if(fat_load_fat()!=0) return;
    u8 name8[8], ext3[3];
    fat_parse_83(chosen, name8, ext3);
    u16 first_cluster=0; u32 file_size=0; int found=0;
    for(int s=0;s<FAT_ROOT_SECTORS&&!found;s++){
        if(fat_read_sector(FAT_ROOT_START+s,fat_sector_buf)!=0) return;
        FatDirEntry *entries=(FatDirEntry*)fat_sector_buf;
        for(int e=0;e<FAT_BYTES_PER_SECTOR/32&&!found;e++){
            FatDirEntry *de=&entries[e];
            if(de->name[0]==0) { s=FAT_ROOT_SECTORS; break; }
            if(de->name[0]==0xE5) continue;
            if(de->attr&(FAT_ATTR_DIRECTORY|FAT_ATTR_VOLUME)) continue;
            if(fat_strncmpi((char*)de->name,(char*)name8,8)==0&&
               fat_strncmpi((char*)de->ext,(char*)ext3,3)==0){
                first_cluster=de->first_cluster; file_size=de->size; found=1;
            }
        }
    }
    if(!found) return;

    int hx=1, hy=1, hw=78, hh=22;
    gwin(hx, hy, hw, hh, chosen, 1);
    int vis_rows = hh-4;
    u32 total_sectors = (file_size+FAT_BYTES_PER_SECTOR-1)/FAT_BYTES_PER_SECTOR;
    u32 sector_row = 0;
    u8 *hbuf = (u8*)0xE800;

    #define HV_LOAD(sec) do { \
        u16 _cl=first_cluster; u32 _si=(sec); \
        while(_si>=FAT_SECTORS_PER_CLUSTER&&_cl<0xFF8){_cl=fat_next_cluster(_cl);_si-=FAT_SECTORS_PER_CLUSTER;} \
        if(_cl>=2&&_cl<0xFF8) fat_read_sector(FAT_DATA_START+(_cl-2)*FAT_SECTORS_PER_CLUSTER+_si,hbuf); \
    } while(0)

    #define HV_DRAW() do { \
        gfill(hx+1,hy+1,hw-2,vis_rows,' ',A_WIN_BODY); \
        u32 _base=sector_row*FAT_BYTES_PER_SECTOR; \
        for(int _r=0;_r<vis_rows;_r++){ \
            u32 _off=_base+(u32)_r*16; if(_off>=file_size) break; \
            char _ob[7]; u32 _ov=_off; \
            for(int _k=5;_k>=0;_k--){_ob[_k]="0123456789ABCDEF"[_ov&0xF];_ov>>=4;} _ob[6]=0; \
            gstr(hx+2,hy+1+_r,_ob,GA(CY,CLG)); gp(hx+8,hy+1+_r,':',GA(CDG,CLG)); \
            for(int _b=0;_b<16;_b++){ \
                u32 _bi=_off+_b; \
                if(_bi>=file_size){gstr(hx+10+_b*3,hy+1+_r,"   ",A_WIN_BODY);continue;} \
                u8 _bv=hbuf[_bi%FAT_BYTES_PER_SECTOR]; \
                char _hx2[3]; _hx2[0]="0123456789ABCDEF"[_bv>>4]; \
                _hx2[1]="0123456789ABCDEF"[_bv&0xF]; _hx2[2]=0; \
                gstr(hx+10+_b*3,hy+1+_r,_hx2,A_WIN_BODY); \
            } \
            gp(hx+59,hy+1+_r,CH_V,GA(CDG,CLG)); \
            for(int _b=0;_b<16;_b++){ \
                u32 _bi=_off+_b; \
                if(_bi>=file_size){gp(hx+60+_b,hy+1+_r,' ',A_WIN_BODY);continue;} \
                u8 _bv=hbuf[_bi%FAT_BYTES_PER_SECTOR]; \
                gp(hx+60+_b,hy+1+_r,(_bv>=32&&_bv<127)?_bv:'.',GA(CLG2,CLG)); \
            } \
        } \
        gfill(hx+1,hy+hh-2,hw-2,1,' ',A_STATUSBAR); \
        gstr(hx+2,hy+hh-2,"Sec:",A_STATUSBAR); \
        char _ss[8]; int _si2=0; u32 _sv2=sector_row+1; \
        if(_sv2==0){_ss[0]='0';_ss[1]=0;} \
        else{while(_sv2>0&&_si2<6){_ss[_si2++]='0'+(_sv2%10);_sv2/=10;}_ss[_si2]=0;my_reverse(_ss,_si2);} \
        gstr(hx+7,hy+hh-2,_ss,A_STATUSBAR); \
        gstr(hx+10,hy+hh-2,"/",A_STATUSBAR); \
        _si2=0; _sv2=total_sectors; \
        if(_sv2==0){_ss[0]='0';_ss[1]=0;} \
        else{while(_sv2>0&&_si2<6){_ss[_si2++]='0'+(_sv2%10);_sv2/=10;}_ss[_si2]=0;my_reverse(_ss,_si2);} \
        gstr(hx+12,hy+hh-2,_ss,A_STATUSBAR); \
        gstr(hx+16,hy+hh-2,"  Up/Down=sector  ESC=close",A_STATUSBAR); \
    } while(0)

    HV_LOAD(0); HV_DRAW();
    while(1){
        char k=get_key();
        if(k==27) break;
        if((k==KEY_DOWN||k=='s'||k=='S')&&sector_row<total_sectors-1){sector_row++;HV_LOAD(sector_row);HV_DRAW();}
        if((k==KEY_UP||k=='w'||k=='W')&&sector_row>0){sector_row--;HV_LOAD(sector_row);HV_DRAW();}
    }
    #undef HV_LOAD
    #undef HV_DRAW
}

// ── 11i. ASCII / Text Viewer ──────────────────────────────────────────────────
#define TV_LINES 60
#define TV_COLS  74

static void app_ascviewer(void) {
    int n = fat_list_dir();
    if (n <= 0) return;

    int wx=10, wy=4, ww=58, wh=15;
    gwin(wx, wy, ww, wh, "Text Viewer - Select File", 1);
    gfill(wx+1, wy+1, ww-2, 1, ' ', GA(CK,CLG));
    gstr(wx+2, wy+1, "Select text file (Enter=open, ESC=cancel)", GA(CK,CLG));

    int sel=0, scroll=0, vis=wh-4;
    #define AV_LIST() do { \
        gfill(wx+1,wy+2,ww-2,vis,' ',A_LISTBOX); \
        for(int _i=0;_i<vis&&(scroll+_i)<n;_i++){ \
            u8 _a=(scroll+_i==sel)?A_LISTBOX_SEL:A_LISTBOX; \
            gfill(wx+1,wy+2+_i,ww-2,1,' ',_a); \
            gstr(wx+2,wy+2+_i,fat_entries[scroll+_i].name,_a); \
        } \
    } while(0)
    AV_LIST();

    char chosen[13]; chosen[0]=0;
    while(1){
        char k=get_key();
        if(k==27) return;
        if((k==KEY_UP||k=='w'||k=='W')&&sel>0){sel--;if(sel<scroll)scroll=sel;AV_LIST();}
        if((k==KEY_DOWN||k=='s'||k=='S')&&sel<n-1){sel++;if(sel>=scroll+vis)scroll=sel-vis+1;AV_LIST();}
        if(k=='\n'||k=='\r'){
            if(fat_entries[sel].attr&FAT_ATTR_DIRECTORY) continue;
            for(int i=0;i<13;i++) chosen[i]=fat_entries[sel].name[i];
            break;
        }
    }
    #undef AV_LIST
    if(!chosen[0]) return;

    // Text viewer window
    int tx=1, ty=1, tw=78, th=22;
    gwin(tx, ty, tw, th, chosen, 1);
    gfill(tx+1, ty+1, tw-2, th-3, ' ', A_EDIT);

    // Parse file into lines at fixed address
    char (*tv_buf)[TV_COLS] = (char (*)[TV_COLS])0xE800;
    int tv_nlines=0;
    for(int i=0;i<TV_LINES*TV_COLS;i++) ((char*)0xE800)[i]=0;

    if(fat_load_fat()!=0) return;
    u8 name8[8], ext3[3];
    fat_parse_83(chosen, name8, ext3);
    u16 first_cluster=0; u32 file_size=0; int found=0;
    for(int s=0;s<FAT_ROOT_SECTORS&&!found;s++){
        if(fat_read_sector(FAT_ROOT_START+s,fat_sector_buf)!=0) return;
        FatDirEntry *entries=(FatDirEntry*)fat_sector_buf;
        for(int e=0;e<FAT_BYTES_PER_SECTOR/32&&!found;e++){
            FatDirEntry *de=&entries[e];
            if(de->name[0]==0){s=FAT_ROOT_SECTORS;break;}
            if(de->name[0]==0xE5) continue;
            if(de->attr&(FAT_ATTR_DIRECTORY|FAT_ATTR_VOLUME)) continue;
            if(fat_strncmpi((char*)de->name,(char*)name8,8)==0&&
               fat_strncmpi((char*)de->ext,(char*)ext3,3)==0){
                first_cluster=de->first_cluster;file_size=de->size;found=1;
            }
        }
    }
    if(!found) return;

    int cur_col2=0;
    u32 bytes_left=file_size;
    u16 cluster=first_cluster;
    while(cluster>=2&&cluster<0xFF8&&bytes_left>0&&tv_nlines<TV_LINES){
        u16 lba=FAT_DATA_START+(cluster-2)*FAT_SECTORS_PER_CLUSTER;
        if(fat_read_sector(lba,fat_sector_buf)!=0) break;
        u32 to_read=bytes_left<FAT_BYTES_PER_SECTOR?bytes_left:FAT_BYTES_PER_SECTOR;
        for(u32 i=0;i<to_read&&tv_nlines<TV_LINES;i++){
            u8 c=fat_sector_buf[i];
            if(c=='\r') continue;
            if(c=='\n'){tv_nlines++;cur_col2=0;continue;}
            if(c>=32&&c<127&&cur_col2<TV_COLS-1){
                tv_buf[tv_nlines][cur_col2++]=c;
                tv_buf[tv_nlines][cur_col2]=0;
            }
        }
        bytes_left-=to_read;
        cluster=fat_next_cluster(cluster);
    }
    if(cur_col2>0) tv_nlines++;

    int vis_rows=th-4, tv_scroll=0;

    #define TV_DRAW() do { \
        gfill(tx+1,ty+1,tw-2,vis_rows,' ',A_EDIT); \
        for(int _r=0;_r<vis_rows&&(tv_scroll+_r)<tv_nlines;_r++) \
            gstr(tx+1,ty+1+_r,tv_buf[tv_scroll+_r],A_EDIT); \
        gscrollbar_v(tx+tw-2,ty+1,vis_rows+2,tv_scroll,tv_nlines>0?tv_nlines-1:0); \
        gfill(tx+1,ty+th-2,tw-2,1,' ',A_STATUSBAR); \
        gstr(tx+2,ty+th-2,"Up/Down/PgUp/PgDn=scroll  ESC=close",A_STATUSBAR); \
    } while(0)

    TV_DRAW();
    while(1){
        char k=get_key();
        if(k==27) break;
        if((k==KEY_UP||k=='w'||k=='W')&&tv_scroll>0){tv_scroll--;TV_DRAW();}
        if((k==KEY_DOWN||k=='s'||k=='S')&&tv_scroll<tv_nlines-vis_rows){tv_scroll++;TV_DRAW();}
        if(k==KEY_PGUP||k=='q'||k=='Q'){tv_scroll-=vis_rows;if(tv_scroll<0)tv_scroll=0;TV_DRAW();}
        if(k==KEY_PGDN||k=='e'||k=='E'){tv_scroll+=vis_rows;if(tv_scroll>tv_nlines-vis_rows)tv_scroll=tv_nlines-vis_rows;if(tv_scroll<0)tv_scroll=0;TV_DRAW();}
    }
    #undef TV_DRAW
}
#undef TV_LINES
#undef TV_COLS
// ── 11j. Memory Viewer ───────────────────────────────────────────────────────
static void app_memviewer(void) {
    int wx=1, wy=1, ww=78, wh=22;
    gwin(wx, wy, ww, wh, "Memory Viewer", 1);

    u8 ea = GA(CK, CWH);
    static const char *regions[] = {
        "0x00000 IVT",
        "0x00400 BDA",
        "0x00500 Free",
        "0x02000 Kernel",
        "0x06000 History",
        "0x0C000 Stack",
        "0x0D000 FAT buf",
        "0xB8000 Video",
        "0xC0000 VGA ROM",
        "0xE0000 BIOS",
    };
    static const u32 region_addrs[] = {
        0x00000, 0x00400, 0x00500, 0x02000,
        0x06000, 0x0C000, 0x0D000, 0xB8000,
        0xC0000, 0xE0000,
    };

    u32 view_addr = 0x00000;
    int vis_rows = wh - 5;
    static const char hexc[] = "0123456789ABCDEF";
    char addr_input[6];
    int addr_len = 0;

    #define MV_DRAW() do { \
        gfill(wx+1, wy+2, 14, vis_rows, ' ', GA(CDG, CLG)); \
        for (int _r = 0; _r < 10 && _r < vis_rows; _r++) \
            gstr(wx+1, wy+2+_r, regions[_r], GA(CY, CLG)); \
        for (int _r = 0; _r < vis_rows; _r++) { \
            u32 _a = view_addr + (u32)_r * 8; \
            char _ab[7]; u32 _av = _a; \
            for (int _k = 4; _k >= 0; _k--) { _ab[_k] = hexc[_av & 0xF]; _av >>= 4; } \
            _ab[5] = ':'; _ab[6] = 0; \
            gstr(wx+16, wy+2+_r, _ab, GA(CY, CLG)); \
            for (int _b = 0; _b < 8; _b++) { \
                u32 _ba = _a + _b; \
                u8 _bv = (_ba < 0x100000) ? *(u8*)_ba : 0; \
                char _hx[3]; _hx[0] = hexc[_bv>>4]; _hx[1] = hexc[_bv&0xF]; _hx[2] = 0; \
                u8 _at = (_ba >= 0xB8000 && _ba < 0xB9000) ? GA(CLR,CLG) : \
                         (_ba < 0x500) ? GA(CY,CLG) : GA(CK,CLG); \
                gstr(wx+23+_b*3, wy+2+_r, _hx, _at); \
            } \
            gp(wx+48, wy+2+_r, '|', GA(CDG,CLG)); \
            for (int _b = 0; _b < 8; _b++) { \
                u32 _ba = _a + _b; \
                u8 _bv = (_ba < 0x100000) ? *(u8*)_ba : 0; \
                gp(wx+49+_b, wy+2+_r, (_bv>=32&&_bv<127)?_bv:'.', GA(CLG2,CLG)); \
            } \
        } \
        gfill(wx+17, wy+1, 8, 1, ' ', ea); \
        char _ca[6]; u32 _cv = view_addr; \
        for (int _k = 4; _k >= 0; _k--) { _ca[_k] = hexc[_cv & 0xF]; _cv >>= 4; } \
        _ca[5] = 0; \
        gstr(wx+17, wy+1, _ca, ea); \
        gfill(wx+1, wy+wh-2, ww-2, 1, ' ', A_STATUSBAR); \
        gstr(wx+2, wy+wh-2, "W/S=scroll  A/D=region jump  Enter=goto addr  ESC=close", A_STATUSBAR); \
    } while(0)

    gfill(wx+1, wy+1, ww-2, 1, ' ', A_MENUBAR);
    gstr(wx+2, wy+1, "Address: ", A_MENUBAR);
    MV_DRAW();

    while (1) {
        char k = get_key();
        if (k == 27 || k == 'q' || k == 'Q') break;
        if (k == 'w' || k == 'W') {
            if (view_addr >= 8) view_addr -= 8; else view_addr = 0;
            MV_DRAW();
        } else if (k == 's' || k == 'S') {
            view_addr += 8;
            if (view_addr > 0xFFF00) view_addr = 0xFFF00;
            MV_DRAW();
        } else if (k == 'a' || k == 'A') {
            for (int i = 9; i >= 0; i--)
                if (region_addrs[i] < view_addr) { view_addr = region_addrs[i]; break; }
            MV_DRAW();
        } else if (k == 'd' || k == 'D') {
            for (int i = 0; i < 10; i++)
                if (region_addrs[i] > view_addr) { view_addr = region_addrs[i]; break; }
            MV_DRAW();
        } else if (k == '\n' || k == '\r') {
            gfill(wx+17, wy+1, 8, 1, ' ', GA(CWH, CK));
            addr_len = 0;
            while (1) {
                char c = get_key();
                if (c == '\n' || c == '\r') break;
                if (c == 27) { addr_len = 0; break; }
                if (c == '\b' && addr_len > 0) {
                    addr_len--;
                    gp(wx+17+addr_len, wy+1, ' ', GA(CWH,CK));
                    continue;
                }
                if (addr_len < 5) {
                    char uc = c;
                    if (uc >= 'a' && uc <= 'f') uc -= 32;
                    if ((uc >= '0' && uc <= '9') || (uc >= 'A' && uc <= 'F')) {
                        addr_input[addr_len++] = uc;
                        gp(wx+17+addr_len-1, wy+1, uc, GA(CWH,CK));
                    }
                }
            }
            if (addr_len > 0) {
                u32 na = 0;
                for (int i = 0; i < addr_len; i++) {
                    char c = addr_input[i];
                    na = na * 16 + (c >= 'A' ? c - 'A' + 10 : c - '0');
                }
                view_addr = na & ~7u;
            }
            MV_DRAW();
        }
    }
    #undef MV_DRAW
}

static void app_reboot(void) {
    int wx=24, wy=9, ww=32, wh=7;
    gdialog(wx, wy, ww, wh, " Confirm Reboot ", A_ERR_TITLE);
    gstrc(wx+1, wy+2, ww-2, "Reboot the system?", GA(CK, CLG));
    gbtn(wx+4,      wy+4, "Yes", 1);
    gbtn(wx+ww-12,  wy+4, "No",  0);
    while (1) {
        char k = get_key();
        if (k=='y'||k=='Y'||k=='\n'||k=='\r') { outb(0x64,0xFE); while(1){} }
        if (k=='n'||k=='N'||k==27) break;
    }
}

// ── 11h. Shell terminal window ────────────────────────────────────────────────
// Launches a mini shell inside a window (limited, for show)
static void app_shell_window(void) {
    int wx=2, wy=4, ww=76, wh=18;
    gwin(wx, wy, ww, wh, "Command Prompt", 1);
    gfill(wx+1, wy+1, ww-2, wh-2, ' ', GA(CLG2, CK));

    // Redirect shell output into this window
    int saved_x = cur_x, saved_y = cur_y;
    u8 saved_attr = ATTR;
    cur_x = wx+1; cur_y = wy+1;

    gstr(wx+1, wy+1, "Matthew OS Command Prompt", GA(CLG2, CK));
    gstr(wx+1, wy+2, "Type EXIT to return to desktop.", GA(CLG2, CK));
    cur_y = wy+3;

    char line[80];
    while (1) {
        // prompt
        gstr(cur_x, cur_y, "A:\\> ", GA(CLG2, CK));
        cur_x += 5;
        // read line (reuse existing read_line but with green-on-black attr)
        int len = 0;
        while (1) {
            char c = get_key();
            if (c == '\n' || c == '\r') { line[len] = 0;
                cur_x = wx+1; cur_y++;
                if (cur_y >= wy+wh-1) { cur_y = wy+3;
                    gfill(wx+1, wy+3, ww-2, wh-4, ' ', GA(CLG2, CK)); }
                break; }
            if (c == '\b' && len > 0) { len--;
                cur_x--; gp(cur_x, cur_y, ' ', GA(CLG2, CK)); continue; }
            if (c >= 32 && c <= 126 && len < 78 && cur_x < wx+ww-2) {
                line[len++] = c;
                gp(cur_x++, cur_y, c, GA(CLG2, CK)); }
        }
        if (streq(line, "EXIT") || streq(line, "exit")) break;
        if (streq(line, "CLS") || streq(line, "cls")) {
            gfill(wx+1, wy+3, ww-2, wh-4, ' ', GA(CLG2, CK));
            cur_y = wy+3; continue; }
        // Execute and capture output into window
        execute(line);
        if (cur_y >= wy+wh-1) { cur_y = wy+3;
            gfill(wx+1, wy+3, ww-2, wh-4, ' ', GA(CLG2, CK)); }
    }
    cur_x = saved_x; cur_y = saved_y;
}

// ══════════════════════════════════════════════════════════════════════════════
// SECTION 12 – Program Manager (main window, Win3.1 style)
// ══════════════════════════════════════════════════════════════════════════════
// The Program Manager is a window that contains the icon grid.
// It has its own menu bar.

static void gui_draw_progman(int icon_sel, int menu_sel) {
    int wx=0, wy=0, ww=COLS, wh=ROWS-1;
    // Draw the Program Manager window (full screen minus taskbar)
    gwin(wx, wy, ww, wh, "Program Manager", 1);
    // Menu bar inside window (row wy+1)
    gdraw_menubar(wy+1, menu_sel);
    // Separator
    for (int c = wx+1; c < wx+ww-1; c++) gp(c, wy+2, CH_H, A_WIN_BORDER);
    // Icon area background
    gfill(wx+1, wy+3, ww-2, wh-4, CH_FILL, A_DESKTOP);
    // Icons
    gdraw_all_icons(icon_sel);
    // Taskbar
    gui_draw_taskbar();
}

// ══════════════════════════════════════════════════════════════════════════════
// SECTION 13 – Main GUI entry point
// ══════════════════════════════════════════════════════════════════════════════
void gui_main(void) {
    int icon_sel  = 0;
    int menu_sel  = -1;   // -1 = no menu open
    int running   = 1;

    // Initial draw
    gui_draw_progman(icon_sel, menu_sel);

    // Welcome splash
    {
        int sx=20, sy=8, sw=40, sh=7;
        gdialog(sx, sy, sw, sh, " Welcome ", A_TITLEBAR_ACT);
        gstrc(sx+1, sy+2, sw-2, "Welcome to Matthew OS!", GA(CK, CLG));
        gstrc(sx+1, sy+3, sw-2, "Use Left/Right arrows + Enter.", GA(CK, CLG));
        gstrc(sx+1, sy+4, sw-2, "Press ESC to return to shell.", GA(CK, CLG));
        gbtn(sx+sw/2-4, sy+sh-2, "OK", 1);
        while (1) { char k = get_key(); if (k==27||k=='\n'||k=='\r') break; }
        gui_draw_progman(icon_sel, menu_sel);
    }

    while (running) {
        char k = get_key();

        if (k == 27) { running = 0; break; }

        // Left/Right arrow navigation — supports arrow keys, h/l, and A/D
        if (k == KEY_LEFT  || k == 'h' || k == 'H' || k == 'a' || k == 'A') {
            if (icon_sel > 0) { icon_sel--; gui_draw_progman(icon_sel, menu_sel); }
        } else if (k == KEY_RIGHT || k == 'l' || k == 'L' || k == 'd' || k == 'D') {
            if (icon_sel < MAX_ICONS-1) { icon_sel++; gui_draw_progman(icon_sel, menu_sel); }
        } else if (k == KEY_UP || k == 'w' || k == 'W') {
            if (icon_sel >= 8) { icon_sel -= 8; gui_draw_progman(icon_sel, menu_sel); }
        } else if (k == KEY_DOWN || k == 's' || k == 'S') {
            if (icon_sel < 8 && icon_sel + 8 < MAX_ICONS) { icon_sel += 8; gui_draw_progman(icon_sel, menu_sel); }
        } else if (k == '\n' || k == '\r') {
            switch (icon_sel) {
                case 0: app_filemanager(); break;
                case 1: app_notepad();     break;
                case 2: app_calc();        break;
                case 3: app_sysinfo();     break;
                case 4: app_about();       break;
                case 5: app_snake();       break;
                case 6: app_hexviewer();   break;
                case 7: app_ascviewer();   break;
                case 8: app_memviewer();   break;
                case 9: app_reboot();      break;
            }
            gui_draw_progman(icon_sel, menu_sel);
        }

        // Alt-F4 style: 'q' quits to shell
        if (k == 'q' || k == 'Q') { running = 0; break; }

        // Number shortcuts 1-9, 0
        if (k >= '1' && k <= '9') {
            int idx = k - '1';
            if (idx < MAX_ICONS) {
                icon_sel = idx;
                gui_draw_progman(icon_sel, menu_sel);
                switch (idx) {
                    case 0: app_filemanager(); break;
                    case 1: app_notepad();     break;
                    case 2: app_calc();        break;
                    case 3: app_sysinfo();     break;
                    case 4: app_about();       break;
                    case 5: app_snake();       break;
                    case 6: app_hexviewer();   break;
                    case 7: app_ascviewer();   break;
                    case 8: app_memviewer();   break;
                }
                gui_draw_progman(icon_sel, menu_sel);
            }
        }
        if (k == '0') { icon_sel=9; app_reboot(); gui_draw_progman(icon_sel, menu_sel); }

        // 'S' opens shell window
        if (k == 's' || k == 'S') {
            app_shell_window();
            gui_draw_progman(icon_sel, menu_sel);
        }
    }

    // Return to shell
    clear_screen();
}
