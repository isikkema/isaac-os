#pragma once


#include <lock.h>


#define NUM_HARTS (8)

#define IS_VALID_HART(hart) (hart >= 0 && hart < NUM_HARTS)


typedef enum HartStatus {
    HS_INVALID,
    HS_STOPPED,
    HS_STOPPING,
    HS_STARTING,
    HS_STARTED
} HartStatus;

typedef enum HartPriviledgeMode {
    HPM_USER,
    HPM_SUPERVISOR,
    HPM_HYPERVISOR,
    HPM_MACHINE
} HartPriviledgeMode;

typedef struct HartData {
    Mutex lock;
    HartStatus status;
    HartPriviledgeMode priv;
    unsigned long target_address;
} HartData;


extern HartData sbi_hart_data[NUM_HARTS];


HartStatus get_hart_status(int hart);
int hart_start(int hart, unsigned long target, int priv_mode);
int hart_stop(int hart);
void hart_handle_msip(int hart);
