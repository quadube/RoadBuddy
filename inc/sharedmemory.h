#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define IPC_RESULT_ERROR (-1)

char* attach_memory_block(char const *filename, int size);
bool detach_memory_block(char *block);
bool destroy_memory_block(char const *filename);

#endif
