

#ifndef SENSOR_BOSCH_BME280_H__
#define SENSOR_BOSCH_BME280_H__

#include "sensor.h"
#include "bme280.h"

#define BME280_ADDR_DEFAULT (BME280_I2C_ADDR_PRIM)

int rt_hw_bme280_init(const char *name, struct rt_sensor_config *cfg);

#endif
