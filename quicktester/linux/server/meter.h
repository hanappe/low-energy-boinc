#ifndef _METER_H
#define _METER_H

typedef struct meter_s meter_t;

extern int meters_init();
extern int meters_fini();
extern int meters_count();
extern meter_t* meters_get(int meter_id);
extern float meter_get_energy(meter_t* meter);

#endif
