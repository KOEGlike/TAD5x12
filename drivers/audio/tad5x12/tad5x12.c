#define DT_DRV_COMPAT ti_tad5x12

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/audio/codec.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "tad5x12.h"

LOG_MODULE_REGISTER(tad5x12);

struct tad5x12_config
{
    struct i2c_dt_spec i2c;
};

#define tad5x12_write(_i2c, _reg, _value) \
    tad5x12_write_masked(_i2c, _reg, _value, 0xff)

#define tad5x12_set_page(_i2c, _page) \
    tad5x12_write(_i2c, TAD5X12_REG_PAGE_CFG, _page)

#define tad5x12_sw_reset(_i2c) \
    tad5x12_write_masked(_i2c, TAD5X12_REG_SW_RESET, 1, BIT(0))

static inline int tad5x12_write_masked(const struct i2c_dt_spec *i2c, uint8_t reg, uint8_t value, uint8_t mask)
{
    int ret;
    uint8_t actual_value = 0;

    if (mask != 0xff)
    {
        ret = i2c_burst_read_dt(i2c, reg, &actual_value, 1);
        if (ret)
        {
            LOG_ERR("Unable to get actual register value [%02X]", reg);
            return ret;
        }
    }
    actual_value = (actual_value & ~mask) | (value & mask);

    return i2c_burst_write_dt(i2c, reg, &actual_value, 1);
}

static int tad5x12_configure(const const struct device *dev, struct audio_codec_cfg *audiocfg)
{
    uint8_t format, wordlen;
    const struct tad5x12_config *cfg = dev->config;

    if (audiocfg->dai_route != AUDIO_ROUTE_PLAYBACK)
    {
        return -ENOTSUP;
    }

    switch (audiocfg->dai_type)
    {
    case AUDIO_DAI_TYPE_I2S:
        format = AUDIO_DAI_TYPE_I2S;
        break;
    case AUDIO_DAI_TYPE_LEFT_JUSTIFIED:
        format = AUDIO_DAI_TYPE_LEFT_JUSTIFIED;
        break;
    default:
        return -ENOTSUP;
    }

    switch (audiocfg->dai_cfg.i2s.word_size)
    {
    case 16:
        wordlen = WORDLEN_16;
        break;
    case 20:
        wordlen = WORDLEN_20;
        break;
    case 24:
        wordlen = WORDLEN_24;
        break;
    case 32:
        wordlen = WORDLEN_32;
        break;
    default:
        return -ENOTSUP;
    }
}

static const struct audio_codec_api tad5x12_api = {
    .configure = tad5x12_configure,
    .start_output = tad5x12_start_output,
    .stop_output = tad5x12_stop_output,
    .set_property = tad5x12_set_property,
    .apply_properties = tad5x12_apply_properties,
    .route_input = tad5x12_route_input,
    .route_output = tad5x12_route_output,
};

#define TAD5X12_INIT(inst)                                       \
    static const struct tad5x12_config tad5x12_config_##inst = { \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                       \
    };                                                           \
    DEVICE_DT_INST_DEFINE(inst, tad5x12_init, NULL, NULL,        \
                          &tad5x12_config_##inst, POST_KERNEL,   \
                          CONFIG_AUDIO_CODEC_INIT_PRIORITY,      \
                          &tad5x12_api);

DT_INST_FOREACH_STATUS_OKAY(TAD5X12_INIT)