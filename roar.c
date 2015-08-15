#include <curses.h>
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#define GROUND_PLANE 0.25
#define GROUND_TEXTURE_SIZE 256
#define GROUND_MESSINESS 0.10
#define GROUND_PALLETTE " .,`'"
#define GROUND_PALLETTE_SIZE sizeof(GROUND_PALLETTE)

#define JUMP_MAX 0.25
#define JUMP_FRAMES 15

#define KEY_JUMP ' '
#define KEY_QUIT1 'q'
#define KEY_QUIT2 'Q'

#define FPS 30

static char game_is_running = 1;
static int step_count = 0;

static struct {
    float x, y;
    int row, col;
    int jump_end, jump_start;
    float jump;
} player;

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

static void clear_player()
{
    int player_col, player_row;
    xy2cr(player.x, player.y + player.jump,
          &player_col, &player_row);
    if(move(0, player.x) == ERR) {
        fprintf(stderr, "clear_player(): could not move()\n");
        return;
    }
    (void)clrtobot();
}

static void draw_player()
{
    mvaddch(player.row, player.col, 'P');
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
        stride = j * 17 + 1;
        idx = j * 31 + stride * step_count;
        for(i = 0; i < screen_width; i++) {
            idx = idx % GROUND_TEXTURE_SIZE;
            mvaddch(j, i, texture[idx]);
            idx += stride;
        }
    }
}

static void process_input()
{
    int ch;

    ch = getch();
    switch(ch) {
    case KEY_JUMP:
        if(player.jump_end < step_count) {
            player.jump_start = step_count;
            player.jump_end = step_count + JUMP_FRAMES;
        }
        return;
    case KEY_QUIT1:
    case KEY_QUIT2:
        game_is_running = 0;
    case ERR:
    default:
        return;
    }
}

static void update()
{
    step_count++;

    // compute the player jump amount
    if(player.jump_end > step_count) {
        int current = step_count - player.jump_start;
        int total = player.jump_end - player.jump_start;
        player.jump = JUMP_MAX * sin(M_PI * current / total);
    }
    else {
        player.jump = 0;
    }

    // compute the position of the player in row/col space
    xy2cr(player.x, player.y + player.jump,
          &player.col, &player.row);
}

static void draw_score()
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%d", step_count);
    mvaddstr(1, 1, "SCORE: ");
    addstr(buf);
}

static void draw()
{
    clear_player();
    draw_ground();
    draw_player();
    draw_score();
}

static void initialize()
{
    player.x = 0.1;
    player.y = GROUND_PLANE;
}

int main()
{
    float dt = 1.0 / FPS;

    setlocale(LC_ALL, "");

    initscr();
    cbreak();
    noecho();
    nonl();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    atexit(teardown);

    initialize();
    while(game_is_running) {
        process_input();
        update();
        usleep(1e6 * dt);
        draw();
    }

    return 0;
}
