/*** includes ***/
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "kilo.h"

char * logfile  = "log.txt";
FILE * logger;

void ab_append(struct abuf * ab, const char * s, int len){
    if(ab->len+len >= ab->size - 1){
        char * new = realloc(ab->b, ab->size+512+len);
        if(new == NULL) return;
        ab->size += len+512;
        ab->b = new;
    }
    memcpy(&ab->b[ab->len], s, len);
    ab->len += len;
}

void ab_free(struct abuf * ab){
    free(ab->b);
}


struct editor_config E;

/* file i/o */
void editor_open( char * filename ){
    free(E.filename);
    E.filename = strdup(filename);
    /* fprintf(logger, "%s", E.filename); */
    FILE * fp = fopen(filename, "r");
    if (!fp) die("fopen");
    char * line = NULL; 
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen =getline(&line, &linecap, fp)) != -1){
        while (linelen > 0 && ( line[linelen - 1] == '\n'  || 
                                line [linelen - 1] == '\r'))
            linelen--;
        /* fprintf(logger, "Linelen: %lu\n", linelen); */
        editor_append_row(line, linelen);
    }
    free(line);
    fclose(fp);
    
}

/*** init ***/
int main(int argc, char ** argv)
{
    logger = fopen(logfile, "w");
    char * name = "daniel";
    char * newname = strdup(name);
    printf("newname: %s\n", newname);
    enable_raw_mode();
    init_editor();
    if (argc >= 2){
         editor_open(argv[1]); 
    }
    while (1){
        editor_refresh_screen();
        editor_process_key();
        fprintf(logger, "E.cy: %d screenrow: %d rowoff: %d\n", E.cy,  E.screenrows, E.rowoff);
    }
    fclose(logger);
    return 0;
}

void init_editor(void){
    E.cx =0;
    E.cy =0;
    E.rx = 0;
    E.numrows = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.row = NULL;
    E.filename=NULL;
    if (get_window_size(&E.screenrows, &E.screencols) == -1)
        die("get_window_size");
    E.screenrows -=1;
}

/*** terminal ***/
void enable_raw_mode(void)
{
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
    atexit(disable_raw_mode);
    struct termios raw = E.orig_termios;

    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_oflag &= ~OPOST;
    raw.c_cflag |= (CS8);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cc[VMIN]=0;
    raw.c_cc[VTIME]=1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); 
}

void disable_raw_mode(void)
{
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    fclose(logger);
    exit(1);
}

