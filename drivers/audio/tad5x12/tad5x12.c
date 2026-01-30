#define DT_DRV_COMPAT ti_tad5x12

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/audio/codec.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "tad5x12.h"

#define LOG_LEVEL CONFIG_AUDIO_CODEC_LOG_LEVEL
LOG_MODULE_REGISTER(tad5x12);

struct tad5x12_config
{
    struct i2c_dt_spec i2c;
};

#define tad5x12_write(_i2c, _reg, _value) \
    i2c_reg_write_byte_dt(_i2c, _reg, _value)

#define tad5x12_set_page(_i2c, _page) \
    tad5x12_write(_i2c, TAD5X12_REG_PAGE_CFG, _page)

#define tad5x12_sw_reset(_i2c) \
    tad5x12_write_masked(_i2c, TAD5X12_REG_SW_RESET, 1, BIT(0))

static inline int tad5x12_write_masked(const struct i2c_dt_spec *i2c, uint8_t reg,
                                       uint8_t value, uint8_t mask)
{
    return i2c_reg_update_byte_dt(i2c, reg, mask, value);
}

static int tad5x12_configure(const const struct device *dev, struct audio_codec_cfg *audiocfg)
{
    uint8_t format, wordlen;
    const struct tad5x12_config *cfg = dev->config;
    int ret;

    if (audiocfg->dai_route != AUDIO_ROUTE_PLAYBACK)
    {
        return -ENOTSUP;
    }

    switch (audiocfg->dai_type)
    {
    case AUDIO_DAI_TYPE_I2S:
        format = DAC_IF_FORMAT_I2S;
        break;
    case AUDIO_DAI_TYPE_LEFT_JUSTIFIED:
        format = DAC_IF_FORMAT_LEFT_JUSTIFIED;
        break;
    default:
        LOG_ERR("Unsupported DAI type: %d", audiocfg->dai_type);
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
        LOG_ERR("Unsupported word size: %d", audiocfg->dai_cfg.i2s.word_size);
        return -ENOTSUP;
    }

    ret = tad5x12_set_page(&cfg->i2c, 0);
    if (ret < 0)
    {
        LOG_ERR("Failed to set page 0");
        return -EIO;
    }
    // Set format and word length
    ret = tad5x12_write_masked(&cfg->i2c, TAD5X12_REG_PASI_CFG0, (format << 6) | (wordlen << 4), 0b11110000);
    if (ret < 0)
    {
        LOG_ERR("Failed to set format and word length");
        return -EIO;
    }

    // Set OUT1 to dual single ended mode, and 0.6 *Vref common mode
    ret = tad5x12_write_masked(&cfg->i2c, TAD5X12_REG_OUT1x_CFG0, 0b00000100, 0b00011100);
    if (ret < 0)
    {
        LOG_ERR("Failed to set output config");
        return -EIO;
    }

    // set OUT1P drive strength to headphone level, and 0db gain
    ret = tad5x12_write(&cfg->i2c, TAD5X12_REG_OUT1x_CFG1, 0b01100000);
    if (ret < 0)
    {
        LOG_ERR("Failed to set output drive strength and gain on OUT1P");
        return -EIO;
    }

    // set OUT1N drive strength to headphone level, and 0db gain
    ret = tad5x12_write(&cfg->i2c, TAD5X12_REG_OUT1x_CFG2, 0b01100000);
    if (ret < 0)
    {
        LOG_ERR("Failed to set output drive strength and gain on OUT1N");
        return -EIO;
    }

    // Enable output channel 1 and 2
    ret = tad5x12_write(&cfg->i2c, TAD5X12_REG_CH_EN, 0b00001100);
    if (ret < 0)
    {
        LOG_ERR("Failed to enable output channels");
        return -EIO;
    }

    return 0;
}

static int tad5x12_set_property(const struct device *dev, audio_property_t property,
                                audio_channel_t channel, audio_property_value_t val)
{
    const struct tad5x12_config *cfg = dev->config;
}

static int tad5x12_apply_properties(const struct device *dev)
{
    /* nothing to do because there is nothing cached */
    return 0;
}

static void tad5x12_start_output(const struct device *dev)
{
    const struct tad5x12_config *cfg = dev->config;
    int ret;

    // Power on DAC
    ret = tad5x12_write(&cfg->i2c, TAD5X12_REG_PWR_CFG, 0b01000000);
    if (ret < 0)
    {
        LOG_ERR("Failed to power on DAC");
    }
}

static void tad5x12_stop_output(const struct device *dev)
{
    const struct tad5x12_config *cfg = dev->config;
    int ret;

    // Power off DAC
    ret = tad5x12_write(&cfg->i2c, TAD5X12_REG_PWR_CFG, 0b00000000);
    if (ret < 0)
    {
        LOG_ERR("Failed to power off DAC");
    }
}

static int tad5x12_init(const struct device *dev)
{
    const struct tad5x12_config *cfg = dev->config;
    int ret;

    // Set to Page 0
    ret = tad5x12_set_page(&cfg->i2c, 0);
    if (ret < 0)
    {
        LOG_ERR("Failed to set page 0");
        return -EIO;
    }
    LOG_DBG("Set to Page 0");
    // Software Reset
    ret = tad5x12_sw_reset(&cfg->i2c);
    if (ret < 0)
    {
        LOG_ERR("Failed to reset device");
        return -EIO;
    }
    LOG_DBG("Device reset");

    k_msleep(1);

    // Exit Sleep Mode with DREG and VREF Enabled
    ret = tad5x12_write(&cfg->i2c, TAD5X12_REG_DEV_MISC_CFG, BIT(0) | BIT(3));
    if (ret < 0)
    {
        LOG_ERR("Failed to exit sleep mode");
        return -EIO;
    }
    LOG_DBG("Exited sleep mode");

    return 0;
}

static const struct audio_codec_api tad5x12_api = {
    .configure = tad5x12_configure,
    .start_output = tad5x12_start_output,
    .stop_output = tad5x12_stop_output,
    .set_property = tad5x12_set_property,
    .apply_properties = tad5x12_apply_properties,
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