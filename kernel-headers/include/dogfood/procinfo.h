#pragma once

#include "types.h"

#define PROCINFO_MAX_NAME_LEN 48

#define PROCINFO_STATE_UNKNOWN   '?'
#define PROCINFO_STATE_CONSTRUCT 'C'
#define PROCINFO_STATE_RUNNABLE  'r'
#define PROCINFO_STATE_RUNNING   'R'
#define PROCINFO_STATE_ZOMBIE    'Z'
#define PROCINFO_STATE_SLEEPING  'S'

struct PROCINFO {
    int next_pid; // 0 if this is the last one
    char state;
    char name[PROCINFO_MAX_NAME_LEN];
};
