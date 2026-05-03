#!/usr/bin/env python3
# Run from the project directory: python3 fix_keyboard.py
import os

os.chdir('/mnt/c/Users/withd/Desktop/M8SBC-486-main/새 폴더')

with open('main.c', 'r', encoding='utf-8', errors='replace') as f:
    src = f.read()

# ── 1. Replace keyboard section ──────────────────────────────────────────────
KB_START = '// ---------------- keyboard polling ----------------'
KB_END   = '// ---------------- Allison heart easter egg ----------------'

si = src.find(KB_START)
ei = src.find(KB_END)
assert si != -1 and ei != -1, f"markers not found {si} {ei}"

NEW_KB = r"""// ---------------- keyboard polling ----------------
// Special key codes returned by get_key() (non-printable range)
#define KEY_UP    '\x01'
#define KEY_DOWN  '\x02'
#define KEY_LEFT  '\x03'
#define KEY_RIGHT '\x04'
#define KEY_HOME  '\x05'
#define KEY_END   '\x06'
#define KEY_PGUP  '\x07'
#define KEY_PGDN  '\x08'
#define KEY_DEL   '\x7F'
#define KEY_F1    '\x10'
#define KEY_F2    '\x11'
#define KEY_F3    '\x12'
#define KEY_F5    '\x14'
#define KEY_F10   '\x19'

static const char scancode_normal[128] = {
    0,   27,  '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0,  '*', 0,  ' ',
};

static const char scancode_shift[128] = {
    0,   27,  '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0,  '|',
    'Z','X','C','V','B','N','M','<','>','?', 0,  '*', 0,  ' ',
};

static int shift_down = 0;
static int caps_on    = 0;

static char get_key(void) {
    while (1) {
        if ((inb(0x64) & 1) == 0) continue;
        u8 sc = inb(0x60);

        // Key release events
        if (sc == 0xAA || sc == 0xB6) { shift_down = 0; continue; }
        if (sc & 0x80) continue;

        // Shift / CapsLock
        if (sc == 0x2A || sc == 0x36) { shift_down = 1; continue; }
        if (sc == 0x3A) { caps_on = !caps_on; continue; }

        // Extended key prefix 0xE0 -> arrow keys, Home, End, PgUp, PgDn, Del
        if (sc == 0xE0) {
            while ((inb(0x64) & 1) == 0) {}
            u8 sc2 = inb(0x60);
            if (sc2 & 0x80) continue;   // release of extended key
            switch (sc2) {
                case 0x48: return KEY_UP;
                case 0x50: return KEY_DOWN;
                case 0x4B: return KEY_LEFT;
                case 0x4D: return KEY_RIGHT;
                case 0x47: return KEY_HOME;
                case 0x4F: return KEY_END;
                case 0x49: return KEY_PGUP;
                case 0x51: return KEY_PGDN;
                case 0x53: return KEY_DEL;
            }
            continue;
        }

        // F-keys
        if (sc == 0x3B) return KEY_F1;
        if (sc == 0x3C) return KEY_F2;
        if (sc == 0x3D) return KEY_F3;
        if (sc == 0x3F) return KEY_F5;
        if (sc == 0x44) return KEY_F10;

        char c = shift_down ? scancode_shift[sc] : scancode_normal[sc];
        if (c >= 'a' && c <= 'z' && caps_on) c -= 32;
        if (c >= 'A' && c <= 'Z' && caps_on && shift_down) c += 32;
        if (c) return c;
    }
}

// Non-blocking poll: returns 0 if no key is waiting
static char get_key_nb(void) {
    if ((inb(0x64) & 1) == 0) return 0;
    return get_key();
}

"""

src = src[:si] + NEW_KB + src[ei:]
print("keyboard section replaced")

# ── 2. Replace read_line with history-aware version ──────────────────────────
RL_START = 'static void read_line(char *buf, int max) {'
RL_END   = '\n}\n\n\n// ---------------- Allison'

rsi = src.find(RL_START)
rei = src.find('\n}\n', rsi) + 3   # end of function
assert rsi != -1, "read_line not found"

NEW_RL = r"""// Shell command history (circular buffer)
#define HIST_MAX 10
#define HIST_LEN 96
static char hist_buf[HIST_MAX][HIST_LEN];
static int  hist_count = 0;
static int  hist_head  = 0;

static void hist_push(const char *cmd) {
    if (!cmd[0]) return;
    int last = (hist_head - 1 + HIST_MAX) % HIST_MAX;
    if (hist_count > 0) {
        int eq = 1;
        for (int i = 0; hist_buf[last][i] || cmd[i]; i++)
            if (hist_buf[last][i] != cmd[i]) { eq = 0; break; }
        if (eq) return;
    }
    for (int i = 0; i < HIST_LEN-1 && cmd[i]; i++)
        hist_buf[hist_head][i] = cmd[i];
    hist_buf[hist_head][HIST_LEN-1] = 0;
    hist_head = (hist_head + 1) % HIST_MAX;
    if (hist_count < HIST_MAX) hist_count++;
}

static void read_line(char *buf, int max) {
    int len = 0;
    int hist_pos = -1;
    int sx = cur_x, sy = cur_y;

    while (1) {
        char c = get_key();

        if (c == '\n' || c == '\r') {
            buf[len] = 0;
            newline();
            hist_push(buf);
            return;
        }
        if (c == '\b') {
            if (len > 0) { len--; putc_attr('\b', ATTR); }
            continue;
        }
        // Up arrow: older history
        if (c == KEY_UP) {
            if (hist_count == 0) continue;
            if (hist_pos < hist_count - 1) hist_pos++;
            int idx = (hist_head - 1 - hist_pos + HIST_MAX * 2) % HIST_MAX;
            // clear current input on screen
            cur_x = sx; cur_y = sy;
            for (int i = 0; i < len; i++) put_cell(sx+i, sy, ' ', ATTR);
            len = 0;
            while (hist_buf[idx][len] && len < max-1) {
                buf[len] = hist_buf[idx][len];
                put_cell(sx+len, sy, buf[len], ATTR);
                len++;
            }
            buf[len] = 0;
            cur_x = sx + len;
            continue;
        }
        // Down arrow: newer history
        if (c == KEY_DOWN) {
            cur_x = sx; cur_y = sy;
            for (int i = 0; i < len; i++) put_cell(sx+i, sy, ' ', ATTR);
            len = 0;
            if (hist_pos > 0) {
                hist_pos--;
                int idx = (hist_head - 1 - hist_pos + HIST_MAX * 2) % HIST_MAX;
                while (hist_buf[idx][len] && len < max-1) {
                    buf[len] = hist_buf[idx][len];
                    put_cell(sx+len, sy, buf[len], ATTR);
                    len++;
                }
            } else {
                hist_pos = -1;
            }
            buf[len] = 0;
            cur_x = sx + len;
            continue;
        }
        // Ignore other special keys in shell input
        if ((unsigned char)c < 32) continue;
        if (c >= 32 && c <= 126 && len < max - 1) {
            buf[len++] = c;
            if (echo_on) putc_attr(c, ATTR);
        }
    }
}

"""

src = src[:rsi] + NEW_RL + src[rei:]
print("read_line replaced")

with open('main.c', 'w', encoding='utf-8') as f:
    f.write(src)
print("main.c written OK")
