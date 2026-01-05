// SPDX-License-Identifier: GPL-2.0
/*
 * GalaxyCore GC2607 sensor driver
 *
 * Copyright (C) 2026 Your Name
 *
 * Based on GC2145 driver and original Ingenic T41 driver
 */

#include <linux/acpi.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
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

	/* Device state */
	bool streaming;
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
 * V4L2 subdev operations
 */
static int gc2607_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct gc2607 *gc2607 = to_gc2607(sd);
	struct i2c_client *client = gc2607->client;

	dev_info(&client->dev, "%s: enable=%d (not implemented yet)\n",
		 __func__, enable);

	gc2607->streaming = enable;
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
static int __maybe_unused gc2607_detect(struct gc2607 *gc2607)
{
	struct i2c_client *client = gc2607->client;
	u8 chip_id_h, chip_id_l;
	int ret;

	ret = gc2607_read_reg(gc2607, GC2607_REG_CHIP_ID_H, &chip_id_h);
	if (ret)
		return ret;

	ret = gc2607_read_reg(gc2607, GC2607_REG_CHIP_ID_L, &chip_id_l);
	if (ret)
		return ret;

	if (chip_id_h != GC2607_CHIP_ID_H || chip_id_l != GC2607_CHIP_ID_L) {
		dev_err(&client->dev,
			"Wrong chip ID: expected 0x%02x%02x, got 0x%02x%02x\n",
			GC2607_CHIP_ID_H, GC2607_CHIP_ID_L,
			chip_id_h, chip_id_l);
		return -ENODEV;
	}

	dev_info(&client->dev, "GC2607 chip ID detected: 0x%02x%02x\n",
		 chip_id_h, chip_id_l);

	return 0;
}

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

	dev_info(dev, "GC2607 probe successful (skeleton driver)\n");
	dev_info(dev, "  I2C address: 0x%02x\n", client->addr);
	dev_info(dev, "  I2C adapter: %s\n", client->adapter->name);

	return 0;

err_media:
	media_entity_cleanup(&gc2607->sd.entity);
	v4l2_ctrl_handler_free(&gc2607->ctrls);
	return ret;
}

static void gc2607_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2607 *gc2607 = to_gc2607(sd);

	dev_info(&client->dev, "GC2607 driver removed\n");

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&gc2607->ctrls);
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
