// main.c
// Matthew OS DOS-like shell build
// Original skeleton by maniek86, shell customization by Matthew Ahn

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

u32 time_ms;

#include "memaccess.c"
#include "misc.c"
#include "ivt.c"
#include "math.c"
#include "disk.c"

#define COLS 80
#define ROWS 25
#define ATTR   0x0F
#define ATTR_TITLE 0x1F
#define ATTR_OK    0x0A
#define ATTR_ERR   0x0C
#define ATTR_DIM   0x08

static int cur_x = 0;
static int cur_y = 0;
static char current_drive = 'A';  // real floppy is A:
static int echo_on = 1;
static u32 boot_ticks = 0;

// forward declarations
void gui_main(void);
static char get_key_nb(void);  // non-blocking key poll (defined after get_key)
// fat.c forward declarations (included later)
void fat_cmd_dir(void);
void fat_cmd_chkdsk(void);
void fat_cmd_vol(void);
void fat_cmd_tree(void);
int  fat_cmd_type(const char *filename);
int  fat_list_dir(void);

// ---------------- low level screen ----------------
static void put_cell(int x, int y, char c, u8 attr) {
    if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return;
    dispchar(chr((u8)c, attr), (u16)((y * COLS + x) * 2));
}

static void clear_screen(void) {
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) put_cell(x, y, ' ', ATTR);
    }
    cur_x = 0;
    cur_y = 0;
}

static void scroll(void) {
    u32 oldfs = set_videomode_fs();
    for (int y = 0; y < ROWS - 1; y++) {
        for (int x = 0; x < COLS; x++) {
            u16 src = (u16)(((y+1)*COLS + x)*2);
            u16 dst = (u16)((y*COLS + x)*2);
            u16 cell;
            __asm__ volatile("movw %%fs:%[s],%[d]" : [d]"=r"(cell) : [s]"m"(*(u16*)(u32)src));
            __asm__ volatile("movw %[d],%%fs:%[s]" : : [d]"r"(cell), [s]"m"(*(u16*)(u32)dst) : "memory");
        }
    }
    set_fs(oldfs);
    for (int x = 0; x < COLS; x++) put_cell(x, ROWS-1, ' ', ATTR);
    cur_y = ROWS - 1;
}

static void newline(void) {
    cur_x = 0;
    cur_y++;
    if (cur_y >= ROWS) scroll();
}

static void putc_attr(char c, u8 attr) {
    if (c == '\n') { newline(); return; }
    if (c == '\r') { cur_x = 0; return; }
    if (c == '\b') {
        if (cur_x > 0) {
            cur_x--;
            put_cell(cur_x, cur_y, ' ', attr);
        }
        return;
    }
    put_cell(cur_x, cur_y, c, attr);
    cur_x++;
    if (cur_x >= COLS) newline();
}

static void puts_attr(const char *s, u8 attr) {
    while (*s) putc_attr(*s++, attr);
}

static void puts(const char *s) { puts_attr(s, ATTR); }

static void draw_text_at(int x, int y, const char *s, u8 attr) {
    int i = 0;
    while (s[i]) {
        put_cell(x + i, y, s[i], attr);
        i++;
    }
}

static int strlen_s(const char *s) { int n = 0; while (s[n]) n++; return n; }

static int streq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static int starts_with(const char *s, const char *prefix) {
    int i = 0;
    while (prefix[i]) {
        char ca = s[i];
        char cb = prefix[i];
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return 0;
        i++;
    }
    return 1;
}

static void trim_left(char **p) {
    while (**p == ' ') (*p)++;
}

static void print_dec(u32 v) {
    char buf[16];
    int i = 0;
    if (v == 0) { putc_attr('0', ATTR); return; }
    while (v > 0 && i < 15) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    while (i--) putc_attr(buf[i], ATTR);
}

static void prompt(void) {
    putc_attr('\n', ATTR);
    putc_attr(current_drive, ATTR);
    puts(":\\>");
}

// ---------------- keyboard polling ----------------
static const char scancode_normal[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'', '`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
};

static const char scancode_shift[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',
};

static int shift_down = 0;
static int caps_on = 0;

// Special key return codes (non-printable, safe range)
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

