// fat.c
// FAT12 runtime filesystem driver for Matthew OS
// Reads the floppy disk (drive 0) using BIOS int 0x13h
// Supports: directory listing, file read, disk info
//
// FAT12 layout (1.44MB floppy):
//   Sector 0       : Boot sector (BPB)
//   Sectors 1-9    : FAT #1  (9 sectors)
//   Sectors 10-18  : FAT #2  (9 sectors)
//   Sectors 19-32  : Root directory (14 sectors, 224 entries)
//   Sectors 33+    : Data area (clusters 2+)

// ── BPB constants for 1.44MB floppy ──────────────────────────────────────────
#define FAT_BYTES_PER_SECTOR    512
#define FAT_SECTORS_PER_CLUSTER 1
#define FAT_RESERVED_SECTORS    1
#define FAT_NUM_FATS            2
#define FAT_ROOT_ENTRIES        224
#define FAT_TOTAL_SECTORS       2880
#define FAT_SECTORS_PER_FAT     9
#define FAT_SECTORS_PER_TRACK   18
#define FAT_NUM_HEADS           2
#define FAT_DRIVE               0       // floppy A:

// Derived constants
#define FAT_ROOT_START   (FAT_RESERVED_SECTORS + FAT_NUM_FATS * FAT_SECTORS_PER_FAT)
#define FAT_ROOT_SECTORS ((FAT_ROOT_ENTRIES * 32 + FAT_BYTES_PER_SECTOR - 1) / FAT_BYTES_PER_SECTOR)
#define FAT_DATA_START   (FAT_ROOT_START + FAT_ROOT_SECTORS)

// ── Sector buffer (one sector at a time to save RAM) ─────────────────────────
// Place large buffers at a fixed physical address above the kernel code.
// Kernel at 0x1000, ~46KB code → ends around 0xC400.
// Use 0xD000 for buffers (safely below video RAM at 0xB8000... wait,
// 0xD000 < 0xB8000 in linear address but 0xD000 is only 52KB — fine).
// Actually 0xD000 = 53248 decimal, 0xB8000 = 753664 — completely safe.
#define FAT_BUF_BASE    0xD000

// ── FAT directory entry (32 bytes) ───────────────────────────────────────────
typedef struct {
    u8  name[8];
    u8  ext[3];
    u8  attr;
    u8  reserved[10];
    u16 time;
    u16 date;
    u16 first_cluster;
    u32 size;
} __attribute__((packed)) FatDirEntry;

#define FAT_ATTR_READONLY   0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME     0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20

#define FAT_MAX_ENTRIES 32
typedef struct {
    char name[13];
    u8   attr;
    u32  size;
    u16  date;
    u16  time;
} FatEntry;

static u8       *fat_sector_buf = (u8 *)FAT_BUF_BASE;                        // 512 bytes  @ 0xA000
static u8       *fat_table      = (u8 *)(FAT_BUF_BASE + 512);                // 4608 bytes @ 0xA200
static FatEntry *fat_entries    = (FatEntry *)(FAT_BUF_BASE + 512 + 4608);   // 32 entries @ 0xB400
static int fat_loaded = 0;
static int fat_entry_count = 0;

// ── Low-level BIOS disk read ──────────────────────────────────────────────────
// Returns 0 on success, non-zero on error
static int fat_read_sector(u16 lba, u8 *buf) {
    u16 cyl    = lba / (FAT_SECTORS_PER_TRACK * FAT_NUM_HEADS);
    u16 temp   = lba % (FAT_SECTORS_PER_TRACK * FAT_NUM_HEADS);
    u16 head   = temp / FAT_SECTORS_PER_TRACK;
    u16 sector = temp % FAT_SECTORS_PER_TRACK + 1;

    u16 result;
    u8 retries = 3;
    while (retries--) {
        __asm__ volatile(
            "int $0x13"
            : "=a"(result)
            : "a"((u16)0x0201),          // AH=02 read, AL=1 sector
              "b"(buf),
              "c"((u16)((cyl << 8) | sector)),
              "d"((u16)((head << 8) | FAT_DRIVE))
            : "memory"
        );
        if ((result >> 8) == 0) return 0;  // success
        // reset drive and retry
        __asm__ volatile("int $0x13" : : "a"((u16)0x0000), "d"((u16)FAT_DRIVE));
    }
    return -1;
}

// ── Load FAT table into cache ─────────────────────────────────────────────────
static int fat_load_fat(void) {
    if (fat_loaded) return 0;
    for (int i = 0; i < FAT_SECTORS_PER_FAT; i++) {
        if (fat_read_sector(FAT_RESERVED_SECTORS + i,
                            fat_table + i * FAT_BYTES_PER_SECTOR) != 0)
            return -1;
    }
    fat_loaded = 1;
    return 0;
}

