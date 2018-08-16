// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/metadata.h>

#include <ddk/protocol/platform-bus.h>
#include <ddk/protocol/platform-defs.h>

#include <soc/aml-common/aml-tdm.h>
#include <soc/aml-s905d2/s905d2-gpio.h>
#include <soc/aml-s905d2/s905d2-hiu.h>
#include <soc/aml-s905d2/s905d2-hw.h>

#include <limits.h>

#include "astro.h"


#if 0
static char teststr[] = "Eric Holland\0";

static pbus_boot_metadata_t meta[] = {
    {
        .type = AML_TDM_METADATA,
        .data = teststr,
        .extra = 0,
        .len = sizeof(teststr)
    },
};
#endif

static const pbus_gpio_t audio_gpios[] = {
    {
        // AUDIO_SOC_FAULT_L
        .gpio = S905D2_GPIOA(4),
    },
    {
        // SOC_AUDIO_EN
        .gpio = S905D2_GPIOA(5),
    },
};


static const pbus_mmio_t audio_mmios[] = {
    {
        .base = S905D2_EE_AUDIO_BASE,
        .length = S905D2_EE_AUDIO_LENGTH
    },
};

static const pbus_bti_t tdm_btis[] = {
    {
        .iommu_index = 0,
        .bti_id = 0,
    },
};

static const pbus_i2c_channel_t codec_i2c[] = {
    {
        .bus_id = 2,
        .address = 0x48,
    },
};

static pbus_dev_t aml_tdm_dev = {
    .name = "AstroAudio",
    .vid = PDEV_VID_AMLOGIC,
    .pid = PDEV_PID_AMLOGIC_S905D2,
    .did = PDEV_DID_AMLOGIC_TDM,
    .gpios = audio_gpios,
    .gpio_count = countof(audio_gpios),
    .i2c_channels = codec_i2c,
    .i2c_channel_count = countof(codec_i2c),
    .mmios = audio_mmios,
    .mmio_count = countof(audio_mmios),
    .btis = tdm_btis,
    .bti_count = countof(tdm_btis),
    //.boot_metadata = meta,
    //.boot_metadata_count = countof(meta)
};

zx_status_t astro_tdm_init(aml_bus_t* bus) {

    aml_hiu_dev_t hiu;
    zx_handle_t bti;
    zx_status_t status = iommu_get_bti(&bus->iommu, 0, 0, &bti);
    if (status != ZX_OK) {
        zxlogf(ERROR, "audio_bus_bind: iommu_get_bti failed: %d\n", status);
        return status;
    }

    status = s905d2_hiu_init(bti, &hiu);
    if (status != ZX_OK) {
        zxlogf(ERROR, "hiu_init: failed: %d\n", status);
        return status;
    }

    aml_pll_dev_t hifi_pll;
    s905d2_pll_init(&hiu, &hifi_pll, HIFI_PLL);
    status = s905d2_pll_set_rate(&hifi_pll, 3072000000);
    if (status != ZX_OK) {
        zxlogf(ERROR,"Invalid rate selected for hifipll\n");
        return status;
    }

    s905d2_pll_ena(&hifi_pll);

    //Config of peripheral pins should be done in the board file vs. the peripheral
    // driver since peripherals can often be muxed to one of many pins.  This allows
    // the peripheral driver to remain agnostic to specific board wiring.

    // TDM pin assignments
    gpio_set_alt_function(&bus->gpio, S905D2_GPIOA(1), S905D2_GPIOA_1_TDMB_SCLK_FN);
    gpio_set_alt_function(&bus->gpio, S905D2_GPIOA(2), S905D2_GPIOA_2_TDMB_FS_FN);
    gpio_set_alt_function(&bus->gpio, S905D2_GPIOA(3), S905D2_GPIOA_3_TDMB_D0_FN);
    gpio_set_alt_function(&bus->gpio, S905D2_GPIOA(6), S905D2_GPIOA_6_TDMB_DIN3_FN);

    // PDM pin assignments
    gpio_set_alt_function(&bus->gpio, S905D2_GPIOA(7), S905D2_GPIOA_7_PDM_DCLK_FN);
    gpio_set_alt_function(&bus->gpio, S905D2_GPIOA(8), S905D2_GPIOA_8_PDM_DIN0_FN);

    gpio_config(&bus->gpio, S905D2_GPIOA(5), GPIO_DIR_OUT);
    gpio_write(&bus->gpio, S905D2_GPIOA(5), 1);


    status = pbus_device_add(&bus->pbus, &aml_tdm_dev, 0);
    if (status != ZX_OK) {
        zxlogf(ERROR, "astro_tdm_init: pbus_device_add failed: %d\n", status);
        return status;
    }

    return ZX_OK;
}