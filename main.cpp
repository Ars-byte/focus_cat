#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#define R   "\033[0m"
#define ORG "\033[38;5;214m"
#define PNK "\033[38;5;218m"
#define BLD "\033[1m"

static struct termios g_orig;
static std::atomic<bool> g_run{true};
static void mv(int r, int c) { printf("\033[%d;%dH", r, c); }
static void cls()             { printf("\033[2J"); fflush(stdout); }
static void get_size(int &rows, int &cols) {
    struct winsize ws{};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    rows = ws.ws_row > 6  ? ws.ws_row : 24;
    cols = ws.ws_col > 18 ? ws.ws_col : 80;
}
static void raw_on() {
    tcgetattr(STDIN_FILENO, &g_orig);
    struct termios t = g_orig;
    t.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    t.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
}
static void cleanup() {
    printf("\033[?1003l\033[?1006l\033[?25h");
    cls(); mv(1,1); fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig);
}
static void on_sig(int) { g_run = false; }
static const int CW = 15;
static const int CH = 4;
static const char* SPRITE[2][CH] = {
    {
        "|\\__/,|   (`\\ ",
        " |_ _  |.--.) )",
        " ( T   )     / ",
        "(((^_(((/(((_/ ",
    },
    {
        "  /`)   |,/__\\|",
        "( (.--.|  _ _| ",
        " \\     (   T ) ",
        " \\_)))\\)))_^)))",
    },
};
static void erase_cat(int row, int col) {
    for (int r = 0; r < CH; ++r) {
        mv(row + r, col);
        printf("               ");
    }
}
static void draw_cat(int row, int col, int dir, bool excited) {
    const char* color = excited ? PNK BLD : ORG BLD;
    for (int r = 0; r < CH; ++r) {
        mv(row + r, col);
        printf("%s%s%s", color, SPRITE[dir][r], R);
    }
}
static bool parse_mouse(const char* buf, int n, int &mx, int &my) {
    if (n < 6 || buf[0]!='\033' || buf[1]!='[' || buf[2]!='<') return false;
    int btn, x, y; char fin;
    return sscanf(buf+3, "%d;%d;%d%c", &btn, &x, &y, &fin)==4
           ? (mx=x, my=y, true) : false;
}
int main() {
    signal(SIGINT,  on_sig);
    signal(SIGTERM, on_sig);
    raw_on();
    atexit(cleanup);

    printf("\033[?1003h\033[?1006h\033[?25l");
    cls();

    int rows, cols;
    get_size(rows, cols);

    double cx = cols / 2.0, cy = rows / 2.0;
    double mx = cx, my = cy;

    int prev_col = (int)cx, prev_row = (int)cy;
    int dir = 0;

    const double SPEED = 0.13;
    const int    MS    = 40;

    char buf[256];
    auto t_last = std::chrono::steady_clock::now();
    int frame = 0;

    while (g_run) {
        int n = (int)read(STDIN_FILENO, buf, sizeof(buf)-1);
        if (n > 0) {
            buf[n] = '\0';
            for (int i = 0; i < n; ++i)
                if (buf[i]=='q' || buf[i]=='Q') { g_run = false; break; }
            int tmx, tmy;
            if (parse_mouse(buf, n, tmx, tmy)) { mx=tmx; my=tmy; }
        }

        auto now = std::chrono::steady_clock::now();
        auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now - t_last).count();
        if (ms < MS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(MS - ms));
            continue;
        }
        t_last = now;
        frame++;
        double tx = mx - CW / 2.0;
        double ty = my - CH / 2.0;
        double dx = (tx - cx);
        double dy = (ty - cy) * 0.5;
        double dist = std::sqrt(dx*dx + dy*dy);

        double old_cx = cx;
        if (dist > 0.5) {
            cx += (tx - cx) * SPEED;
            cy += (ty - cy) * SPEED;
        }
        if      (cx > old_cx + 0.05) dir = 0;
        else if (cx < old_cx - 0.05) dir = 1;

        get_size(rows, cols);
        int nc = std::clamp((int)std::round(cx), 1, cols - CW);
        int nr = std::clamp((int)std::round(cy), 1, rows - CH);

        bool excited = dist < 2.0;

        erase_cat(prev_row, prev_col);
        draw_cat(nr, nc, dir, excited);
        fflush(stdout);

        prev_col = nc;
        prev_row = nr;
    }

    return 0;
}