// ── Get next cluster from FAT12 chain ────────────────────────────────────────
static u16 fat_next_cluster(u16 cluster) {
    u32 offset = cluster + (cluster / 2);  // cluster * 1.5
    u16 val = (u16)fat_table[offset] | ((u16)fat_table[offset + 1] << 8);
    if (cluster & 1)
        val >>= 4;
    else
        val &= 0x0FFF;
    return val;
}

// ── Format 8.3 name from raw FAT entry into "NAME.EXT" style ─────────────────
static void fat_format_name(const u8 *name, const u8 *ext, char *out) {
    int i, j = 0;
    for (i = 0; i < 8 && name[i] != ' '; i++) out[j++] = name[i];
    if (ext[0] != ' ') {
        out[j++] = '.';
        for (i = 0; i < 3 && ext[i] != ' '; i++) out[j++] = ext[i];
    }
    out[j] = 0;
}

// ── Format FAT date (bits: year[15:9] month[8:5] day[4:0]) ───────────────────
static void fat_format_date(u16 date, char *out) {
    int day   = date & 0x1F;
    int month = (date >> 5) & 0x0F;
    int year  = ((date >> 9) & 0x7F) + 1980;
    // MM-DD-YY
    out[0] = '0' + month / 10; out[1] = '0' + month % 10;
    out[2] = '-';
    out[3] = '0' + day / 10;   out[4] = '0' + day % 10;
    out[5] = '-';
    int y2 = year % 100;
    out[6] = '0' + y2 / 10;    out[7] = '0' + y2 % 10;
    out[8] = 0;
}

// ── Format FAT time (bits: hour[15:11] min[10:5] sec[4:0]*2) ─────────────────
static void fat_format_time(u16 time, char *out) {
    int sec  = (time & 0x1F) * 2;
    int min  = (time >> 5) & 0x3F;
    int hour = (time >> 11) & 0x1F;
    out[0] = '0' + hour / 10; out[1] = '0' + hour % 10;
    out[2] = ':';
    out[3] = '0' + min / 10;  out[4] = '0' + min % 10;
    out[5] = 0;
    (void)sec;
}

// ── Print a u32 decimal number ────────────────────────────────────────────────
static void fat_print_u32(u32 v) {
    char buf[12];
    int i = 0;
    if (v == 0) { puts("0"); return; }
    while (v > 0 && i < 11) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i--) putc_attr(buf[i], ATTR);
}

// ── Right-align a number in a field of width w ───────────────────────────────
static void fat_print_u32_w(u32 v, int w) {
    char buf[12];
    int i = 0;
    if (v == 0) { buf[i++] = '0'; }
    else { while (v > 0 && i < 11) { buf[i++] = '0' + (v % 10); v /= 10; } }
    // pad
    int pad = w - i;
    while (pad-- > 0) putc_attr(' ', ATTR);
    while (i--) putc_attr(buf[i], ATTR);
}

// ── String compare (case-insensitive, up to n chars) ─────────────────────────
static int fat_strncmpi(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return 1;
        if (!ca) return 0;
    }
    return 0;
}

