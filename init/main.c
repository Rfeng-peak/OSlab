#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);

    // [p1-task3] load task by id and execute it.
    while (1)
    {
        bios_putstr("[task3] Input task id (0-3): ");

        int ch = 0;
        while (1)
        {
            ch = bios_getchar();

            // Ignore line-ending chars left by previous command input.
            if (ch == '\r' || ch == '\n')
            {
                continue;
            }

            if (ch >= '0' && ch <= '3')
            {
                bios_putchar(ch);
                bios_putstr("\n\r");
                break;
            }

            // Ignore empty/no-input reads or non-printable bytes from serial noise.
            if (ch < 32 || ch > 126)
            {
                continue;
            }

            bios_putstr("\n\r[task3] invalid input, please type 0-3: ");
        }

        int taskid = ch - '0';
        uint64_t entrypoint = load_task_img(taskid);

        if (entrypoint == 0) {
            bios_putstr("[task3] invalid task id\n\r");
        } else {
            bios_putstr("[task3] loading task...\n\r");
            ((int (*)(void))entrypoint)();
            bios_putstr("[task3] task finished\n\r");
        }
    }

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
