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

#define SHELL_BEGIN 7

int main(void)
{
    const char *prompt = "> root@UCAS_OS: ";
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    printf("%s", prompt);

    while (1)
    {
        char line[128];
        int pos = 0;
        memset(line, 0, sizeof(line));

        while (1) {
            int ch = sys_getchar();
            if (ch <= 0) continue;

            /* ignore arrow-key escape sequence: ESC [ A/B/C/D */
            if (ch == 27) {
                int c1 = sys_getchar();
                if (c1 == '[') {
                    (void)sys_getchar();
                }
                continue;
            }

            if (ch == '\r' || ch == '\n') {
                printf("\n");
                break;
            }
            if (ch == 8 || ch == 127 || ch == 263) { // backspace / delete
                if (pos > 0) {
                    pos--;
                    line[pos] = '\0';
                    printf("\b");
                }
                continue;
            }

            if (ch < 32 || ch > 126) {
                continue;
            }

            if (pos < (int)sizeof(line) - 1) {
                line[pos++] = (char)ch;
                line[pos] = '\0';
                printf("%c", ch);
            }
        }

        /* parse command line into argv-style tokens */
        char *argv_exec[8];
        int argc_exec = 0;
        char *scan = line;
        while (*scan) {
            while (*scan && isspace((unsigned char)*scan)) {
                *scan = '\0';
                scan++;
            }
            if (!*scan)
                break;
            if (argc_exec < (int)(sizeof(argv_exec) / sizeof(argv_exec[0]))) {
                argv_exec[argc_exec++] = scan;
            }
            while (*scan && !isspace((unsigned char)*scan)) {
                scan++;
            }
        }

        if (argc_exec == 0) {
            printf("%s", prompt);
            continue;
        }
        if (strcmp(argv_exec[0], "ps") == 0) {
            sys_ps();
        } else if (strcmp(argv_exec[0], "clear") == 0) {
            sys_clear();
        } else if (strcmp(argv_exec[0], "help") == 0) {
            printf("Available: ps exec <name> [args...] kill <pid> clear help\n");
        } else if (strcmp(argv_exec[0], "exec") == 0) {
            if (argc_exec >= 2) {
                pid_t pid = sys_exec(argv_exec[1], argc_exec - 1, &argv_exec[1]);
                printf("Info: execute %s successfully, pid = %d ...\n", argv_exec[1], pid);
            } else {
                printf("usage: exec <name> [args...]\n");
            }
        } else if (strcmp(argv_exec[0], "kill") == 0) {
            if (argc_exec >= 2) {
                int pid = atoi(argv_exec[1]);
                int r = sys_kill(pid);
                printf("> kill %d -> %d\n", pid, r);
            } else {
                printf("usage: kill <pid>\n");
            }
        } else {
            printf("Error: Unknown Command %s!\n", argv_exec[0]);
        }

        printf("%s", prompt);

        /************************************************************/
        /* Do not touch this comment. Reserved for future projects. */
        /************************************************************/    
    }

    return 0;
}
