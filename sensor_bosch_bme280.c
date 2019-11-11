
#include "sensor_bosch_bme280.h"

#define DBG_ENABLE
#define DBG_LEVEL DBG_LOG
#define DBG_SECTION_NAME  "sensor.bosch.bme280"
#define DBG_COLOR
#include <rtdbg.h>

#define ODR_125HZ   BME280_OVERSAMPLING_1X
#define ODR_71HZ    BME280_OVERSAMPLING_2X
#define ODR_38HZ    BME280_OVERSAMPLING_4X
#define ODR_20HZ    BME280_OVERSAMPLING_8X
#define ODR_10HZ    BME280_OVERSAMPLING_16X

static struct bme280_dev _bme280_dev;
static struct rt_i2c_bus_device *i2c_bus_dev;

static void rt_delay_ms(uint32_t period)
{
    rt_thread_mdelay(period);
}

static int8_t rt_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    rt_uint8_t tmp = reg;
    struct rt_i2c_msg msgs[2];

    msgs[0].addr  = addr;             /* Slave address */
    msgs[0].flags = RT_I2C_WR;        /* Write flag */
    msgs[0].buf   = &tmp;             /* Slave register address */
    msgs[0].len   = 1;                /* Number of bytes sent */

    msgs[1].addr  = addr;             /* Slave address */
    msgs[1].flags = RT_I2C_WR | RT_I2C_NO_START;        /* Read flag */
    msgs[1].buf   = data;             /* Read data pointer */
    msgs[1].len   = len;              /* Number of bytes read */

    if (rt_i2c_transfer(i2c_bus_dev, msgs, 2) != 2)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}

static int8_t rt_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    rt_uint8_t tmp = reg;
    struct rt_i2c_msg msgs[2];

    msgs[0].addr  = addr;             /* Slave address */
    msgs[0].flags = RT_I2C_WR;        /* Write flag */
    msgs[0].buf   = &tmp;             /* Slave register address */
    msgs[0].len   = 1;                /* Number of bytes sent */

    msgs[1].addr  = addr;             /* Slave address */
    msgs[1].flags = RT_I2C_RD;        /* Read flag */
    msgs[1].buf   = data;             /* Read data pointer */
    msgs[1].len   = len;              /* Number of bytes read */

    if (rt_i2c_transfer(i2c_bus_dev, msgs, 2) != 2)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t _bme280_init(struct rt_sensor_intf *intf)
{
    int8_t rslt = BME280_OK;

    _bme280_dev.dev_id = (rt_uint32_t)(intf->user_data) & 0xff;
    _bme280_dev.intf = BME280_I2C_INTF;
    _bme280_dev.read = rt_i2c_read_reg;
    _bme280_dev.write = rt_i2c_write_reg;
    _bme280_dev.delay_ms = rt_delay_ms;

    i2c_bus_dev = (struct rt_i2c_bus_device *)rt_device_find(intf->dev_name);
    if (i2c_bus_dev == RT_NULL)
    {
        LOG_E("can not find device %s", intf->dev_name);
        return -RT_ERROR;
    }

    rslt = bme280_init(&_bme280_dev);
    if (rslt == BME280_OK)
    {
        uint8_t settings_sel;

        /* Recommended mode of operation: Indoor navigation */
        _bme280_dev.settings.osr_h = BME280_OVERSAMPLING_16X;
        _bme280_dev.settings.osr_p = BME280_OVERSAMPLING_16X;
        _bme280_dev.settings.osr_t = BME280_OVERSAMPLING_16X;
        _bme280_dev.settings.filter = BME280_FILTER_COEFF_16;

        settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;

        rslt = bme280_set_sensor_settings(settings_sel, &_bme280_dev);

        return RT_EOK;
    }
    else
    {
        LOG_E("bme280 init failed");
        return -RT_ERROR;
    }
}

