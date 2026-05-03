#!/usr/bin/env python3
import re

with open('main.c', 'r', encoding='utf-8', errors='replace') as f:
    content = f.read()

start_marker = '// ---------------- keyboard polling ----------------'
end_marker   = '// ---------------- Allison heart easter egg ----------------'

start_idx = content.find(start_marker)
end_idx   = content.find(end_marker)

if start_idx == -1 or end_idx == -1:
    print('ERROR: markers not found', start_idx, end_idx)
    exit(1)

print(f'Replacing keyboard section [{start_idx}:{end_idx}]')

new_keyboard = (
'// ---------------- keyboard polling ----------------\n'
'// Special key return values (non-printable control codes)\n'
'#define KEY_UP    (\'\\x01\')\n'
'#define KEY_DOWN  (\'\\x02\')\n'
'#define KEY_LEFT  (\'\\x03\')\n'
'#define KEY_RIGHT (\'\\x04\')\n'
'#define KEY_F1    (\'\\x10\')\n'
'#define KEY_F2    (\'\\x11\')\n'
'#define KEY_F3    (\'\\x12\')\n'
'#define KEY_F5    (\'\\x14\')\n'
'#define KEY_F10   (\'\\x19\')\n'
'#define KEY_HOME  (\'\\x05\')\n'
'#define KEY_END   (\'\\x06\')\n'
'#define KEY_PGUP  (\'\\x07\')\n'
'#define KEY_PGDN  (\'\\x08\')\n'
'#define KEY_DEL   (\'\\x7F\')\n'
'\n'
'static const char scancode_normal[128] = {\n'
'    0,   27,  \'1\',\'2\',\'3\',\'4\',\'5\',\'6\',\'7\',\'8\',\'9\',\'0\',\'-\',\'=\', \'\\b\',\n'
'    \'\\t\',\'q\',\'w\',\'e\',\'r\',\'t\',\'y\',\'u\',\'i\',\'o\',\'p\',\'[\',\']\',\'\\n\', 0,\n'
'    \'a\',\'s\',\'d\',\'f\',\'g\',\'h\',\'j\',\'k\',\'l\',\';\',\'\\\'\',\'`\', 0, \'\\\\\',\n'
'    \'z\',\'x\',\'c\',\'v\',\'b\',\'n\',\'m\',\',\',\'.\',\'/\', 0,  \'*\', 0,  \' \',\n'
'};\n'
'\n'
'static const char scancode_shift[128] = {\n'
'    0,   27,  \'!\',\'@\',\'#\',\'$\',\'%\',\'^\',\'&\',\'*\',\'(\',\')\',\'_\',\'+\', \'\\b\',\n'
'    \'\\t\',\'Q\',\'W\',\'E\',\'R\',\'T\',\'Y\',\'U\',\'I\',\'O\',\'P\',\'{\',\'}\',\'\\n\', 0,\n'
'    \'A\',\'S\',\'D\',\'F\',\'G\',\'H\',\'J\',\'K\',\'L\',\':\',\'"\',\'~\', 0,  \'|\',\n'
'    \'Z\',\'X\',\'C\',\'V\',\'B\',\'N\',\'M\',\'<\',\'>\',\'?\', 0,  \'*\', 0,  \' \',\n'
'};\n'
'\n'
'static int shift_down = 0;\n'
'static int caps_on    = 0;\n'
'\n'
'static char get_key(void) {\n'
'    while (1) {\n'
'        if ((inb(0x64) & 1) == 0) continue;\n'
'        u8 sc = inb(0x60);\n'
'        if (sc == 0xAA || sc == 0xB6) { shift_down = 0; continue; }\n'
'        if (sc & 0x80) continue;\n'
'        if (sc == 0x2A || sc == 0x36) { shift_down = 1; continue; }\n'
'        if (sc == 0x3A) { caps_on = !caps_on; continue; }\n'
'        // Extended key prefix 0xE0 (arrows, etc.)\n'
'        if (sc == 0xE0) {\n'
'            while ((inb(0x64) & 1) == 0) {}\n'
'            u8 sc2 = inb(0x60);\n'
'            if (sc2 & 0x80) continue;\n'
'            switch (sc2) {\n'
'                case 0x48: return KEY_UP;\n'
'                case 0x50: return KEY_DOWN;\n'
'                case 0x4B: return KEY_LEFT;\n'
'                case 0x4D: return KEY_RIGHT;\n'
'                case 0x47: return KEY_HOME;\n'
'                case 0x4F: return KEY_END;\n'
'                case 0x49: return KEY_PGUP;\n'
'                case 0x51: return KEY_PGDN;\n'
'                case 0x53: return KEY_DEL;\n'
'            }\n'
'            continue;\n'
'        }\n'
'        if (sc == 0x3B) return KEY_F1;\n'
'        if (sc == 0x3C) return KEY_F2;\n'
'        if (sc == 0x3D) return KEY_F3;\n'
'        if (sc == 0x3F) return KEY_F5;\n'
'        if (sc == 0x44) return KEY_F10;\n'
'        char c = shift_down ? scancode_shift[sc] : scancode_normal[sc];\n'
'        if (c >= \'a\' && c <= \'z\' && caps_on) c -= 32;\n'
'        if (c >= \'A\' && c <= \'Z\' && caps_on && shift_down) c += 32;\n'
'        if (c) return c;\n'
'    }\n'
'}\n'
'\n'
'// Non-blocking: returns 0 if no key pending\n'
'static char get_key_nb(void) {\n'
'    if ((inb(0x64) & 1) == 0) return 0;\n'
'    return get_key();\n'
'}\n'
'\n'
)

