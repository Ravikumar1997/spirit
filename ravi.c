#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/spi/spi.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/platform_device.h>
struct stsp_dev {
	const char *name;
	struct device *dev;
	struct mutex lock;
	int bus_type;	
};

static ssize_t showOne(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return 'm';
}

static ssize_t storeOne(struct device *dev, 
			struct device_attribute *attr,
			const char *buf, size_t size) 
{
	return 'r';
}

static struct device_attribute attributes[] = {
	__ATTR(fileOne, 0660, showOne, storeOne),
};

static int create_sysfs_interfaces(struct device *dev) 
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
		goto error;
		return 0;

error:
	for (; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s: unable to create interfaces\n", __func__);

	return -1;
}

static void remove_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
}
int stsp_mag_probe(struct stsp_dev *dev)
{
	
	int err;

	err = create_sysfs_interfaces(dev->dev);
	return 1;
}

int stsp_mag_remove(struct stsp_dev *dev)
{
	remove_sysfs_interfaces(dev->dev);
	return 1;


}



static int stsp_spi_probe(struct spi_device *spi)
{
	int ret;
	struct stsp_dev *dev;
	dev_info(&spi->dev, "probing start\n");
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);

	if(!dev)
	ret = -ENOMEM;
	dev->dev = &spi->dev;
	dev->name = spi->modalias;
	dev->bus_type = BUS_SPI;
	spi_set_drvdata(spi, dev);

	ret = stsp_mag_probe(dev);

	if (ret<0) { 
		kfree(dev);
		return ret;
	}
	return 0;


}



static int stsp_spi_remove(struct spi_device *spi)
{
	struct stsp_dev *dev = spi_get_drvdata(spi);

	dev_info(&spi->dev, "driver removing\n");

	stsp_mag_remove(dev);
	kfree(dev);
	
	return 0;
}



static const struct spi_device_id stsp_spi_ids[] = {
	{ "stsp", 0 },
	{ },
};
MODULE_DEVICE_TABLE(spi, stsp_spi_ids);

static const struct of_device_id stsp_spi_id_table[] = {
	{ .compatible = "st,stsp", },
	{ },
};
MODULE_DEVICE_TABLE(of, stsp_spi_id_table);

static struct spi_driver stsp_spi_driver = {
	.driver		= {
		.owner = THIS_MODULE,
       		.name	= "stsp",
		.of_match_table = stsp_spi_id_table,
	},
	.probe		= stsp_spi_probe,
	.remove		= stsp_spi_remove,
	.id_table	= stsp_spi_ids, 
};

module_spi_driver(stsp_spi_driver);

MODULE_DESCRIPTION("ST SPI SPIRIT1 driver");
MODULE_LICENSE("GPL");

