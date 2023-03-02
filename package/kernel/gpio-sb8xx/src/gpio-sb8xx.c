// SPDX-License-Identifier: GPL-2.0+

/*
 * GPIO driver for the AMD G series FCH (eg. GX-412TC)
 *
 * Copyright (C) 2018 metux IT consult
 * Author: Enrico Weigelt, metux IT consult <info@metux.net>
 *
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/driver.h>
#include <linux/dmi.h>
#include <linux/platform_data/gpio/gpio-amd-fch.h>
#include <linux/spinlock.h>

#define AMD_FCH_MMIO_BASE		0xFED80000
#define AMD_FCH_GPIO_BASE		0x100
#define AMD_FCH_GPIO_SIZE		0x100

#define AMD_FCH_GPIO_FLAG_DIRECTION	BIT(5)  // input enable if 1
#define AMD_FCH_GPIO_FLAG_WRITE		BIT(6)
#define AMD_FCH_GPIO_FLAG_READ		BIT(7)

static const struct resource amd_fch_gpio_iores =
	DEFINE_RES_MEM_NAMED(
		AMD_FCH_MMIO_BASE + AMD_FCH_GPIO_BASE,
		AMD_FCH_GPIO_SIZE,
		"sb8xx-gpio-iomem");

struct amd_fch_gpio_priv {
	struct gpio_chip		gc;
	void __iomem			*base;
	spinlock_t			lock;
};

static inline int sb8xx_gpio_is_invalid(struct amd_fch_gpio_priv *priv, unsigned int gpio)
{
	return (gpio > 67 && gpio < 128) || (gpio > 150 && gpio < 160) || (gpio > 228);
}

static void __iomem *amd_fch_gpio_addr(struct amd_fch_gpio_priv *priv,
				       unsigned int gpio)
{
	return priv->base + gpio*sizeof(u8);
}

static int amd_fch_gpio_direction_input(struct gpio_chip *gc,
					unsigned int gpio)
{
	unsigned long flags;
	struct amd_fch_gpio_priv *priv = gpiochip_get_data(gc);
	void __iomem *ptr = amd_fch_gpio_addr(priv, gpio);

	if (sb8xx_gpio_is_invalid(priv, gpio))
		return -EINVAL;

	spin_lock_irqsave(&priv->lock, flags);
	writeb_relaxed(readb_relaxed(ptr) | AMD_FCH_GPIO_FLAG_DIRECTION, ptr);
	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int amd_fch_gpio_direction_output(struct gpio_chip *gc,
					 unsigned int gpio, int value)
{
	unsigned long flags;
	struct amd_fch_gpio_priv *priv = gpiochip_get_data(gc);
	void __iomem *ptr = amd_fch_gpio_addr(priv, gpio);
	u8 val;

	if (sb8xx_gpio_is_invalid(priv, gpio))
		return -EINVAL;

	spin_lock_irqsave(&priv->lock, flags);

	val = readb_relaxed(ptr);
	if (value)
		val |= AMD_FCH_GPIO_FLAG_WRITE;
	else
		val &= ~AMD_FCH_GPIO_FLAG_WRITE;

	writeb_relaxed(val & ~AMD_FCH_GPIO_FLAG_DIRECTION, ptr);

	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int amd_fch_gpio_get_direction(struct gpio_chip *gc, unsigned int gpio)
{
	int ret;
	unsigned long flags;
	struct amd_fch_gpio_priv *priv = gpiochip_get_data(gc);
	void __iomem *ptr = amd_fch_gpio_addr(priv, gpio);

	spin_lock_irqsave(&priv->lock, flags);
	ret = (readb_relaxed(ptr) & AMD_FCH_GPIO_FLAG_DIRECTION);
	spin_unlock_irqrestore(&priv->lock, flags);

	return ret ? GPIO_LINE_DIRECTION_IN : GPIO_LINE_DIRECTION_OUT;
}

static void amd_fch_gpio_set(struct gpio_chip *gc,
			     unsigned int gpio, int value)
{
	unsigned long flags;
	struct amd_fch_gpio_priv *priv = gpiochip_get_data(gc);
	void __iomem *ptr = amd_fch_gpio_addr(priv, gpio);
	u8 mask;

	spin_lock_irqsave(&priv->lock, flags);

	mask = readb_relaxed(ptr);
	if (value)
		mask |= AMD_FCH_GPIO_FLAG_WRITE;
	else
		mask &= ~AMD_FCH_GPIO_FLAG_WRITE;
	writeb_relaxed(mask, ptr);

	spin_unlock_irqrestore(&priv->lock, flags);
}

static int amd_fch_gpio_get(struct gpio_chip *gc,
			    unsigned int gpio)
{
	unsigned long flags;
	int ret;
	struct amd_fch_gpio_priv *priv = gpiochip_get_data(gc);
	void __iomem *ptr = amd_fch_gpio_addr(priv, gpio);

	spin_lock_irqsave(&priv->lock, flags);
	ret = (readb_relaxed(ptr) & AMD_FCH_GPIO_FLAG_READ);
	spin_unlock_irqrestore(&priv->lock, flags);

	return ret;
}

static int amd_fch_gpio_request(struct gpio_chip *chip,
				unsigned int gpio_pin)
{
	return 0;
}

static int amd_fch_gpio_probe(struct platform_device *pdev)
{
	struct amd_fch_gpio_priv *priv;

	printk(KERN_INFO "gpio-sb8xx: probe function\n");
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->gc.owner			= THIS_MODULE;
	priv->gc.parent			= &pdev->dev;
	priv->gc.label			= dev_name(&pdev->dev);
	priv->gc.ngpio			= 228;
	priv->gc.base			= 0;
	priv->gc.request		= amd_fch_gpio_request;
	priv->gc.direction_input	= amd_fch_gpio_direction_input;
	priv->gc.direction_output	= amd_fch_gpio_direction_output;
	priv->gc.get_direction		= amd_fch_gpio_get_direction;
	priv->gc.get			= amd_fch_gpio_get;
	priv->gc.set			= amd_fch_gpio_set;

	spin_lock_init(&priv->lock);

	priv->base = devm_ioremap_resource(&pdev->dev, &amd_fch_gpio_iores);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	platform_set_drvdata(pdev, priv);

	return devm_gpiochip_add_data(&pdev->dev, &priv->gc, priv);
}

static struct of_device_id gpio_sb8xx_of_match[] = {
	{ .compatible = "gpio-sb8xx", },
	{ },
};
MODULE_DEVICE_TABLE(of, gpio_sb8xx_of_match);

static struct platform_driver sb8xx_gpio_driver = {
	.probe = amd_fch_gpio_probe,
	.driver = {
		.name = AMD_FCH_GPIO_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = gpio_sb8xx_of_match,
	},	
};

static struct platform_device *sb8xx_gpio_pdev;

static int __init init_sb8xx_gpio(void)
{
	int rc;

	printk(KERN_INFO "gpio-sb8xx: init function\n");
	rc = platform_driver_register(&sb8xx_gpio_driver);
	if (rc)
		return rc;
	return 0;	

}

static void __exit exit_sb8xx_gpio(void)
{
	platform_device_unregister(sb8xx_gpio_pdev);
	platform_driver_unregister(&sb8xx_gpio_driver);
}

module_init(init_sb8xx_gpio);
module_exit(exit_sb8xx_gpio);

MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_DESCRIPTION("AMD G-series FCH GPIO driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" AMD_FCH_GPIO_DRIVER_NAME);
MODULE_DEVICE_TABLE(dmi, apu_gpio_dmi_table);
