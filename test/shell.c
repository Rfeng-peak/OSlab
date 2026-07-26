/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stddef.h>

#define SHELL_BEGIN 5

int main(void)
{
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    printf("> root@UCAS_OS: ");

    while (1)
    {
        char line[128];
        int pos = 0;
        memset(line, 0, sizeof(line));
        while (1) {
            int ch = sys_getchar();
            if (ch <= 0) continue;
            if (ch == '\r' || ch == '\n') {
                printf("\n");
                break;
            }
            if (ch == 8 || ch == 127) { // backspace
                if (pos > 0) {
                    pos--;
                    printf("\b \b");
                }
                continue;
            }
            if (pos < (int)sizeof(line) - 1) {
                line[pos++] = (char)ch;
                printf("%c", ch);
            }
        }

        /* parse command without strtok (tiny libc may not provide it) */
        char cmdbuf[32];
        char argbuf[64];
        char *p = line;
        while (*p && isspace(*p)) p++;
        int i = 0;
        while (*p && !isspace(*p) && i < (int)sizeof(cmdbuf)-1) cmdbuf[i++] = *p++;
        cmdbuf[i] = '\0';
        while (*p && isspace(*p)) p++;
        i = 0;
        while (*p && !isspace(*p) && i < (int)sizeof(argbuf)-1) argbuf[i++] = *p++;
        argbuf[i] = '\0';

        if (cmdbuf[0] == '\0') {
            printf("> root@UCAS_OS: ");
            continue;
        }
        if (strcmp(cmdbuf, "ps") == 0) {
            sys_ps();
        } else if (strcmp(cmdbuf, "clear") == 0) {
            sys_clear(SHELL_BEGIN + 1);
        } else if (strcmp(cmdbuf, "help") == 0) {
            printf("Available: ps exec <id> kill <pid> clear help\n");
        } else if (strcmp(cmdbuf, "exec") == 0) {
            if (argbuf[0]) {
                int id = atoi(argbuf);
                pid_t pid;
                if (id == 0 && argbuf[0] != '0') {
                    /* pass name pointer to kernel for lookup */
                    pid = sys_exec(argbuf, 0, 0, 0, 0);
                } else {
                    pid = sys_exec((void *)(long)id, 0, 0, 0, 0);
                }
                printf("> exec pid=%d\n", pid);
            } else {
                printf("usage: exec <id|name>\n");
            }
        } else if (strcmp(cmdbuf, "kill") == 0) {
            if (argbuf[0]) {
                int pid = atoi(argbuf);
                int r = sys_kill(pid);
                printf("> kill %d -> %d\n", pid, r);
            } else {
                printf("usage: kill <pid>\n");
            }
        } else {
            printf("Unknown command: %s\n", cmdbuf);
        }

        printf("> root@UCAS_OS: ");

        /************************************************************/
        /* Do not touch this comment. Reserved for future projects. */
        /************************************************************/    
    }

    return 0;
}
