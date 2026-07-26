#include <stdio.h>
#include <unistd.h>

static char blank[] = {"                                                "};

int main(void)
{
    int print_location = 5;
    int sleep_time = 5;

    while (1)
    {
        for (int i = 0; i < 10; i++)
        {
            sys_move_cursor(0, print_location);
            printf("> [TASK] sleep1 test running. (%d)\n", i);
            sys_sleep(1);
        }

        sys_move_cursor(0, print_location);
        printf("> [TASK] sleep1 is sleeping, sleep time is %d.\n", sleep_time);

        sys_sleep(sleep_time);

        sys_move_cursor(0, print_location);
        printf("%s", blank);
    }
}
