#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>

#define USER_ENTRYPOINT 0x200000
uint64_t load_task_img(int taskid, uintptr_t pgdir);

#endif