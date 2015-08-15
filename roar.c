#include <curses.h>
#include <locale.h>
#include <math.h>
#include <signal.h>
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

#define OBSTACLE_HEIGHT_MAX 0.2
#define OBSTACLE_SPEED 0.0125
#define OBSTACLE_MAX_ARRIVAL 100

#define FPS 30

// various game data
static char game_is_running = 1;
static int step_count = 0;
static int next_obstacle = 0;

// obstacle structure
struct obstacle {
    float x, y, height;
    float speed;
    int row1, row2, col;
    struct obstacle* next;
};

// pointer to head of obstacle queue
static struct obstacle *obstacle_head = NULL;

// player-related data
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
    printf("Final score: %d\n", step_count);
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

// schedule the arrival time of the next obstacle
static void obstacle_schedule()
{
    next_obstacle = step_count + drand48() * OBSTACLE_MAX_ARRIVAL;
}

// add an obstacle to the queue
static void obstacle_push()
{
    struct obstacle* ob = malloc(sizeof(struct obstacle));
    if(ob == NULL) {
        perror("malloc()");
        return;
    }
    ob->height = drand48() * OBSTACLE_HEIGHT_MAX;
    ob->x = 1;
    ob->y = GROUND_PLANE;
    ob->speed = OBSTACLE_SPEED;
    ob->next = NULL;

    if(obstacle_head == NULL) {
        obstacle_head = ob;
    }
    else {
        struct obstacle* cur = obstacle_head;
        while(cur->next) {
            cur = cur->next;
        }
        cur->next = ob;
    }
}

// remove obstacle from top of queue
static void obstacle_pop()
{
    struct obstacle* next = obstacle_head->next;
    (void)free(obstacle_head);
    obstacle_head = next;
}

// draw all the obstacles
static void draw_obstacles()
{
    struct obstacle *ob = obstacle_head;
    while(ob) {
        int i;
        for(i = ob->row2; i <= ob->row1; i++) {
            mvaddch(i, ob->col, '#');
        }
        ob = ob->next;
    }
}

// draw the player
static void draw_player()
{
    mvaddch(player.row, player.col, 'P');
}

// draw the ground plane and texture
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

// handle player input
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

// game update logic
static void update()
{
    struct obstacle* ob;
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

    // update and compute positions of obstacles in row/col space
    ob = obstacle_head;
    while(ob) {
        ob->x -= ob->speed;
        xy2cr(ob->x, ob->y, &ob->col, &ob->row1);
        y2r(ob->y + ob->height, &ob->row2);
        ob = ob->next;
    }

    // pop off the obstacles which have gone off screen
    ob = obstacle_head;
    while(ob && ob->x < 0) {
        ob = ob->next;
        obstacle_pop();
    }

    // add a new obstacle if it is time
    if(next_obstacle <= step_count) {
        obstacle_push();
        obstacle_schedule();
    }
}

// draw the score counter
static void draw_score()
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%d", step_count);
    mvaddstr(1, 1, "SCORE: ");
    addstr(buf);
}

// draw all the game objects
static void draw()
{
    clear();
    draw_ground();
    draw_obstacles();
    draw_player();
    draw_score();
}

// initial game setup
static void initialize()
{
    player.x = 0.1;
    player.y = GROUND_PLANE;
    obstacle_schedule();
}

// signal handler to terminate game normally
void handle_signal(int signum)
{
    game_is_running = 0;
}

int main()
{
    // sleep this amount in between frames
    float dt = 1.0 / FPS;

    // signal handler which terminates the game normally
    struct sigaction action;
    action.sa_handler = &handle_signal;

    // curses recommends doing this to avoid undefined behavior
    setlocale(LC_ALL, "");

    // set up the terminal
    initscr();
    cbreak();
    noecho();
    nonl();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    // register various handlers
    atexit(teardown);
    sigaction(SIGINT, &action, NULL);

    // main game loop
    initialize();
    while(game_is_running) {
        process_input();
        update();
        usleep(1e6 * dt);
        draw();
    }

    exit(EXIT_SUCCESS);
}
