#include <curses.h>
#include <locale.h>
#include <stdlib.h>

#define GROUND_PLANE 0.25
#define GROUND_TEXTURE_SIZE 256
#define GROUND_MESSINESS 0.10
#define GROUND_PALLETTE " .,`'"
#define GROUND_PALLETTE_SIZE 5

#define LOG_FILE "roar.log"

FILE* _log;
static float player_x = 0.1, player_y = GROUND_PLANE;

// graphics to draw for the ground texture, lazy initialized
char *_ground_texture;

// initialize/get the ground texture
static char* get_ground_texture() {
    int i;
    int idx;
    if(_ground_texture != NULL) {
        return _ground_texture;
    }
    _ground_texture = malloc(GROUND_TEXTURE_SIZE * sizeof(*_ground_texture));
    if(_ground_texture == NULL) {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    for(i = 0; i < GROUND_TEXTURE_SIZE; i++) {
        if(drand48() < GROUND_MESSINESS) {
            idx = drand48() * (GROUND_PALLETTE_SIZE - 1);
            _ground_texture[i] = GROUND_PALLETTE[1 + idx];
        }
        else {
            _ground_texture[i] = GROUND_PALLETTE[0];
        }
    }

    return _ground_texture;
}

// reset terminal to a sane state
static void teardown()
{
    (void)free(_ground_texture);
    (void)endwin();
}

// convert x coordinate to column
static void x2c(float x, int *col)
{
    int width, _unused;
    getmaxyx(stdscr, _unused, width);

    *col = width * x;
}

// convert y coordinate to row
static void y2r(float y, int *row)
{
    int height, _unused;
    getmaxyx(stdscr, height, _unused);

    *row = height - height * y;
}

// convert x and y to column and row
static void xy2cr(float x, float y, int *col, int *row)
{
    x2c(x, col);
    y2r(y, row);
}

static void draw_player()
{
    int player_col, player_row;
    xy2cr(player_x, player_y, &player_col, &player_row);
    mvaddch(player_row, player_col, 'P');
}

static void draw_ground()
{
    int ground_row;
    int screen_width, screen_height;
    int i, j, idx, stride;
    char *texture = get_ground_texture();

    y2r(GROUND_PLANE, &ground_row);
    getmaxyx(stdscr, screen_height, screen_width);

    for(i = 0; i < screen_width; i++) {
        mvaddch(ground_row, i, '-');
    }

    for(j = ground_row + 1; j < screen_height; j++) {
        idx = j * 31;
        stride = j * 17 + 1;
        for(i = 0; i < screen_width; i++) {
            idx = idx % GROUND_TEXTURE_SIZE;
            mvaddch(j, i, texture[idx]);
            idx += stride;
        }
    }
}

static void draw()
{
    draw_ground();
    draw_player();
}

int main()
{
    setlocale(LC_ALL, "");
    initscr();
    atexit(teardown);

    draw();
    getch();

    return 0;
}