content = content[:start_idx] + new_keyboard + content[end_idx:]

with open('main.c', 'w', encoding='utf-8') as f:
    f.write(content)

print('keyboard section replaced OK')

# Now fix read_line to support history + arrow keys
old_readline = '''static void read_line(char *buf, int max) {
    int len = 0;
    while (1) {
        char c = get_key();
        if (c == '\\n' || c == '\\r') {
            buf[len] = 0;
            newline();
            return;
        }
        if (c == '\\b') {
            if (len > 0) {
                len--;
                putc_attr('\\b', ATTR);
            }
            continue;
        }
        if (c >= 32 && c <= 126 && len < max - 1) {
            buf[len++] = c;
            if (echo_on) putc_attr(c, ATTR);
        }
    }
}'''

new_readline = (
'// Shell command history\n'
'#define HIST_MAX  10\n'
'#define HIST_LEN  96\n'
'static char hist_buf[HIST_MAX][HIST_LEN];\n'
'static int  hist_count = 0;\n'
'static int  hist_head  = 0;  // index of next slot to write\n'
'\n'
'static void hist_push(const char *cmd) {\n'
'    if (!cmd[0]) return;\n'
'    // avoid duplicate of last entry\n'
'    int last = (hist_head - 1 + HIST_MAX) % HIST_MAX;\n'
'    if (hist_count > 0) {\n'
'        int match = 1;\n'
'        for (int i = 0; hist_buf[last][i] || cmd[i]; i++)\n'
'            if (hist_buf[last][i] != cmd[i]) { match = 0; break; }\n'
'        if (match) return;\n'
'    }\n'
'    for (int i = 0; i < HIST_LEN-1 && cmd[i]; i++) hist_buf[hist_head][i] = cmd[i];\n'
'    hist_buf[hist_head][HIST_LEN-1] = 0;\n'
'    hist_head = (hist_head + 1) % HIST_MAX;\n'
'    if (hist_count < HIST_MAX) hist_count++;\n'
'}\n'
'\n'
'static void read_line(char *buf, int max) {\n'
'    int len = 0;\n'
'    int hist_pos = -1;  // -1 = not browsing history\n'
'    int start_x = cur_x, start_y = cur_y;\n'
'\n'
'    // Helper: redraw current input\n'
'    #define RL_REDRAW() do { \\\n'
'        cur_x = start_x; cur_y = start_y; \\\n'
'        for (int _i = 0; _i < max-1; _i++) put_cell(start_x+_i, start_y, \' \', ATTR); \\\n'
'        for (int _i = 0; _i < len; _i++) { \\\n'
'            put_cell(start_x+_i, start_y, buf[_i], ATTR); \\\n'
'        } \\\n'
'        cur_x = start_x + len; \\\n'
'    } while(0)\n'
'\n'
'    while (1) {\n'
'        char c = get_key();\n'
'        if (c == \'\\n\' || c == \'\\r\') {\n'
'            buf[len] = 0;\n'
'            newline();\n'
'            hist_push(buf);\n'
'            return;\n'
'        }\n'
'        if (c == \'\\b\') {\n'
'            if (len > 0) { len--; putc_attr(\'\\b\', ATTR); }\n'
'            continue;\n'
'        }\n'
'        // Up arrow: go back in history\n'
'        if (c == KEY_UP) {\n'
'            if (hist_count == 0) continue;\n'
'            if (hist_pos < hist_count - 1) hist_pos++;\n'
'            int idx = (hist_head - 1 - hist_pos + HIST_MAX * 2) % HIST_MAX;\n'
'            len = 0;\n'
'            while (hist_buf[idx][len] && len < max-1) { buf[len] = hist_buf[idx][len]; len++; }\n'
'            buf[len] = 0;\n'
'            RL_REDRAW();\n'
'            continue;\n'
'        }\n'
'        // Down arrow: go forward in history\n'
'        if (c == KEY_DOWN) {\n'
'            if (hist_pos <= 0) { hist_pos = -1; len = 0; buf[0] = 0; RL_REDRAW(); continue; }\n'
'            hist_pos--;\n'
'            int idx = (hist_head - 1 - hist_pos + HIST_MAX * 2) % HIST_MAX;\n'
'            len = 0;\n'
'            while (hist_buf[idx][len] && len < max-1) { buf[len] = hist_buf[idx][len]; len++; }\n'
'            buf[len] = 0;\n'
'            RL_REDRAW();\n'
'            continue;\n'
'        }\n'
'        // Ignore other special keys in shell input\n'
'        if ((unsigned char)c < 32 && c != \'\\b\') continue;\n'
'        if (c >= 32 && c <= 126 && len < max - 1) {\n'
'            buf[len++] = c;\n'
'            if (echo_on) putc_attr(c, ATTR);\n'
'        }\n'
'    }\n'
'    #undef RL_REDRAW\n'
'}\n'
)

if old_readline in content:
    content = content.replace(old_readline, new_readline, 1)
    print('read_line replaced OK')
else:
    print('WARNING: read_line not found exactly, trying partial match...')
    # find and replace just the function
    rl_start = content.find('static void read_line(char *buf, int max)')
    rl_end   = content.find('\n}\n', rl_start) + 3
    if rl_start != -1:
        content = content[:rl_start] + new_readline + content[rl_end:]
        print('read_line replaced via partial match OK')
    else:
        print('ERROR: read_line not found')

with open('main.c', 'w', encoding='utf-8') as f:
    f.write(content)

print('All done!')
