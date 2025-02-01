/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

// A global structure to keep track of the editor state
struct editorConfig {
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

/*** terminal ***/

// Error handling function
void die(const char *s) {
    // Before dying, we clear the screen
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

// Return user's terminal as before
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr failed");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) 
        die("tcgetattr failed");
    // To register an automatic function call once the program exits
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    // We flip the bits of these flags
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cflag |= (CS8);

    // We change some control characters
    // VMIN sets the minimum number of bytes before read() can return, 0 means return as soon as there is any input to be read
    // VTIME sets the maximum amount of time to wait before read() returns
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;


    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr failed");
}

// Function that returns input keypress
char editorReadKey() {
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read failed");
    }

    return c;
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }

    // The start end of a string should be \0
    buf[i] = '\0';

    // We check if the response start with an escape sequence
    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    // We parse the answer
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        // We move the cursor with the C (cursor forward) and B (cursor down) commands
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** append buffer ***/

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL)
        return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** output **/

// To draw the left column of characters
void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        abAppend(ab, "~", 1);

        if (y < E.screenrows - 1)
            abAppend(ab, "\r\n", 2);
    }
}

// To initiliaze the display
void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    // \x1b is the byte that represent the escape character
    // J is meant to clear the screen
    // 2 is meant to specify we want to clear the entire screen
    abAppend(&ab, "\x1b[2J", 4);
    // H is meant to position the cursor on the terminal
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    // After we finished drawing the left column, we reposition the cursor
    write(STDOUT_FILENO, "\x1b[H", 3);

    // We dump the buffer
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input ***/

// Waits for a key to be pressed then handle it
void editorProcessKeypress() {
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            // We clear the before exiting
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

/*** init **/

void initEditor() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWndowSize failed");
}

int main() {
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}