#ifndef _KILO_H
#define _KILO_H
#include <stdio.h>
#include <termios.h>
/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define _DEFUALT_SOURCE

/** struct & data **/
typedef struct erow {
    int size;
    char * chars;
    char * render;
    int rsize;
} erow;

struct editor_config {
    int cx;
    int cy;
    int rx;
    int screenrows;
    int screencols;
    int coloff;
    int rowoff;
    int numrows;
    int erow_size;
    char * filename;
    erow * row;
    struct termios orig_termios;
};
enum editor_key{
    ARROW_LEFT =1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_DOWN,
    PAGE_UP,
    HOME_KEY,
    END_KEY,
    DEL_KEY
};

/*** append buffer ***/

struct abuf{
    char * b;
    int size;
    int len;
};


/*** header ***/
void disable_raw_mode(void);
void enable_raw_mode(void);
void die(const char *s);
int editor_read_key(void);
void editor_process_key(void);
void editor_refresh_screen(void);
void editor_draw_rows(struct abuf * ab);
void init_editor(void);
int get_window_size(int * rows, int * cols);
int get_cursor_position(int * row, int * cols);
void editor_move_cursor(int key);
void editor_append_row(char * s, size_t len);
void editor_open(char * filename);
void editor_scroll();
void editor_update_row(erow *);
int editor_row_cx_to_rx(erow * row, int cx);
void editor_draw_status_bar(struct abuf * ab);
char * strdup(const char *);


#define ABUF_INIT {NULL, 0, 0}

#endif
