//
//  main.c
//  snake
//
//  Created by Dietmar Planitzer on 3/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

//
// Based on the snake sample code from the geeksforgeeks.org website
//

#include <ctype.h>
#include <dispatch.h>
#include <stdbool.h>
#include <stdio.h>
#include <ext/string.h>
#include <flowterm.h>
#include <time.h>
#include <ext/math.h>
#include <ext/stdlib.h>
#include <ext/nanotime.h>


#define INFO_HEIGHT 5

#define DRAW_FULL_FRAME         1
#define DRAW_SNAKE_MOVE         2
#define DRAW_SNAKE_GROWTH       4
#define DRAW_FRUIT_PLACEMENT    8
#define DRAW_SCORE_CHANGE       16


static ft_event_t event;
static nanotime_t game_loop_delay;

static int playfield_x, playfield_y;
static int playfield_width, playfield_height;

static int snake_len;
static int snake_x[100], snake_y[100];
static int snake_old_tail_x, snake_old_tail_y;

static int fruit_x, fruit_y;

static int dx, dy;
static int prev_dx, prev_dy;
static int score;
static bool game_over;
static bool game_paused;
static int draw_flags;


static void place_fruit(void)
{
    bool done = false;

    while (!done) {
        fruit_x = rand() % playfield_width;
        fruit_y = rand() % playfield_height;
        done = true;


        for (int i = 0; i < snake_len; i++) {
            if (fruit_x == snake_x[i] && fruit_y == snake_y[i]) {
                done = false;
                break;
            }
        }
    }

    draw_flags |= DRAW_FRUIT_PLACEMENT;
}

static void setup(void)
{
    ft_init(0);
    ft_cursor(FT_OFF);

    int screen_width, screen_height;
    ft_screensize(&screen_width, &screen_height);

    playfield_width = __min(40, screen_width - 2);
    playfield_height = __min(screen_height, 22 + INFO_HEIGHT) - 2 - INFO_HEIGHT;

    game_over = false;
    dx = 0;
    dy = 0;
    prev_dx = 0;
    prev_dy = 0;
    score = 0;

    nanotime_from_ms(&game_loop_delay, 140);

    playfield_x = (screen_width - (playfield_width + 2)) / 2;
    playfield_y = 0;

    snake_len = 1;
    snake_x[0] = playfield_width / 2;
    snake_y[0] = playfield_height / 2;

    srand(time(NULL));
    
    place_fruit();

    draw_flags |= DRAW_FULL_FRAME;
}

static void cleanup(void)
{
    ft_cleanup();
}


static void handle_key(const ft_event_t* _Nonnull evt)
{
    switch (evt->data.character.unicode) {
    case 3:     // Ctrl-C
    case 17:    // Ctrl-Q
    case FT_CHAR_ESCAPE:
        game_over = true;
        break;

    case ' ':
        game_paused = !game_paused;
        break;
    }

    if (game_paused || game_over) {
        return;
    }


    switch (evt->data.character.unicode) {
    case 'a':
    case 'A':
    case FT_CHAR_CURSOR_LEFT:
        if (prev_dx != 1) {
            dx = -1;
            dy =  0;
        }
        break;

    case 'd':
    case 'D':
    case FT_CHAR_CURSOR_RIGHT:
        if (prev_dx != -1) {
            dx = 1;
            dy = 0;
        }
        break;

    case 'w':
    case 'W':
    case FT_CHAR_CURSOR_UP:
        if (prev_dy != 1) {
            dx =  0;
            dy = -1;
        }
        break;

    case 's':
    case 'S':
    case FT_CHAR_CURSOR_DOWN:
        if (prev_dy != -1) {
            dx = 0;
            dy = 1;
        }
        break;
    }
}

static void handle_mouse(const ft_event_t* _Nonnull evt)
{
    const int vec_x = evt->data.mouse.x - snake_x[0];
    const int vec_y = evt->data.mouse.y - snake_y[0];

    if (__abs(vec_x) >= __abs(vec_y)) {
        // X axis
        dx = (vec_x < 0) ? -1 : 1;
        dy = 0;
    }
    else {
        // Y axis
        dx = 0;
        dy = (vec_y < 0) ? -1 : 1;
    }
}

static void input(void)
{
    prev_dx = dx;
    prev_dy = dy;

    if (!ft_getevent(FT_ANY, FT_NONBLOCKING, &event)) {
        switch (event.type) {
            case FT_EVT_CHAR:
                handle_key(&event);
                break;

            case FT_EVT_MOUSE_DOWN:
            case FT_EVT_MOUSE_DRAG:
                handle_mouse(&event);
                break;

            default:
                // ignore
                break;
        }
    }
}

