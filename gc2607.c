// SPDX-License-Identifier: GPL-2.0
/*
 * GalaxyCore GC2607 sensor driver
 *
 * Copyright (C) 2026 Your Name
 *
 * Based on GC2145 driver and original Ingenic T41 driver
 */

#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>

#define GC2607_CHIP_ID_H		0x26
#define GC2607_CHIP_ID_L		0x07
#define GC2607_REG_CHIP_ID_H		0x03f0
#define GC2607_REG_CHIP_ID_L		0x03f1

struct gc2607 {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct i2c_client *client;

	/* V4L2 controls */
	struct v4l2_ctrl_handler ctrls;

	/* Power management resources (provided by INT3472 PMIC) */
	struct clk *xclk;		/* Master clock (typically 19.2 MHz) */
	struct gpio_desc *reset_gpio;	/* Reset GPIO (active low) */
	struct gpio_desc *powerdown_gpio; /* Power-down GPIO (if present) */
	struct regulator_bulk_data supplies[3];

	/* Device state */
	bool streaming;
	bool powered;
};

static inline struct gc2607 *to_gc2607(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc2607, sd);
}

/*
 * I2C I/O operations
 * GC2607 uses 16-bit register addresses and 8-bit values
 */
static int gc2607_read_reg(struct gc2607 *gc2607, u16 reg, u8 *val)
{
	struct i2c_client *client = gc2607->client;
	struct i2c_msg msgs[2];
	u8 addr_buf[2];
	int ret;

	addr_buf[0] = reg >> 8;
	addr_buf[1] = reg & 0xff;

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = addr_buf;

	/* Read data */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = val;

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to read reg 0x%04x: %d\n", reg, ret);
		return ret;
	}

	return 0;
}

static int __maybe_unused gc2607_write_reg(struct gc2607 *gc2607, u16 reg, u8 val)
{
	struct i2c_client *client = gc2607->client;
	u8 buf[3];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;

	ret = i2c_master_send(client, buf, 3);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to write reg 0x%04x: %d\n", reg, ret);
		return ret;
	}

	return 0;
}

/*
 * Power management
 */
static int gc2607_power_on(struct gc2607 *gc2607)
{
	struct i2c_client *client = gc2607->client;
	int ret;

	dev_info(&client->dev, "%s: Powering on sensor\n", __func__);

	/* Enable regulators if available */
	if (gc2607->supplies[0].supply) {
		ret = regulator_bulk_enable(ARRAY_SIZE(gc2607->supplies),
					     gc2607->supplies);
		if (ret) {
			dev_err(&client->dev, "Failed to enable regulators: %d\n", ret);
			return ret;
		}
		dev_dbg(&client->dev, "Regulators enabled\n");
		usleep_range(5000, 6000);
	}

	/* Enable master clock if available */
	if (gc2607->xclk) {
		ret = clk_prepare_enable(gc2607->xclk);
		if (ret) {
			dev_err(&client->dev, "Failed to enable clock: %d\n", ret);
			goto err_reg;
		}
		dev_dbg(&client->dev, "Clock enabled\n");
		usleep_range(5000, 6000);
	}

	/*
	 * Reset sequence from reference driver (gc2607.c:689-694):
	 * Physical: HIGH (20ms) → LOW (20ms) → HIGH (10ms)
	 *
	 * For gpiod API with active-low GPIO (GPIOD_OUT_LOW):
	 * - gpiod_set_value(0) = de-assert = physical HIGH = running
	 * - gpiod_set_value(1) = assert = physical LOW = reset
	 */
	if (gc2607->reset_gpio) {
		/* Start: de-asserted (running) */
		gpiod_set_value_cansleep(gc2607->reset_gpio, 0);
		msleep(20);

		/* Assert reset (put sensor into reset) */
		gpiod_set_value_cansleep(gc2607->reset_gpio, 1);
		msleep(20);

		/* De-assert reset (release from reset, sensor boots) */
		gpiod_set_value_cansleep(gc2607->reset_gpio, 0);
		msleep(10);

		dev_dbg(&client->dev, "Reset pulse completed\n");
	}

	/*
	 * Powerdown sequence from reference driver (gc2607.c:702-707):
	 * If present, pulse the powerdown GPIO
	 * Assuming active-high powerdown (high = powered down)
	 */
	if (gc2607->powerdown_gpio) {
		/* Power down */
		gpiod_set_value_cansleep(gc2607->powerdown_gpio, 1);
		msleep(10);

		/* Power up */
		gpiod_set_value_cansleep(gc2607->powerdown_gpio, 0);
		msleep(10);

		dev_dbg(&client->dev, "Powerdown pulse completed\n");
	}

	/* Wait for sensor to fully boot */
	msleep(20);

	gc2607->powered = true;
	dev_info(&client->dev, "Sensor powered on\n");

	return 0;

err_reg:
	if (gc2607->supplies[0].supply)
		regulator_bulk_disable(ARRAY_SIZE(gc2607->supplies), gc2607->supplies);
	return ret;
}

