#include "shared_memory.h"

static int get_shared_block(char const *filename, int size){
    key_t key;
    
    key = ftok(filename, 0);
    if(key == IPC_RESULT_ERROR)
        return IPC_RESULT_ERROR;
    
    return shmget(key, size, 0644 | IPC_CREAT);
}

char* attach_memory_block(char const * filename, int size){
    int shared_block_id = get_shared_block(filename, size);
    char* result;

    if(shared_block_id == IPC_RESULT_ERROR)
        return NULL;
    
    result = (char*) shmat(shared_block_id, NULL, 0);
    if(result == (char*)IPC_RESULT_ERROR)
        return NULL;

    return result;
}

bool detach_memory_block(char* block){
    return (shmdt(block) != IPC_RESULT_ERROR);
}

bool destroy_memory_block(char* filename){
    int shared_block_id = get_shared_block(filename, 0);

    if(shared_block_id == IPC_RESULT_ERROR)
        return NULL;
    
    return (shmctl(shared_block_id, IPC_RMID, NULL) != IPC_RESULT_ERROR);
}