static void draw_full_frame(void)
{
    ft_cls();

    // Playfield
    ft_fgcolor(FT_GREEN);
    ft_moveto(playfield_x + 1, playfield_y + 1);
    ft_drawrect(&ft_rectstyle_hflat, playfield_width + 2, playfield_height + 2);

    // Info (limit height to INFO_HEIGHT)
    fiprintf(stdout, "\n\nScore: %d\n\n", score);
    fputs("Press W, A, S, D to move the snake.\n", stdout);
    fputs("Press SPACE to pause/resume the game and ESC to quit.", stdout);

    // Fruit
    ft_fgcolor(FT_RED);
    ft_moveto(fruit_x + playfield_x + 2, fruit_y + playfield_y + 2);
    putc('*', stdout);

    // Snake
    ft_fgcolor(FT_YELLOW);
    ft_moveto(snake_x[0] + playfield_x + 2, snake_y[0] + playfield_y + 2);
    putc('O', stdout);
    for (int i = 1; i < snake_len; i++) {
        ft_moveto(snake_x[i] + playfield_x + 2, snake_y[i] + playfield_y + 2);
        putc('o', stdout);
    }
}

static void draw_delta_frame(int flags)
{
    // +2 in here for (1) adjusting to 1-based coords and (2) skipping over the
    // left/top playfield border

    if ((flags & DRAW_FRUIT_PLACEMENT) != 0) {
        ft_fgcolor(FT_RED);
        ft_moveto(fruit_x + playfield_x + 2, fruit_y + playfield_y + 2);
        putc('*', stdout);
    }


    if ((flags & DRAW_SNAKE_MOVE) != 0) {
        ft_fgcolor(FT_YELLOW);
        ft_moveto(snake_x[0] + playfield_x + 2, snake_y[0] + playfield_y + 2);
        putc('O', stdout);

        if (snake_len > 1) {
            ft_moveto(snake_x[1] + playfield_x + 2, snake_y[1] + playfield_y + 2);
            putc('o', stdout);
        }

        // Note: we keep the last snake segment on the screen if the snake has
        // grown in length. This old segment is the new snake segment for this
        // frame. 
        if ((flags & DRAW_SNAKE_GROWTH) == 0) {
            ft_moveto(snake_old_tail_x + playfield_x + 2, snake_old_tail_y + playfield_y + 2);
            putc(' ', stdout);
        }
    }


    if ((flags & DRAW_SCORE_CHANGE) != 0) {
        ft_fgcolor(FT_GREEN);
        ft_moveto(7 + 1, playfield_height + 4);
        fiprintf(stdout, "%d", score);
    }
}

static void draw(void)
{
    if (game_paused || game_over) {
        return;
    }

    if ((draw_flags & DRAW_FULL_FRAME) != 0) {
        draw_full_frame();
    }
    else if ((draw_flags & (DRAW_SCORE_CHANGE|DRAW_SNAKE_MOVE|DRAW_SNAKE_GROWTH|DRAW_FRUIT_PLACEMENT)) != 0) {
        draw_delta_frame(draw_flags);
    }

    draw_flags = 0;
    ft_flush();
}

static void logic(void)
{
    if (game_paused || game_over || (dx == 0 && dy == 0)) {
        return;
    }


    // Save the old tail of the snake so that the draw_changes() function can
    // erase the tail from the screen
    snake_old_tail_x = snake_x[snake_len - 1];
    snake_old_tail_y = snake_y[snake_len - 1];


    // Make the snake body follow the snake head
    for (int i = snake_len - 1; i >= 1; i--) {
        snake_x[i] = snake_x[i - 1];
        snake_y[i] = snake_y[i - 1];
    }
    

    // Update the snake head location based on user input
    snake_x[0] += dx;
    snake_y[0] += dy;

    draw_flags |= DRAW_SNAKE_MOVE;


    // Snake head hits a wall -> game over
    if (snake_x[0] < 0 || snake_x[0] >= playfield_width || snake_y[0] < 0 || snake_y[0] >= playfield_height) {
        game_over = true;
    }
    

    // Snake head hits snake body -> game over
    const int hx = snake_x[0];
    const int hy = snake_y[0];

    for (int i = 1; i < snake_len; i++) {
        if (hx == snake_x[i] && hy == snake_y[i]) {
            game_over = true;
            break;
        }
    }

    if (game_over) {
        return;
    }


    // Snake head hits fruit -> increase score and grow snake
    if (hx == fruit_x && hy == fruit_y) {
        place_fruit();

        score += 10;
        snake_len++;
        snake_x[snake_len - 1] = snake_x[snake_len - 2];
        snake_y[snake_len - 1] = snake_y[snake_len - 2];

        //draw_flags |= DRAW_SNAKE_GROWTH;  //XXX leaves artifacts on the screen
        draw_flags |= DRAW_SCORE_CHANGE;
    }
}

static void game_loop(void* ctx)
{
    input();
    logic();
    draw();
    
    if (game_over) {
        cleanup();
        exit(0);
    }
}


int main(int argc, char *argv[])
{
    setup();

    dispatch_repeating(dispatch_main_queue(), 0, &NANOTIME_ZERO, &game_loop_delay, (dispatch_async_func_t)game_loop, NULL);
    dispatch_run_main_queue();
    /* NOT REACHED */
    return 0;
}
