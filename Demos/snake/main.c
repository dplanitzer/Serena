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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/timespec.h>
#include "utils.h"


#define PLAYFIELD_WIDTH 40
#define PLAYFIELD_HEIGHT 20

static struct timespec game_loop_delay;

static int playfield_x, playfield_y;

static int snake_head_x, snake_head_y;
static int snake_body_len;
static int snake_body_x[100], snake_body_y[100];

static int fruit_x, fruit_y;

static int key, score;
static bool game_over;

static char buf[1024];
static char playfield_l_edge_buf[4];
static char playfield_width_buf[4];


static void place_fruit(void)
{
    for (;;) {
        fruit_x = rand() % PLAYFIELD_WIDTH;
        fruit_y = rand() % PLAYFIELD_HEIGHT;

        if (fruit_x != snake_head_x && fruit_y != snake_head_y) {
            bool hasMatch = false;

            for (int i = 0; i < snake_body_len; i++) {
                if (fruit_x == snake_body_x[i] && fruit_y == snake_body_y[i]) {
                    hasMatch = true;
                    break;
                }
            }

            if (!hasMatch) {
                break;
            }
        }
    }
}

static void setup(void)
{
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    fcntl(STDIN_FILENO, F_UPDTFL, 1, O_NONBLOCK);
    cursor_on(false);

    game_over = false;
    snake_body_len = 0;
    key = 0;
    score = 0;

    timespec_from_ms(&game_loop_delay, 66);

    playfield_x = (80 - (PLAYFIELD_WIDTH + 2)) / 2;
    playfield_y = 0;

    itoa(playfield_x, playfield_l_edge_buf, 10);
    itoa(PLAYFIELD_WIDTH, playfield_width_buf, 10);

    snake_head_x = PLAYFIELD_WIDTH / 2;
    snake_head_y = PLAYFIELD_HEIGHT / 2;
    
    place_fruit();
}

static void cleanup(void)
{
    cursor_on(true);
    fcntl(STDIN_FILENO, F_UPDTFL, 0, O_NONBLOCK);
}


static void input(void)
{
    switch (getchar()) {
    case 'a':
    case 'A':
        if (key != 2) {
            key = 1;
        }
        break;

    case 'd':
    case 'D':
        if (key != 1) {
            key = 2;
        }
        break;

    case 'w':
    case 'W':
        if (key != 4) {
            key = 3;
        }
        break;

    case 's':
    case 'S':
        if (key != 3) {
            key = 4;
        }
        break;

    case 0x1b:
        game_over = true;
        break;

    case EOF:
        // no key pressed
        clearerr(stdin);
        break;

    default:
        break;
    }
}

static void draw(void)
{
    char* b = buf;

    // Draw the playfield
    b = cls(b);
    b = mv_by_precomp(b, playfield_l_edge_buf);
    b = h_line(b, '-', PLAYFIELD_WIDTH + 2);
    *b++ = '\n';

    for (int i = 0; i < PLAYFIELD_HEIGHT; i++) {
        b = mv_by_precomp(b, playfield_l_edge_buf);
        *b++ = '|';
        b = mv_by_precomp(b, playfield_width_buf);
        *b++ = '|';
        *b++ = '\n';
    }

    b = mv_by_precomp(b, playfield_l_edge_buf);
    b = h_line(b, '-', PLAYFIELD_WIDTH + 2);
    *b++ = '\n';
    *b++ = '\n';

    b = __strcpy(b, "Score: ");
    itoa(score, b, 10);
    b = __strcat(b, "\n\n");
    b = __strcpy(b, "Press W, A, S, D to move the snake.\n");
    b = __strcpy(b, "Press ESC to quit the game.");

    (void)write(STDOUT_FILENO, buf, b - buf);


    // Draw the fruit
    b = buf;
    b = mv_to(b, fruit_x + playfield_x + 1, fruit_y + playfield_y + 1);
    *b++ = '*';


    // Draw the snake
    b = mv_to(b, snake_head_x + playfield_x + 1, snake_head_y + playfield_y + 1);
    *b++ = 'O';
    for (int i = 0; i < snake_body_len; i++) {
        b = mv_to(b, snake_body_x[i] + playfield_x + 1, snake_body_y[i] + playfield_y + 1);
        *b++ = 'o';
    }

    (void)write(STDOUT_FILENO, buf, b - buf);
}

static void logic(void)
{
    // Make the snake body follow the snake head
    int prevX = snake_body_x[0];
    int prevY = snake_body_y[0];
    int prev2X, prev2Y;
    snake_body_x[0] = snake_head_x;
    snake_body_y[0] = snake_head_y;
    for (int i = 1; i < snake_body_len; i++) {
        prev2X = snake_body_x[i];
        prev2Y = snake_body_y[i];
        snake_body_x[i] = prevX;
        snake_body_y[i] = prevY;
        prevX = prev2X;
        prevY = prev2Y;
    }
    

    // Update snake location based on keyboard input
    switch (key) {
    case 1:
        snake_head_x--;
        break;

    case 2:
        snake_head_x++;
        break;

    case 3:
        snake_head_y--;
        break;

    case 4:
        snake_head_y++;
        break;

    default:
        break;
    }


    // Snake hitting walls -> game over
    if (snake_head_x < 0 || snake_head_x >= PLAYFIELD_WIDTH || snake_head_y < 0 || snake_head_y >= PLAYFIELD_HEIGHT) {
        game_over = true;
    }
    

    // Snake hitting itself -> game over
    for (int i = 0; i < snake_body_len; i++) {
        if (snake_body_x[i] == snake_head_x && snake_body_y[i] == snake_head_y) {
            game_over = true;
            break;
        }
    }


    // Snake head hits fruit -> increase score and grow snake
    if (snake_head_x == fruit_x && snake_head_y == fruit_y) {
        place_fruit();

        score += 10;
        snake_body_len++;
    }
}

static void game_loop(void* ctx)
{
    if (game_over) {
        cleanup();
        exit(0);
    }

    draw();
    input();
    logic();
}


int main(int argc, char *argv[])
{
    setup();

    dispatch_repeating(dispatch_main_queue(), 0, &TIMESPEC_ZERO, &game_loop_delay, (dispatch_async_func_t)game_loop, NULL);
    dispatch_run_main_queue();
    /* NOT REACHED */
    return 0;
}
