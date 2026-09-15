/* src/gpu.h */
#ifndef GPU_H
#define GPU_H

struct gpu_status {
    int is_available;
    int fd;
    int major_version;
    int minor_version;
};

extern struct gpu_status g_gpu;

int  gpu_init(void);
void gpu_cleanup(void);
void gpu_kick_keepalive(void);

#endif