static rt_err_t _bme280_set_odr(rt_sensor_t sensor, rt_uint16_t odr)
{
    uint8_t odr_ctr;

    if(odr < 10) 
        odr_ctr = ODR_10HZ;
    else if(odr < 20)
        odr_ctr = ODR_20HZ;
    else if(odr < 38)
        odr_ctr = ODR_38HZ;
    else if (odr < 71)
        odr_ctr = ODR_71HZ;
    else
        odr_ctr = ODR_125HZ;
            
    if (sensor->info.type == RT_SENSOR_CLASS_BARO)
    {
        _bme280_dev.settings.osr_p = odr_ctr;
        
        if(bme280_set_sensor_settings(BME280_OSR_PRESS_SEL, &_bme280_dev) == 0)
            return RT_EOK;
    }
    else if (sensor->info.type == RT_SENSOR_CLASS_TEMP)
    {
        _bme280_dev.settings.osr_t = odr_ctr;
        
        if(bme280_set_sensor_settings(BME280_OSR_TEMP_SEL, &_bme280_dev) == 0)
            return RT_EOK;
    }
    else if (sensor->info.type == RT_SENSOR_CLASS_HUMI)
    {
        _bme280_dev.settings.osr_h = odr_ctr;
        
        if(bme280_set_sensor_settings(BME280_OSR_HUM_SEL, &_bme280_dev) == 0)
            return RT_EOK;
    }
    
    return RT_EOK;
}

static rt_err_t _bme280_set_power(rt_sensor_t sensor, rt_uint8_t power)
{
    int8_t rslt = 0;

    if (power == RT_SENSOR_POWER_DOWN)
    {
        rslt = bme280_set_sensor_mode(BME280_SLEEP_MODE, &_bme280_dev);
    }
    else if (power == RT_SENSOR_POWER_NORMAL)
    {
        rslt = bme280_set_sensor_mode(BME280_NORMAL_MODE, &_bme280_dev);
    }
    else
    {
        LOG_W("Unsupported mode, code is %d", power);
        return -RT_ERROR;
    }
    return rslt;
}

static rt_size_t bme280_fetch_data(struct rt_sensor_device *sensor, void *buf, rt_size_t len)
{
    struct bme280_data comp_data;
    struct rt_sensor_data *data = buf;
        
    if (sensor->info.type == RT_SENSOR_CLASS_BARO)
    {
        bme280_get_sensor_data(BME280_PRESS, &comp_data, &_bme280_dev);

        data->type = RT_SENSOR_CLASS_BARO;
        data->data.baro = comp_data.pressure;
        data->timestamp = rt_sensor_get_ts();
    }
    else if (sensor->info.type == RT_SENSOR_CLASS_TEMP)
    {
        bme280_get_sensor_data(BME280_TEMP, &comp_data, &_bme280_dev);

        data->type = RT_SENSOR_CLASS_TEMP;
        data->data.temp = comp_data.temperature / 10;
        data->timestamp = rt_sensor_get_ts();
    }
    else if (sensor->info.type == RT_SENSOR_CLASS_HUMI)
    {
        bme280_get_sensor_data(BME280_HUM, &comp_data, &_bme280_dev);

        data->type = RT_SENSOR_CLASS_HUMI;
        data->data.humi = comp_data.humidity / 100;
        data->timestamp = rt_sensor_get_ts();
    }
    return 1;
}

static rt_err_t bme280_control(struct rt_sensor_device *sensor, int cmd, void *args)
{
    rt_err_t result = RT_EOK;

    switch (cmd)
    {
    case RT_SENSOR_CTRL_GET_ID:
        *(rt_uint8_t *)args = _bme280_dev.chip_id;
        break;
    case RT_SENSOR_CTRL_SET_ODR:
         result = _bme280_set_odr(sensor, (rt_uint32_t)args & 0xffff);
        break;
    case RT_SENSOR_CTRL_SET_POWER:
        result = _bme280_set_power(sensor, (rt_uint32_t)args & 0xff);
        break;
    case RT_SENSOR_CTRL_SELF_TEST:
        /* TODO */
        result = -RT_EINVAL;
        break;
    default:
        return -RT_EINVAL;
    }
    return result;
}

static struct rt_sensor_ops sensor_ops =
{
    bme280_fetch_data,
    bme280_control
};

