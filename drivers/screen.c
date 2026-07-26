#include <screen.h>
#include <printk.h>
#include <os/string.h>
#include <os/sched.h>
#include <os/irq.h>
#include <os/kernel.h>

#define SCREEN_WIDTH    80
#define SCREEN_HEIGHT   24
#define SCREEN_LOC(x, y) ((y) * SCREEN_WIDTH + (x))

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
/* write a char */
void screen_write_ch(char ch)
{
    if (ch == '\n')
    {
        current_running->cursor_x = 0;
        if (current_running->cursor_y < SCREEN_HEIGHT - 1)
            current_running->cursor_y++;
    }
    else if (ch == '\b' || ch == '\177')
    {
        if (current_running->cursor_x > 0)
            current_running->cursor_x--;
    }
    else
    {
        if (current_running->cursor_y >= SCREEN_HEIGHT)
            current_running->cursor_y = SCREEN_HEIGHT - 1;
        new_screen[SCREEN_LOC(current_running->cursor_x, current_running->cursor_y)] = ch;
        if (++current_running->cursor_x >= SCREEN_WIDTH)
        {
            current_running->cursor_x = 0;
            if (current_running->cursor_y < SCREEN_HEIGHT - 1)
                current_running->cursor_y++;
        }
    }
}



void init_screen(void)
{
    vt100_hidden_cursor();
    vt100_clear();
    screen_clear(0);
}

long screen_clear(int start_row)
{
    int i, j;
    vt100_move_cursor(1, start_row + 1);
    printv("%c[J", 27);
    for (i = start_row; i < SCREEN_HEIGHT; i++)
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            new_screen[SCREEN_LOC(j, i)] = ' ';
			old_screen[SCREEN_LOC(j, i)] = ' ';
        }
    }
    current_running->cursor_x = 0;
    current_running->cursor_y = start_row;
    return 0;
}

void screen_move_cursor(int x, int y)
{
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
}



void screen_write(char *buff)
{
    int i = 0;
    int l = strlen(buff);

    for (i = 0; i < l; i++)
    {
        screen_write_ch(buff[i]);
    }
}

/*
 * This function is used to print the serial port when the clock
 * interrupt is triggered. However, we need to pay attention to
 * the fact that in order to speed up printing, we only refresh
 * the characters that have been modified since this time.
 */
void screen_reflush(void)
{
    int i, j;

    for (i = 0; i < SCREEN_HEIGHT; i++)
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            if (new_screen[SCREEN_LOC(j, i)] != old_screen[SCREEN_LOC(j, i)])
            {
                bios_putchar(27);
                bios_putchar('[');
                if (i + 1 >= 10)
                    bios_putchar('0' + (i + 1) / 10);
                bios_putchar('0' + (i + 1) % 10);
                bios_putchar(';');
                if (j + 1 >= 10)
                    bios_putchar('0' + (j + 1) / 10);
                bios_putchar('0' + (j + 1) % 10);
                bios_putchar('H');
                bios_putchar(new_screen[SCREEN_LOC(j, i)]);
                old_screen[SCREEN_LOC(j, i)] = new_screen[SCREEN_LOC(j, i)];
            }
        }
    }

    bios_putchar(27);
    bios_putchar('[');
    if (current_running->cursor_y + 1 >= 10)
        bios_putchar('0' + (current_running->cursor_y + 1) / 10);
    bios_putchar('0' + (current_running->cursor_y + 1) % 10);
    bios_putchar(';');
    if (current_running->cursor_x + 1 >= 10)
        bios_putchar('0' + (current_running->cursor_x + 1) / 10);
    bios_putchar('0' + (current_running->cursor_x + 1) % 10);
    bios_putchar('H');
}
