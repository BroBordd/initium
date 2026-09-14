/* hardware.h */
#ifndef HARDWARE_H
#define HARDWARE_H

void init_power_and_signals(void);
void init_watchdog(void);
void kick_watchdog(void);
void trigger_vibration(void);
int  mknod_fb0(void);
void setup_input_devnodes(void);
unsigned long get_heartbeat_counter(void);

#endif