int editor_read_key()
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b'){
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '['){
            if (seq[1] >= '0' && seq[1] <= '9'){
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~'){
                    switch(seq[1]){
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }
            else {
                switch(seq[1]){
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;

                }
            }
        } else if (seq[0] == '0'){
            switch (seq[1]){
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';

    }else{
        return c;
    }
    
}

int get_window_size(int * rows, int * cols)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write (STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) !=12) return -1;
        return get_cursor_position(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

int get_cursor_position(int * rows, int * cols)
{
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf)-1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;

    if (sscanf(buf+2, "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

/*** row operation ***/
void editor_append_row(char * s, size_t len){
    if (E.numrows >= E.erow_size){
        int len = E.erow_size + 128;
        E.row = realloc(E.row, len * sizeof(* E.row));
        if (E.row == NULL) die("realloc");
        E.erow_size = len;
    }

    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editor_update_row(&E.row[at]);
    E.numrows++;
}

void editor_update_row(erow * row){
    int tabs = 0;
    int j;
    for(j = 0; j < row->size; j++) 
        if (row->chars[j] == '\t') tabs++;
    free(row->render);

    row->render = malloc(row->size + tabs*7+1);
    int idx = 0;
    for (j = 0; j < row->size; j++)
        if(row->chars[j] == '\t'){
            row->render[idx++]= ' ';
            while (idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';
        }else{
            row->render[idx++] = row->chars[j];
        }

    row->render[idx]= '\0';
    row->rsize = idx;
}


int editor_row_cx_to_rx(erow * row, int cx){
    int rx = 0;
    for (int j = 0; j < cx; j++){
        if (row->chars[j] == '\t')
            rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        rx++;
    }
    return rx;
}

/*** input ***/

void editor_process_key()
{
    int c = editor_read_key();
    switch(c){
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case ARROW_UP: case ARROW_RIGHT: case ARROW_DOWN: case ARROW_LEFT: 
            editor_move_cursor(c);
            break;

        case PAGE_UP: case PAGE_DOWN:
            {
                if (c == PAGE_UP) E.cy = E.rowoff;
                else if (c == PAGE_DOWN) {
                    E.cy = E.rowoff + E.screenrows - 1;
                    if (E.cy > E.numrows) E.cy = E.numrows;
                }
                int times = E.screenrows;
                while(times--){
                    editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;

        case HOME_KEY: case END_KEY:
            editor_move_cursor(c);
    }
}
            
void editor_move_cursor(int key){
    erow * row = (E.cy >= E.numrows)? NULL : &E.row[E.cy];
    switch (key){ case ARROW_LEFT:
            if (E.cx != 0)
                E.cx--;
            else if (E.cy > 0){
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_UP:
            if (E.cy != 0)
                E.cy--;
            break;
        case ARROW_DOWN:
            if (E.cy < E.numrows)
                E.cy++;
            break;
        case ARROW_RIGHT:
            if (row && E.cx < row->size)
                E.cx++;
            else if (row && E.row->size){
                E.cy++;
                E.cx = 0;
            }
            break;

        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if (E.cy < E.numrows)
                E.cx = E.row[E.cy].size;
            break;
    }

    row = (E.cy >= E.numrows)? NULL: &E.row[E.cy];
    int rowlen = row? row->size : 0;
    if (E.cx > rowlen)
        E.cx = rowlen;
}

/*** output ***/

void editor_scroll(){
    E.rx =0; 
    if (E.cy < E.numrows){
        E.rx = editor_row_cx_to_rx(&E.row[E.cy], E.cx);
    }
    if(E.cy < E.rowoff){
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows){
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.rx <E.coloff){
        E.coloff = E.rx;
    }
    if (E.rx >= E.coloff + E.screencols){
        E.coloff = E.rx - E.screencols + 1;
    }
}

void editor_refresh_screen()
{
    editor_scroll();
    struct abuf ab = ABUF_INIT;
    ab_append(&ab, "\x1b[?25l", 6);
    ab_append(&ab, "\x1b[H", 3);
    editor_draw_rows(&ab);
    editor_draw_status_bar(&ab);
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy- E.rowoff)+1, (E.rx - E.coloff)+1);
    ab_append(&ab, buf, strlen(buf));
    ab_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}

void editor_draw_rows( struct abuf * ab )
{
    int y;
    for(y=0; y < E.screenrows; y++){
        int filerow = y + E.rowoff;
        if (filerow>=E.numrows){
        if (E.numrows == 0 && y == E.screenrows/2){
            char welcome[80];
            int welcome_len = snprintf(welcome, sizeof(welcome), 
                    "Kilo editor -- version %s ", KILO_VERSION);

            if (welcome_len > E.screencols) welcome_len = E.screencols;
            int padding = (E.screencols - welcome_len) / 2;
            if(padding){
                ab_append(ab, "~", 1);
            }
                while(--padding) ab_append(ab, " ", 1);
                ab_append(ab, welcome, welcome_len);
        } else{
            ab_append(ab, "~", 1);
        }
        } else {
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;
            /* fprintf(logger, "in Editor draw rows\n"); */
            /* fprintf(logger, "y: %d, text: %s\n", y, E.row[filerow].chars); */
            ab_append(ab, &E.row[filerow].render[E.coloff], len);
        }

        ab_append(ab, "\x1b[K", 3);
        ab_append(ab, "\r\n", 2);
    }
}

void editor_draw_status_bar(struct abuf * ab){
    ab_append(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%s - %d lines",
             E.filename? E.filename: "[No Name]", E.numrows);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy+1, E.numrows);
    if (len > E.screencols)len = E.screencols;
    ab_append(ab, status, len);
    while (len < E.screencols){
        if (E.screencols - len == rlen){
            ab_append(ab, rstatus, rlen);
            break;
        }else {
            ab_append(ab, " ", 1);
            len++;
        }
    }
    ab_append(ab, "\x1b[m", 3);
}