static void gc2607_power_off(struct gc2607 *gc2607)
{
	struct i2c_client *client = gc2607->client;

	dev_info(&client->dev, "%s: Powering off sensor\n", __func__);

	if (!gc2607->powered)
		return;

	/* Assert reset if GPIO exists */
	if (gc2607->reset_gpio)
		gpiod_set_value_cansleep(gc2607->reset_gpio, 0);

	/* Assert power-down if GPIO exists */
	if (gc2607->powerdown_gpio)
		gpiod_set_value_cansleep(gc2607->powerdown_gpio, 1);

	/* Disable master clock if available */
	if (gc2607->xclk)
		clk_disable_unprepare(gc2607->xclk);

	/* Disable regulators if available */
	if (gc2607->supplies[0].supply)
		regulator_bulk_disable(ARRAY_SIZE(gc2607->supplies), gc2607->supplies);

	gc2607->powered = false;
	dev_info(&client->dev, "Sensor powered off\n");
}

/*
 * V4L2 subdev operations
 */
static int gc2607_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct gc2607 *gc2607 = to_gc2607(sd);
	struct i2c_client *client = gc2607->client;
	int ret;

	if (enable) {
		ret = pm_runtime_resume_and_get(&client->dev);
		if (ret)
			return ret;

		dev_info(&client->dev, "Stream ON (register init not implemented yet)\n");
		gc2607->streaming = true;
	} else {
		dev_info(&client->dev, "Stream OFF\n");
		gc2607->streaming = false;
		pm_runtime_put(&client->dev);
	}

	return 0;
}

static const struct v4l2_subdev_video_ops gc2607_video_ops = {
	.s_stream = gc2607_s_stream,
};

static const struct v4l2_subdev_ops gc2607_subdev_ops = {
	.video = &gc2607_video_ops,
};

/*
 * Detect chip ID to verify sensor presence
 */
static int gc2607_detect(struct gc2607 *gc2607)
{
	struct i2c_client *client = gc2607->client;
	u8 chip_id_h = 0, chip_id_l = 0;
	int ret;

	dev_info(&client->dev, "Detecting chip ID...\n");

	ret = gc2607_read_reg(gc2607, GC2607_REG_CHIP_ID_H, &chip_id_h);
	if (ret) {
		dev_err(&client->dev, "Failed to read chip ID high byte: %d\n", ret);
		dev_err(&client->dev, "This usually means:\n");
		dev_err(&client->dev, "  - Sensor is not powered\n");
		dev_err(&client->dev, "  - Wrong I2C address (currently 0x%02x)\n", client->addr);
		dev_err(&client->dev, "  - I2C bus issue\n");
		return ret;
	}

	ret = gc2607_read_reg(gc2607, GC2607_REG_CHIP_ID_L, &chip_id_l);
	if (ret) {
		dev_err(&client->dev, "Failed to read chip ID low byte: %d\n", ret);
		return ret;
	}

	dev_info(&client->dev, "Read chip ID: 0x%02x%02x\n", chip_id_h, chip_id_l);

	if (chip_id_h != GC2607_CHIP_ID_H || chip_id_l != GC2607_CHIP_ID_L) {
		dev_err(&client->dev,
			"Wrong chip ID: expected 0x%02x%02x, got 0x%02x%02x\n",
			GC2607_CHIP_ID_H, GC2607_CHIP_ID_L,
			chip_id_h, chip_id_l);
		return -ENODEV;
	}

	dev_info(&client->dev, "GC2607 chip detected successfully!\n");

	return 0;
}

/*
 * Runtime PM operations
 */
static int gc2607_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2607 *gc2607 = to_gc2607(sd);

	gc2607_power_off(gc2607);
	return 0;
}

static int gc2607_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2607 *gc2607 = to_gc2607(sd);

	return gc2607_power_on(gc2607);
}

static const struct dev_pm_ops gc2607_pm_ops = {
	SET_RUNTIME_PM_OPS(gc2607_runtime_suspend, gc2607_runtime_resume, NULL)
};

/*
 * I2C driver probe/remove
 */