int rt_hw_bme280_init(const char *name, struct rt_sensor_config *cfg)
{
    rt_int8_t result;
    rt_sensor_t sensor_baro = RT_NULL, sensor_temp = RT_NULL, sensor_humi = RT_NULL;
    struct rt_sensor_module *module = RT_NULL;

    if (_bme280_init(&cfg->intf) != RT_EOK)
    {
        return RT_ERROR;
    }
    
    module = rt_calloc(1, sizeof(struct rt_sensor_module));
    if (module == RT_NULL)
    {
        return -1;
    }

    /*  barometric pressure sensor register */
    {
        sensor_baro = rt_calloc(1, sizeof(struct rt_sensor_device));
        if (sensor_baro == RT_NULL)
            goto __exit;

        sensor_baro->info.type       = RT_SENSOR_CLASS_BARO;
        sensor_baro->info.vendor     = RT_SENSOR_VENDOR_BOSCH;
        sensor_baro->info.model      = "bme280_baro";
        sensor_baro->info.unit       = RT_SENSOR_UNIT_PA;
        sensor_baro->info.intf_type  = RT_SENSOR_INTF_I2C;
        sensor_baro->info.range_max  = 110000;
        sensor_baro->info.range_min  = 30000;
        sensor_baro->info.period_min = 100;

        rt_memcpy(&sensor_baro->config, cfg, sizeof(struct rt_sensor_config));
        sensor_baro->ops = &sensor_ops;
        sensor_baro->module = module;
        module->sen[0] = sensor_baro;
        module->sen_num++;
        
        result = rt_hw_sensor_register(sensor_baro, name, RT_DEVICE_FLAG_RDWR, RT_NULL);
        if (result != RT_EOK)
        {
            LOG_E("device register err code: %d", result);
            goto __exit;
        }
    }
    /* temperature sensor register */
    {
        sensor_temp = rt_calloc(1, sizeof(struct rt_sensor_device));
        if (sensor_temp == RT_NULL)
            goto __exit;

        sensor_temp->info.type       = RT_SENSOR_CLASS_TEMP;
        sensor_temp->info.vendor     = RT_SENSOR_VENDOR_BOSCH;
        sensor_temp->info.model      = "bme280_temp";
        sensor_temp->info.unit       = RT_SENSOR_UNIT_DCELSIUS;
        sensor_temp->info.intf_type  = RT_SENSOR_INTF_I2C;
        sensor_temp->info.range_max  = 850;
        sensor_temp->info.range_min  = -400;
        sensor_temp->info.period_min = 100;

        rt_memcpy(&sensor_temp->config, cfg, sizeof(struct rt_sensor_config));
        sensor_temp->ops = &sensor_ops;
        sensor_temp->module = module;
        module->sen[1] = sensor_temp;
        module->sen_num++;
        
        result = rt_hw_sensor_register(sensor_temp, name, RT_DEVICE_FLAG_RDWR, RT_NULL);
        if (result != RT_EOK)
        {
            LOG_E("device register err code: %d", result);
            goto __exit;
        }
    }
    /* humidity sensor register */
    {
        sensor_humi = rt_calloc(1, sizeof(struct rt_sensor_device));
        if (sensor_humi == RT_NULL)
            goto __exit;

        sensor_humi->info.type       = RT_SENSOR_CLASS_HUMI;
        sensor_humi->info.vendor     = RT_SENSOR_VENDOR_BOSCH;
        sensor_humi->info.model      = "bme280_humi";
        sensor_humi->info.unit       = RT_SENSOR_UNIT_PERMILLAGE;
        sensor_humi->info.intf_type  = RT_SENSOR_INTF_I2C;
        sensor_humi->info.range_max  = 1000;
        sensor_humi->info.range_min  = 0;
        sensor_humi->info.period_min = 100;

        rt_memcpy(&sensor_humi->config, cfg, sizeof(struct rt_sensor_config));
        sensor_humi->ops = &sensor_ops;
        sensor_humi->module = module;
        module->sen[2] = sensor_humi;
        module->sen_num++;
        
        result = rt_hw_sensor_register(sensor_humi, name, RT_DEVICE_FLAG_RDWR, RT_NULL);
        if (result != RT_EOK)
        {
            LOG_E("device register err code: %d", result);
            goto __exit;
        }
    }

    LOG_I("sensor init success");
    return RT_EOK;
    
__exit:
    if(sensor_baro)
        rt_free(sensor_baro);
    if(sensor_temp)
        rt_free(sensor_temp);
    if(sensor_humi)
        rt_free(sensor_humi);
    if (module)
        rt_free(module);
    return -RT_ERROR;
}
