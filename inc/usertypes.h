#ifndef USERTYPES_H
#define USERTYPES_H

#include "sharedmemory.h"
#include "cusertimer.h"
#include "cvideocam.h"
#include "cfmu.h"
#include "cfpu.h"

#define SIG_ENG_OFF (SIGRTMIN+1)
#define SIG_UNMOUNT (SIGRTMIN+2)
#define SIG_WARN_SYS (SIGRTMIN+3)
#define SIG_PROBUT (SIGRTMIN+4)
#define SIG_BUZZBUT (SIGRTMIN+5)
#define SIG_ENG_ON (SIGRTMIN+6)
#define SIG_MOUNT (SIGRTMIN+7)

#define BLOCK_SIZE 64
#define SEM_PD_PRODUCER_FNAME "/daemon_sem"
#define FILENAME_PD "/etc/roadbuddy/daemons/dPendriveManager"

#define SEM_CS_PRODUCER_FNAME "/sem_prodcs"
#define SEM_CS_CONSUMER_FNAME "/sem_conscs"
#define FILENAME_CS "/etc/roadbuddy/daemons/dGetCarStatus"

#define handle_error_en(en, msg) \
    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#endif

struct obj_t
{
    CCam* cam;
    CVideo* vid;
    CFmu* fmu;
    CFpu* fpu;
};