static int gc2607_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct gc2607 *gc2607;
	int ret;

	dev_info(dev, "GC2607 probe started\n");

	gc2607 = devm_kzalloc(dev, sizeof(*gc2607), GFP_KERNEL);
	if (!gc2607)
		return -ENOMEM;

	gc2607->client = client;

	/* Initialize regulator supply names */
	gc2607->supplies[0].supply = "avdd";  /* Analog power */
	gc2607->supplies[1].supply = "dovdd"; /* I/O power */
	gc2607->supplies[2].supply = "dvdd";  /* Digital core power */

	/* Get regulators (optional - INT3472 may handle power internally) */
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(gc2607->supplies),
				       gc2607->supplies);
	if (ret) {
		dev_warn(dev, "Regulators not available (%d), assuming INT3472 handles power\n", ret);
		/* Clear supplies array to indicate no regulators */
		memset(gc2607->supplies, 0, sizeof(gc2607->supplies));
	} else {
		dev_info(dev, "Got %d regulators from platform\n",
			 (int)ARRAY_SIZE(gc2607->supplies));
	}

	/* Get reset GPIO (optional on some platforms) */
	gc2607->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(gc2607->reset_gpio)) {
		ret = PTR_ERR(gc2607->reset_gpio);
		dev_err(dev, "Failed to get reset GPIO: %d\n", ret);
		return ret;
	}

	if (gc2607->reset_gpio)
		dev_info(dev, "Got reset GPIO\n");
	else
		dev_warn(dev, "No reset GPIO, assuming INT3472 handles it\n");

	/* Get powerdown GPIO (optional - active high: 1=powerdown, 0=running) */
	gc2607->powerdown_gpio = devm_gpiod_get_optional(dev, "powerdown",
							  GPIOD_OUT_LOW);
	if (IS_ERR(gc2607->powerdown_gpio)) {
		ret = PTR_ERR(gc2607->powerdown_gpio);
		dev_err(dev, "Failed to get powerdown GPIO: %d\n", ret);
		return ret;
	}

	if (gc2607->powerdown_gpio)
		dev_info(dev, "Got powerdown GPIO\n");
	else
		dev_dbg(dev, "No powerdown GPIO\n");

	/* Get master clock (optional - INT3472 may provide it internally) */
	gc2607->xclk = devm_clk_get_optional(dev, NULL);
	if (IS_ERR(gc2607->xclk)) {
		ret = PTR_ERR(gc2607->xclk);
		dev_err(dev, "Failed to get clock: %d\n", ret);
		return ret;
	}

	if (gc2607->xclk) {
		dev_info(dev, "Got clock from platform: %lu Hz\n",
			 clk_get_rate(gc2607->xclk));
	} else {
		dev_warn(dev, "No clock from platform, assuming INT3472 provides it\n");
	}

	dev_info(dev, "Resources acquired successfully\n");

	/* Initialize V4L2 subdev */
	v4l2_i2c_subdev_init(&gc2607->sd, client, &gc2607_subdev_ops);

	/* Initialize media pad */
	gc2607->pad.flags = MEDIA_PAD_FL_SOURCE;
	gc2607->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&gc2607->sd.entity, 1, &gc2607->pad);
	if (ret) {
		dev_err(dev, "Failed to init media entity: %d\n", ret);
		return ret;
	}

	/* Initialize control handler (empty for now) */
	v4l2_ctrl_handler_init(&gc2607->ctrls, 0);
	gc2607->sd.ctrl_handler = &gc2607->ctrls;

	if (gc2607->ctrls.error) {
		ret = gc2607->ctrls.error;
		dev_err(dev, "Control handler init failed: %d\n", ret);
		goto err_media;
	}

	/* Enable runtime PM */
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	/* Power on sensor and detect chip ID */
	ret = pm_runtime_resume_and_get(dev);
	if (ret) {
		dev_err(dev, "Failed to power on sensor: %d\n", ret);
		goto err_pm;
	}

	ret = gc2607_detect(gc2607);
	if (ret) {
		dev_err(dev, "Failed to detect sensor: %d\n", ret);
		goto err_power;
	}

	/* Power off after detection */
	pm_runtime_put(dev);

	dev_info(dev, "GC2607 probe successful\n");
	dev_info(dev, "  I2C address: 0x%02x\n", client->addr);
	dev_info(dev, "  I2C adapter: %s\n", client->adapter->name);

	return 0;

err_power:
	pm_runtime_put_noidle(dev);
err_pm:
	pm_runtime_disable(dev);
	pm_runtime_set_suspended(dev);
	v4l2_ctrl_handler_free(&gc2607->ctrls);
err_media:
	media_entity_cleanup(&gc2607->sd.entity);
	return ret;
}

static void gc2607_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2607 *gc2607 = to_gc2607(sd);
	struct device *dev = &client->dev;

	dev_info(dev, "GC2607 driver removing\n");

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&gc2607->ctrls);

	/* Disable runtime PM */
	pm_runtime_disable(dev);
	if (!pm_runtime_status_suspended(dev))
		gc2607_power_off(gc2607);
	pm_runtime_set_suspended(dev);

	dev_info(dev, "GC2607 driver removed\n");
}

/*
 * ACPI match table for Huawei MateBook Pro
 */
static const struct acpi_device_id gc2607_acpi_ids[] = {
	{ "GCTI2607" },
	{ }
};
MODULE_DEVICE_TABLE(acpi, gc2607_acpi_ids);

/*
 * I2C device ID table
 */
static const struct i2c_device_id gc2607_id[] = {
	{ "gc2607", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, gc2607_id);

static struct i2c_driver gc2607_i2c_driver = {
	.driver = {
		.name = "gc2607",
		.pm = &gc2607_pm_ops,
		.acpi_match_table = gc2607_acpi_ids,
	},
	.probe = gc2607_probe,
	.remove = gc2607_remove,
	.id_table = gc2607_id,
};

module_i2c_driver(gc2607_i2c_driver);

MODULE_DESCRIPTION("GalaxyCore GC2607 sensor driver");
MODULE_AUTHOR("Your Name <your.email@example.com>");
MODULE_LICENSE("GPL");
