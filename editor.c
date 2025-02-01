/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** output **/

// To draw the left column of characters
void editorDrawRows() {
    int y;
    for (y = 0; y < 24; y++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

// To initiliaze the display
void editorRefreshScreen() {
    // \x1b is the byte that represent the escape character
    // J is meant to clear the screen
    // 2 is meant to specify we want to clear the entire screen
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // H is meant to position the cursor on the terminal
    write(STDOUT_FILENO, "\x1b[H", 3);

    editorDrawRows();

    // After we finished drawing the left column, we reposition the cursor
    write(STDOUT_FILENO, "\x1b[H", 3);
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
        die('getWindowSize failed');
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