// ── Parse "FILENAME.EXT" into 8.3 padded form ────────────────────────────────
static void fat_parse_83(const char *in, u8 *name8, u8 *ext3) {
    int i;
    for (i = 0; i < 8; i++) name8[i] = ' ';
    for (i = 0; i < 3; i++) ext3[i]  = ' ';
    i = 0;
    int j = 0;
    while (in[i] && in[i] != '.' && j < 8) {
        char c = in[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        name8[j++] = c;
    }
    if (in[i] == '.') {
        i++; j = 0;
        while (in[i] && j < 3) {
            char c = in[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            ext3[j++] = c;
        }
    }
}

// ── DIR command: list root directory ─────────────────────────────────────────
void fat_cmd_dir(void) {
    if (fat_load_fat() != 0) {
        puts_attr("Disk read error loading FAT.\n", ATTR_ERR);
        return;
    }

    puts_attr(" Volume in drive A is MATTHEW\n", ATTR);
    puts_attr(" Directory of A:\\\n\n", ATTR);

    int file_count = 0;
    int dir_count  = 0;
    u32 total_bytes = 0;

    for (int s = 0; s < FAT_ROOT_SECTORS; s++) {
        if (fat_read_sector(FAT_ROOT_START + s, fat_sector_buf) != 0) {
            puts_attr("Disk read error.\n", ATTR_ERR);
            return;
        }
        FatDirEntry *entries = (FatDirEntry *)fat_sector_buf;
        int per_sector = FAT_BYTES_PER_SECTOR / 32;
        for (int e = 0; e < per_sector; e++) {
            FatDirEntry *de = &entries[e];
            if (de->name[0] == 0x00) goto done;       // end of directory
            if (de->name[0] == 0xE5) continue;         // deleted
            if (de->attr & FAT_ATTR_VOLUME) continue;  // volume label
            if (de->attr & FAT_ATTR_HIDDEN) continue;  // hidden

            char fname[13];
            fat_format_name(de->name, de->ext, fname);

            char date_s[10], time_s[8];
            fat_format_date(de->date, date_s);
            fat_format_time(de->time, time_s);

            if (de->attr & FAT_ATTR_DIRECTORY) {
                // <DIR>
                puts_attr(fname, ATTR_OK);
                // pad to 14 chars
                int l = 0; while (fname[l]) l++;
                for (int p = l; p < 14; p++) putc_attr(' ', ATTR);
                puts_attr("<DIR>         ", ATTR_OK);
                puts_attr(date_s, ATTR);
                putc_attr(' ', ATTR);
                puts_attr(time_s, ATTR);
                putc_attr('\n', ATTR);
                dir_count++;
            } else {
                // file
                puts_attr(fname, ATTR);
                int l = 0; while (fname[l]) l++;
                for (int p = l; p < 14; p++) putc_attr(' ', ATTR);
                fat_print_u32_w(de->size, 10);
                putc_attr(' ', ATTR);
                puts_attr(date_s, ATTR);
                putc_attr(' ', ATTR);
                puts_attr(time_s, ATTR);
                putc_attr('\n', ATTR);
                file_count++;
                total_bytes += de->size;
            }
        }
    }
done:
    putc_attr('\n', ATTR);
    fat_print_u32(file_count);
    puts_attr(" file(s)  ", ATTR);
    fat_print_u32(total_bytes);
    puts_attr(" bytes\n", ATTR);
    fat_print_u32(dir_count);
    puts_attr(" dir(s)   ", ATTR);
    // Free space: (total_sectors - used) * bytes_per_sector (approximate)
    u32 free_sectors = FAT_TOTAL_SECTORS - FAT_DATA_START;
    fat_print_u32(free_sectors * FAT_BYTES_PER_SECTOR);
    puts_attr(" bytes free\n", ATTR);
}

// ── TYPE command: print file contents ────────────────────────────────────────
// Returns 0 on success, -1 if not found
int fat_cmd_type(const char *filename) {
    if (fat_load_fat() != 0) {
        puts_attr("Disk read error loading FAT.\n", ATTR_ERR);
        return -1;
    }

    u8 name8[8], ext3[3];
    fat_parse_83(filename, name8, ext3);

    // Search root directory
    u16 first_cluster = 0;
    u32 file_size = 0;
    int found = 0;

    for (int s = 0; s < FAT_ROOT_SECTORS && !found; s++) {
        if (fat_read_sector(FAT_ROOT_START + s, fat_sector_buf) != 0) return -1;
        FatDirEntry *entries = (FatDirEntry *)fat_sector_buf;
        int per_sector = FAT_BYTES_PER_SECTOR / 32;
        for (int e = 0; e < per_sector && !found; e++) {
            FatDirEntry *de = &entries[e];
            if (de->name[0] == 0x00) goto type_done;
            if (de->name[0] == 0xE5) continue;
            if (de->attr & (FAT_ATTR_DIRECTORY | FAT_ATTR_VOLUME)) continue;
            if (fat_strncmpi((char*)de->name, (char*)name8, 8) == 0 &&
                fat_strncmpi((char*)de->ext,  (char*)ext3,  3) == 0) {
                first_cluster = de->first_cluster;
                file_size     = de->size;
                found = 1;
            }
        }
    }
type_done:
    if (!found) return -1;

    // Read and print cluster chain
    u32 bytes_left = file_size;
    u16 cluster = first_cluster;
    while (cluster >= 2 && cluster < 0xFF8 && bytes_left > 0) {
        u16 lba = FAT_DATA_START + (cluster - 2) * FAT_SECTORS_PER_CLUSTER;
        for (int s = 0; s < FAT_SECTORS_PER_CLUSTER && bytes_left > 0; s++) {
            if (fat_read_sector(lba + s, fat_sector_buf) != 0) {
                puts_attr("\nDisk read error.\n", ATTR_ERR);
                return -1;
            }
            u32 to_print = bytes_left < FAT_BYTES_PER_SECTOR
                           ? bytes_left : FAT_BYTES_PER_SECTOR;
            for (u32 i = 0; i < to_print; i++) {
                u8 c = fat_sector_buf[i];
                if (c == '\r') continue;
                if (c == '\n' || (c >= 32 && c < 127))
                    putc_attr((char)c, ATTR);
            }
            bytes_left -= to_print;
        }
        cluster = fat_next_cluster(cluster);
    }
    putc_attr('\n', ATTR);
    return 0;
}

// ── CHKDSK: scan FAT and report ───────────────────────────────────────────────
void fat_cmd_chkdsk(void) {
    if (fat_load_fat() != 0) {
        puts_attr("Disk read error loading FAT.\n", ATTR_ERR);
        return;
    }

    u32 free_clusters  = 0;
    u32 used_clusters  = 0;
    u32 bad_clusters   = 0;
    u32 total_clusters = FAT_TOTAL_SECTORS - FAT_DATA_START;

    for (u16 c = 2; c < (u16)(total_clusters + 2); c++) {
        u16 val = fat_next_cluster(c);
        if (val == 0x000)       free_clusters++;
        else if (val == 0xFF7)  bad_clusters++;
        else                    used_clusters++;
    }

    puts_attr("Volume MATTHEW\n", ATTR);
    puts_attr("File system type: FAT12\n\n", ATTR);
    puts_attr("Total disk space  : ", ATTR); fat_print_u32(FAT_TOTAL_SECTORS * FAT_BYTES_PER_SECTOR); puts_attr(" bytes\n", ATTR);
    puts_attr("Total clusters    : ", ATTR); fat_print_u32(total_clusters); putc_attr('\n', ATTR);
    puts_attr("Used clusters     : ", ATTR); fat_print_u32(used_clusters);  putc_attr('\n', ATTR);
    puts_attr("Free clusters     : ", ATTR); fat_print_u32(free_clusters);  putc_attr('\n', ATTR);
    if (bad_clusters)  { puts_attr("Bad clusters      : ", ATTR_ERR); fat_print_u32(bad_clusters); putc_attr('\n', ATTR); }
    puts_attr("\nAvailable disk space: ", ATTR);
    fat_print_u32(free_clusters * FAT_SECTORS_PER_CLUSTER * FAT_BYTES_PER_SECTOR);
    puts_attr(" bytes\n", ATTR);
}

// ── VOL: print volume label ───────────────────────────────────────────────────
void fat_cmd_vol(void) {
    // Volume label is in root dir as an entry with ATTR_VOLUME set
    for (int s = 0; s < FAT_ROOT_SECTORS; s++) {
        if (fat_read_sector(FAT_ROOT_START + s, fat_sector_buf) != 0) return;
        FatDirEntry *entries = (FatDirEntry *)fat_sector_buf;
        int per_sector = FAT_BYTES_PER_SECTOR / 32;
        for (int e = 0; e < per_sector; e++) {
            FatDirEntry *de = &entries[e];
            if (de->name[0] == 0x00) goto vol_done;
            if (de->attr & FAT_ATTR_VOLUME) {
                puts_attr(" Volume in drive A is ", ATTR);
                for (int i = 0; i < 8 && de->name[i] != ' '; i++)
                    putc_attr(de->name[i], ATTR);
                putc_attr('\n', ATTR);
                return;
            }
        }
    }
vol_done:
    puts_attr(" Volume in drive A has no label\n", ATTR);
}

// ── DIR listing for GUI file manager (fills fat_entries array) ───────────────
// Returns number of entries filled.
int fat_list_dir(void) {
    fat_entry_count = 0;
    if (fat_load_fat() != 0) return -1;

    for (int s = 0; s < FAT_ROOT_SECTORS; s++) {
        if (fat_read_sector(FAT_ROOT_START + s, fat_sector_buf) != 0) return -1;
        FatDirEntry *entries = (FatDirEntry *)fat_sector_buf;
        int per_sector = FAT_BYTES_PER_SECTOR / 32;
        for (int e = 0; e < per_sector; e++) {
            FatDirEntry *de = &entries[e];
            if (de->name[0] == 0x00) return fat_entry_count;
            if (de->name[0] == 0xE5) continue;
            if (de->attr & FAT_ATTR_VOLUME) continue;
            if (de->attr & FAT_ATTR_HIDDEN) continue;
            if (fat_entry_count >= FAT_MAX_ENTRIES) return fat_entry_count;

            FatEntry *fe = &fat_entries[fat_entry_count++];
            fat_format_name(de->name, de->ext, fe->name);
            fe->attr = de->attr;
            fe->size = de->size;
            fe->date = de->date;
            fe->time = de->time;
        }
    }
    return fat_entry_count;
}

// ── TREE command: tree-style listing ─────────────────────────────────────────
void fat_cmd_tree(void) {
    int n = fat_list_dir();
    if (n < 0) { puts_attr("Disk read error.\n", ATTR_ERR); return; }
    puts("A:\\\n");
    for (int i = 0; i < n; i++) {
        int is_last = (i == n - 1);
        if (fat_entries[i].attr & FAT_ATTR_DIRECTORY) {
            puts(is_last ? "\\---" : "+---");
            puts(fat_entries[i].name);
            puts("\\\n");
        } else {
            puts(is_last ? "    " : "|   ");
            puts(fat_entries[i].name);
            puts("\n");
        }
    }
}
