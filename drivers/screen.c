#include <screen.h>
#include <printk.h>
#include <os/string.h>
#include <os/sched.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/smp.h>

#define SCREEN_WIDTH    80
#define SCREEN_HEIGHT   50
#define SCREEN_LOC(x, y) ((y) * SCREEN_WIDTH + (x))
/* Keep the shell's split-screen layout intact: row 7 is the divider,
 * and the interactive command area starts on the next row. */
#define SCREEN_COMMAND_BEGIN 8

/* screen buffer */
char new_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = {0};
char old_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = {0};

/* cursor position */
static void vt100_move_cursor(int x, int y)
{
    // \033[y;xH
    printv("%c[%d;%dH", 27, y, x);
}

/* clear screen */
static void vt100_clear()
{
    // \033[2J
    printv("%c[2J", 27);
}

/* hidden cursor */
static void vt100_hidden_cursor()
{
    // \033[?25l
    printv("%c[?25l", 27);
}

/* write a char */
void screen_write_ch(char ch)
{
    if (ch == '\n')
    {
        current_running->cursor_x = 0;
        current_running->cursor_y++;
    }
    else if (ch == '\b' || ch == '\177')
    {
        if (current_running->cursor_x > 0)
        {
            current_running->cursor_x--;
        }
        else if (current_running->cursor_y > 0)
        {
            current_running->cursor_y--;
            current_running->cursor_x = SCREEN_WIDTH - 1;
        }
        else
        {
            return;
        }

        new_screen[SCREEN_LOC(current_running->cursor_x, current_running->cursor_y)] = ' ';
    }
    else
    {
        new_screen[SCREEN_LOC(current_running->cursor_x, current_running->cursor_y)] = ch;
        if (++current_running->cursor_x >= SCREEN_WIDTH)
        {
            current_running->cursor_x = 0;
            current_running->cursor_y++;
        }
    }

    /* scroll when cursor goes past bottom */
    if (current_running->cursor_y >= SCREEN_HEIGHT) {
        int row, col;
        /*
         * Scroll the physical terminal: VT100 SU (\033[S) scrolls the
         * display up by one line regardless of terminal height.
         */
        printv("%c[S", 27);

        /* Shift screen buffers up by one row. */
        for (row = 1; row < SCREEN_HEIGHT; row++)
            for (col = 0; col < SCREEN_WIDTH; col++) {
                new_screen[SCREEN_LOC(col, row - 1)] = new_screen[SCREEN_LOC(col, row)];
                old_screen[SCREEN_LOC(col, row - 1)] = old_screen[SCREEN_LOC(col, row)];
            }
        /* Clear bottom row in both buffers. */
        for (col = 0; col < SCREEN_WIDTH; col++) {
            new_screen[SCREEN_LOC(col, SCREEN_HEIGHT - 1)] = ' ';
            old_screen[SCREEN_LOC(col, SCREEN_HEIGHT - 1)] = ' ';
        }

        /*
         * The global screen scrolled, so every active process sees its
         * content shifted up by one row.  Decrement their cursor_y
         * (unless already at the top) so future output lands at the
         * correct position.
         */
        for (int i = 0; i < NUM_MAX_TASK; i++) {
            if (pcb[i].pid != 0 && pcb[i].status != TASK_EXITED) {
                if (pcb[i].cursor_y > 0)
                    pcb[i].cursor_y--;
            }
        }
        current_running->cursor_y = SCREEN_HEIGHT - 1;
    }
}

void init_screen(void)
{
    vt100_hidden_cursor();
    vt100_clear();
    screen_clear();
}

void screen_clear(void)
{
    lock_kernel();
    int i, j;
    for (i = SCREEN_COMMAND_BEGIN; i < SCREEN_HEIGHT; i++)
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            new_screen[SCREEN_LOC(j, i)] = ' ';
        }
    }
    current_running->cursor_x = 0;
    current_running->cursor_y = SCREEN_COMMAND_BEGIN;
    unlock_kernel();
    screen_reflush();
}

void screen_move_cursor(int x, int y)
{
    lock_kernel();
    if (x >= SCREEN_WIDTH)
        x = SCREEN_WIDTH - 1;
    else if (x < 0)
        x = 0;
    if (y >= SCREEN_HEIGHT)
        y = SCREEN_HEIGHT - 1;
    else if (y < 0)
        y = 0;
    current_running->cursor_x = x;
    current_running->cursor_y = y;
    vt100_move_cursor(x + 1, y + 1);
    unlock_kernel();
}

void screen_write(char *buff)
{
    int i = 0;
    int l = strlen(buff);

    lock_kernel();
    for (i = 0; i < l; i++)
    {
        screen_write_ch(buff[i]);
    }
    unlock_kernel();
}

void screen_reflush(void)
{
    int i, j;

    lock_kernel();
    for (i = 0; i < SCREEN_HEIGHT; i++)
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            if (new_screen[SCREEN_LOC(j, i)] != old_screen[SCREEN_LOC(j, i)])
            {
                vt100_move_cursor(j + 1, i + 1);
                bios_putchar(new_screen[SCREEN_LOC(j, i)]);
                old_screen[SCREEN_LOC(j, i)] = new_screen[SCREEN_LOC(j, i)];
            }
        }
    }

    vt100_move_cursor(current_running->cursor_x + 1, current_running->cursor_y + 1);
    unlock_kernel();
}
