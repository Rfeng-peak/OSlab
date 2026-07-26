#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#define MOD 1000007

struct TestMultiCoreArg
{
    int print_location;
    int from;
    int to;
    int* result;
};

int main(int argc, char * argv[])
{
#ifndef S_CORE
    if (argc < 2)
    {
        printf("Error: argc = %d\n", argc);
    }

    static struct TestMultiCoreArg fallback_args;
    static int fallback_result = 0;
    fallback_args.print_location = 1;
    fallback_args.from = 0;
    fallback_args.to = 10;
    fallback_args.result = &fallback_result;

    struct TestMultiCoreArg *args = (argc >= 2)
        ? (struct TestMultiCoreArg *)(atol(argv[1]))
        : &fallback_args;

    int print_location = args->print_location;
    int from  = args->from;
    int to = args->to;
    int result = 0;

    sys_move_cursor(0, print_location);

    printf("start compute, from = %d, to = %d  ", from, to);
    for (int i = from; i < to; ++i) {
        result = (result + i) % MOD;
    }

    if (args->result) {
        *(args->result) = result;
    }

    printf("Done \n\r");
#endif

    sys_exit();
}