static char get_key(void) {
    while (1) {
        // Wait for key
        while ((inb(0x64) & 1) == 0) {}
        u8 sc = inb(0x60);

        // Key release
        if (sc == 0xAA || sc == 0xB6) { shift_down = 0; continue; }
        if (sc & 0x80) continue;

        // Modifiers
        if (sc == 0x2A || sc == 0x36) { shift_down = 1; continue; }
        if (sc == 0x3A) { caps_on = !caps_on; continue; }

        // Extended key prefix 0xE0
        if (sc == 0xE0) {
            // Drain any pending data first, then wait for the actual scancode
            u8 sc2 = 0;
            // Wait up to ~100ms for the second byte
            for (volatile u32 t = 0; t < 500000; t++) {
                if (inb(0x64) & 1) { sc2 = inb(0x60); break; }
            }
            if (sc2 & 0x80) continue;  // release
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

// Non-blocking: returns 0 if no key is waiting
static char get_key_nb(void) {
    if ((inb(0x64) & 1) == 0) return 0;
    u8 sc = inb(0x60);

    if (sc == 0xAA || sc == 0xB6) { shift_down = 0; return 0; }
    if (sc & 0x80) return 0;
    if (sc == 0x2A || sc == 0x36) { shift_down = 1; return 0; }
    if (sc == 0x3A) { caps_on = !caps_on; return 0; }

    if (sc == 0xE0) {
        u8 sc2 = 0;
        for (volatile u32 t = 0; t < 500000; t++) {
            if (inb(0x64) & 1) { sc2 = inb(0x60); break; }
        }
        if (sc2 & 0x80) return 0;
        switch (sc2) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
        }
        return 0;
    }

    if (sc == 0x3B) return KEY_F1;
    if (sc == 0x3C) return KEY_F2;
    if (sc == 0x3D) return KEY_F3;
    if (sc == 0x3F) return KEY_F5;
    if (sc == 0x44) return KEY_F10;

    char c = shift_down ? scancode_shift[sc] : scancode_normal[sc];
    if (c >= 'a' && c <= 'z' && caps_on) c -= 32;
    if (c >= 'A' && c <= 'Z' && caps_on && shift_down) c += 32;
    return c;
}

// Shell command history — stored at fixed address to keep BSS small
// 0x7800: between stack bottom (0x7000) and stack top (0x8000)? No.
// Use 0x6000: safe conventional RAM, below stack at 0x7000.
#define HIST_BUF_BASE 0x6000
#define HIST_MAX 10
#define HIST_LEN 96

static char (*hist_buf)[HIST_LEN] = (char (*)[HIST_LEN])HIST_BUF_BASE;
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
        // Up arrow: older history entry
        if (c == KEY_UP) {
            if (hist_count == 0) continue;
            if (hist_pos < hist_count - 1) hist_pos++;
            int idx = (hist_head - 1 - hist_pos + HIST_MAX * 2) % HIST_MAX;
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
        // Down arrow: newer history entry
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


// ---------------- Allison heart easter egg ----------------
// VGA text attribute: 0x0C = bright red foreground on black background.
// This keeps the old shell behavior intact and only changes the ALLISON output.
#define ATTR_HEART 0x0C

static void delay_line(void) {
    for (volatile u32 i = 0; i < 260000UL; i++) {
        // busy-wait delay for tiny real-mode demo; no libc/timer dependency
    }
}

static void puts_line_slow(const char *s, u8 attr) {
    puts_attr(s, attr);
    putc_attr('\n', attr);
    delay_line();
}

static void print_allison_heart(void) {
    puts_line_slow("       *****       *****       ", ATTR_HEART);
    puts_line_slow("     *********   *********     ", ATTR_HEART);
    puts_line_slow("   ************* *************   ", ATTR_HEART);
    puts_line_slow("  *****************************  ", ATTR_HEART);
    puts_line_slow(" ******************************* ", ATTR_HEART);
    puts_line_slow(" ******************************* ", ATTR_HEART);
    puts_line_slow("  *****************************  ", ATTR_HEART);
    puts_line_slow("   ***************************   ", ATTR_HEART);
    puts_line_slow("    *************************    ", ATTR_HEART);
    puts_line_slow("      *********************      ", ATTR_HEART);
    puts_line_slow("        *****************        ", ATTR_HEART);
    puts_line_slow("          *************          ", ATTR_HEART);
    puts_line_slow("            *********            ", ATTR_HEART);
    puts_line_slow("              *****              ", ATTR_HEART);
    puts_line_slow("                *                ", ATTR_HEART);
    puts_line_slow("", ATTR);
    puts_line_slow("        A   L      L      I   SSSSS  OOOOO  N   N", ATTR);
    puts_line_slow("       A A  L      L      I   S      O   O  NN  N", ATTR);
    puts_line_slow("      A   A L      L      I   SSSSS  O   O  N N N", ATTR);
    puts_line_slow("      AAAAA L      L      I       S  O   O  N  NN", ATTR);
    puts_line_slow("      A   A LLLLL  LLLLL  I   SSSSS  OOOOO  N   N", ATTR);
}

// ---------------- fake DOS commands ----------------
static void print_banner(void) {
    clear_screen();
    draw_text_at(40, 22, "4C 6F 76 65 20 79 6F 75 2C 20", ATTR_DIM);
    draw_text_at(50, 23, "41 6C 6C 69 73 6F 6E 2E", ATTR_DIM);
    puts_attr("============================= MATTHEW OS =============================", ATTR_TITLE);
    puts("\n");
    puts_attr("      ##     ##    ###    ######## ######## ##     ## ######## ##      ##\n", ATTR_OK);
    puts_attr("      ###   ###   ## ##      ##       ##    ##     ## ##       ##  ##  ##\n", ATTR_OK);
    puts_attr("      #### ####  ##   ##     ##       ##    ##     ## ##       ##  ##  ##\n", ATTR_OK);
    puts_attr("      ## ### ## #########    ##       ##    ######### ######   ##  ##  ##\n", ATTR_OK);
    puts_attr("      ##     ## ##     ##    ##       ##    ##     ## ##       ##  ##  ##\n", ATTR_OK);
    puts_attr("      ##     ## ##     ##    ##       ##    ##     ## ########  ###  ### \n", ATTR_OK);
    puts("\n");
    puts_attr("                  Matthew OS v0.3\n", ATTR_TITLE);
    puts("                  Copyright (C) 2026 Matthew Doyoon Ahn\n");
    puts("                  Type HELP for command list.\n");
    puts("                  Type GUI  to launch the graphical desktop.\n");
}

static void cmd_help(void) {
    puts("Supported commands:\n");
    puts("  HELP        CLS         VER         DATE        TIME\n");
    puts("  DIR         TYPE        MEM         ECHO        ECHO ON/OFF\n");
    puts("  CD          VOL         CHKDSK      TREE        SYSINFO\n");
    puts("  MEMMAP      UPTIME      BEEP [hz]   HEXDUMP     DISKINFO\n");
    puts("  REBOOT      ABOUT       GUI         EXIT        PAUSE\n");
    puts("\n");
    puts("GUI      - Graphical desktop\n");
    puts("MEMMAP   - Physical memory map\n");
    puts("UPTIME   - System uptime\n");
    puts("BEEP     - PC speaker beep (e.g. BEEP 440)\n");
    puts("HEXDUMP  - Memory hex dump (e.g. HEXDUMP B8000 64)\n");
    puts("DIR      - Real FAT12 directory listing\n");
    puts("CHKDSK   - Real FAT12 disk check\n");
}

static void cmd_dir(void) {
    fat_cmd_dir();
}

static void cmd_type(char *arg) {
    trim_left(&arg);
    // Special built-in files
    if (streq(arg, "ALLISON") || streq(arg, "ALLISON.EXE")) {
        print_allison_heart(); return;
    }
    // Try real FAT read first
    if (fat_cmd_type(arg) == 0) return;
    // Fallback: built-in README
    if (streq(arg, "README.TXT") || streq(arg, "README")) {
        puts("Matthew OS is a bootable real-mode shell demo.\n");
        puts("Keyboard input and DOS-like commands are working.\n");
        puts("FAT12 filesystem driver is active.\n");
        return;
    }
    puts_attr("File not found - ", ATTR_ERR); puts(arg); puts("\n");
}

static void cmd_mem(void) {
    puts("Memory Type        Total       Used       Free\n");
    puts("Conventional      640K        ~64K       ~576K\n");
    puts("Upper memory        0K          0K          0K\n");
    puts("Extended memory     N/A (real mode)\n");
    puts("\nKernel loaded at: 0x8000\n");
    puts("Stack top:        0x9000\n");
    puts("Video buffer:     0xB8000\n");
}

static void cmd_chkdsk(void) {
    fat_cmd_chkdsk();
}

static void cmd_tree(void) {
    fat_cmd_tree();
}

static void cmd_memmap(void) {
    puts_attr("Physical Memory Map\n", ATTR_TITLE);
    puts_attr("===================\n", ATTR_TITLE);
    puts_attr("0x00000 - 0x003FF  ", ATTR_OK);   puts("IVT  (Interrupt Vector Table, 256 vectors x 4 bytes)\n");
    puts_attr("0x00400 - 0x004FF  ", ATTR_OK);   puts("BDA  (BIOS Data Area)\n");
    puts_attr("0x00500 - 0x01FFF  ", ATTR_OK);   puts("Free conventional RAM\n");
    puts_attr("0x02000 - 0x0????  ", ATTR_OK);   puts("Kernel code + data (this OS)\n");
    puts_attr("0x06000 - 0x06960  ", ATTR_OK);   puts("Shell history buffer\n");
    puts_attr("0x0C000 - 0x0D000  ", ATTR_OK);   puts("Stack (4KB)\n");
    puts_attr("0x0D000 - 0x0E7FF  ", ATTR_OK);   puts("FAT12 sector/table/entry buffers\n");
    puts_attr("0x0E800 - 0x0EFFF  ", ATTR_OK);   puts("Text/Hex viewer buffer\n");
    puts_attr("0xA0000 - 0xAFFFF  ", ATTR_ERR);  puts("VGA graphics (not used)\n");
    puts_attr("0xB8000 - 0xB8FFF  ", ATTR_ERR);  puts("VGA text buffer (80x25 = 4000 bytes)\n");
    puts_attr("0xC0000 - 0xDFFFF  ", ATTR_DIM);  puts("VGA BIOS ROM\n");
    puts_attr("0xE0000 - 0xFFFFF  ", ATTR_DIM);  puts("System BIOS ROM\n");
    puts("\n");
    puts("Conventional RAM: 640KB (0x00000 - 0x9FFFF)\n");
    puts("Extended memory : not accessible in real mode\n");
}

static void cmd_uptime(void) {
    u32 total = time_ms / 1000;
    u32 h = total / 3600;
    u32 m = (total % 3600) / 60;
    u32 s = total % 60;
    puts("System uptime: ");
    if (h < 10) putc_attr('0', ATTR); print_dec(h); putc_attr(':', ATTR);
    if (m < 10) putc_attr('0', ATTR); print_dec(m); putc_attr(':', ATTR);
    if (s < 10) putc_attr('0', ATTR); print_dec(s);
    puts("\n");
    puts("Timer ticks   : "); print_dec(time_ms); puts(" ms\n");
}

static void cmd_beep(char *arg) {
    trim_left(&arg);
    u32 hz = 1000;
    if (arg[0] >= '0' && arg[0] <= '9') {
        hz = 0;
        while (*arg >= '0' && *arg <= '9') { hz = hz * 10 + (*arg - '0'); arg++; }
    }
    if (hz < 20)   hz = 20;
    if (hz > 8000) hz = 8000;
    u32 div = 1193180 / hz;
    // Set PIT channel 2 for speaker
    outb(0x43, 0xB6);
    outb(0x42, (u8)(div & 0xFF));
    outb(0x42, (u8)(div >> 8));
    // Enable speaker (bits 0 and 1 of port 0x61)
    u8 tmp = inb(0x61);
    outb(0x61, tmp | 0x03);
    // Beep for ~300ms
    u32 start = time_ms;
    while (time_ms - start < 300) {}
    // Disable speaker
    outb(0x61, tmp & ~0x03);
    puts("Beep! ("); print_dec(hz); puts(" Hz)\n");
}

static void cmd_hexdump(char *arg) {
    trim_left(&arg);
    // Parse hex address: HEXDUMP ADDR [len]
    u32 addr = 0;
    while (*arg && *arg != ' ') {
        char c = *arg++;
        if (c >= '0' && c <= '9') addr = addr * 16 + (c - '0');
        else if (c >= 'a' && c <= 'f') addr = addr * 16 + (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') addr = addr * 16 + (c - 'A' + 10);
    }
    trim_left(&arg);
    u32 len = 128;
    if (*arg >= '0' && *arg <= '9') {
        len = 0;
        while (*arg >= '0' && *arg <= '9') { len = len * 10 + (*arg - '0'); arg++; }
    }
    if (len > 512) len = 512;

    static const char hex[] = "0123456789ABCDEF";
    for (u32 i = 0; i < len; i += 16) {
        // Print address
        u32 a = addr + i;
        char abuf[6];
        for (int k = 4; k >= 0; k--) { abuf[k] = hex[a & 0xF]; a >>= 4; }
        abuf[5] = 0;
        puts_attr(abuf, ATTR_OK); puts(": ");
        // Hex bytes
        for (u32 j = 0; j < 16 && (i+j) < len; j++) {
            u8 b = *(u8*)(addr + i + j);
            putc_attr(hex[b >> 4], ATTR);
            putc_attr(hex[b & 0xF], ATTR);
            putc_attr(' ', ATTR);
        }
        puts(" | ");
        // ASCII
        for (u32 j = 0; j < 16 && (i+j) < len; j++) {
            u8 b = *(u8*)(addr + i + j);
            putc_attr((b >= 32 && b < 127) ? b : '.', ATTR);
        }
        putc_attr('\n', ATTR);
    }
}

static void cmd_sysinfo(void) {
    puts("Matthew OS v0.3 - System Information\n");
    puts("=====================================\n");
    puts("CPU    : Intel 486 (Real Mode, 16-bit)\n");
    puts("Video  : VGA Text 80x25, 16 colors\n");
    puts("Disk   : FAT12 Floppy 1.44MB (A:)\n");
    puts("RAM    : 640K Conventional\n");
    puts("Kernel : 0x8000 (32KB offset)\n");
    puts("Stack  : 4KB at 0x9000\n");
    puts("Timer  : PIT 1000 Hz\n");
    puts("Uptime : "); print_dec(time_ms / 1000); puts(" seconds\n");
}

static void not_impl(const char *name) {
    puts_attr(name, ATTR_ERR);
    puts(": write operations not supported (read-only FAT12 driver).\n");
}

static void execute(char *cmd) {
    char *p = cmd;
    trim_left(&p);
    if (p[0] == 0) return;

    // Easter eggs
    if (streq(p, "ALLISON")) { print_allison_heart(); return; }
    if (streq(p, "ELLEN"))   { putc_attr('a', ATTR); puts("\n"); return; }

    if (streq(p, "GUI"))    { gui_main(); print_banner(); return; }
    if (streq(p, "HELP") || streq(p, "?")) { cmd_help(); return; }
    if (streq(p, "CLS"))    { clear_screen(); return; }
    if (streq(p, "VER"))    { puts("Matthew OS Version 0.3\n"); return; }
    if (streq(p, "DATE"))   { puts("Current date: Sat 05-02-2026\n"); return; }
    if (streq(p, "TIME"))   { puts("Uptime: "); print_dec(time_ms); puts(" ms\n"); return; }
    if (streq(p, "DIR") || starts_with(p, "DIR "))  { cmd_dir(); return; }
    if (starts_with(p, "TYPE ")) { cmd_type(p + 5); return; }
    if (streq(p, "MEM"))    { cmd_mem(); return; }
    if (streq(p, "TREE"))   { cmd_tree(); return; }
    if (streq(p, "CHKDSK")) { cmd_chkdsk(); return; }
    if (streq(p, "VOL"))    { fat_cmd_vol(); return; }
    if (streq(p, "SYSINFO"))  { cmd_sysinfo(); return; }
    if (streq(p, "DISKINFO")) { fat_cmd_chkdsk(); return; }
    if (streq(p, "MEMMAP"))   { cmd_memmap(); return; }
    if (streq(p, "UPTIME"))   { cmd_uptime(); return; }
    if (starts_with(p, "BEEP"))    { cmd_beep(p + 4); return; }
    if (starts_with(p, "HEXDUMP")) { cmd_hexdump(p + 7); return; }
    if (streq(p, "ABOUT"))  { puts("Matthew OS - 486 homebrew project by Matthew Ahn\n"); return; }
    if (streq(p, "EXIT"))   { puts("Cannot exit: no parent OS.\n"); return; }
    if (streq(p, "REBOOT")) { puts("Rebooting...\n"); outb(0x64, 0xFE); while(1){} }
    if (streq(p, "PAUSE"))  { puts("Press any key to continue...\n"); get_key(); return; }
    if (starts_with(p, "REM ") || streq(p, "REM")) { return; }  // comment

    if (starts_with(p, "ECHO OFF")) { echo_on = 0; return; }
    if (starts_with(p, "ECHO ON"))  { echo_on = 1; return; }
    if (starts_with(p, "ECHO "))    { puts(p + 5); puts("\n"); return; }
    if (streq(p, "ECHO"))           { puts(echo_on ? "ECHO is on\n" : "ECHO is off\n"); return; }

    if (starts_with(p, "PROMPT ")) { puts("PROMPT: cosmetic only in this build.\n"); return; }

    // Drive switching
    if (p[1] == ':' && p[2] == 0) {
        char d = p[0];
        if (d >= 'a' && d <= 'z') d -= 32;
        if (d == 'A' || d == 'B') {
            current_drive = d;
            puts("Drive "); putc_attr(d, ATTR); puts(": selected (floppy)\n");
        } else if (d == 'C') {
            puts_attr("Drive C: not available (no hard disk).\n", ATTR_ERR);
        } else {
            puts_attr("Invalid drive.\n", ATTR_ERR);
        }
        return;
    }

    // CD / CHDIR - cosmetic (no subdirectory support in FAT12 root-only driver)
    if (starts_with(p, "CD") || starts_with(p, "CHDIR")) {
        char *arg = p + 2; trim_left(&arg);
        if (arg[0] == 0 || (arg[0] == '\\' && arg[1] == 0)) {
            puts("Current directory: "); putc_attr(current_drive, ATTR); puts(":\\\n");
        } else {
            puts_attr("Subdirectory navigation not supported (root only).\n", ATTR_ERR);
        }
        return;
    }

    // Write operations (not supported)
    if (starts_with(p, "MD ") || starts_with(p, "MKDIR ")) { not_impl("MD"); return; }
    if (starts_with(p, "RD ") || starts_with(p, "RMDIR ")) { not_impl("RD"); return; }
    if (starts_with(p, "COPY "))   { not_impl("COPY"); return; }
    if (starts_with(p, "DEL ") || starts_with(p, "ERASE ")) { not_impl("DEL"); return; }
    if (starts_with(p, "REN ") || starts_with(p, "RENAME ")) { not_impl("REN"); return; }
    if (starts_with(p, "FORMAT"))  { puts_attr("FORMAT blocked for safety.\n", ATTR_ERR); return; }
    if (starts_with(p, "SYS"))     { not_impl("SYS"); return; }
    if (starts_with(p, "COLOR"))   { puts("COLOR: VGA attribute changes not implemented.\n"); return; }
    if (starts_with(p, "SET "))    { puts("SET: environment variables not supported.\n"); return; }
    if (starts_with(p, "PATH"))    { puts("PATH=A:\\\n"); return; }

    puts_attr("Bad command or file name: ", ATTR_ERR); puts(p); puts("\n");
}

// ---------------- interrupts from original skeleton ----------------
void nmi_handler() {
    __asm("pusha");
    outb(0x20, 0x20);
    __asm("popa;leave;iret");
}

void keyboard_hanlder() {
    __asm("pusha");
    outb(0x20, 0x20);
    __asm("popa;leave;iret");
}

void pic_handler() {
    __asm("pusha");
    time_ms = time_ms + 1;
    outb(0x20, 0x20);
    __asm("popa;leave;iret");
}

void interrupt_setup() {
    set_timer_hz(1000);
    ivt_set_callback(&nmi_handler, 2);
    ivt_set_callback(&pic_handler, 8);
    ivt_set_callback(&keyboard_hanlder, 9);
}

#include "fat.c"
#include "gui.c"

void main(void) {
    __asm("cli");
    interrupt_setup();
    __asm("sti");
    resetDisk(0);

    print_banner();
    char line[96];
    while (1) {
        prompt();
        read_line(line, sizeof(line));
        execute(line);
    }
}
// PLACEHOLDER - will be replaced