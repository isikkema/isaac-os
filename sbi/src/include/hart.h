#pragma once

#define NUM_HARTS (8)


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
    HartStatus status;
    HartPriviledgeMode priv;
    unsigned long target_address;
} HartData;


extern HartData sbi_hart_data[NUM_HARTS];


HartStatus get_hart_status(unsigned int hart);
int hart_start(unsigned int hart, unsigned long target, int priv_mode);
