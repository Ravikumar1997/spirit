#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/spi/spi.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/platform_device.h>

#define STSP_TX_MAX_LENGTH 96
#define STSP_RX_MAX_LENGTH 96
#define SPI_AUTO_INCREMENT 0x00
#define ANA_FUNC_CONF1_BASE     ((uint8_t)0x00) /*!< ANA_FUNC_CONF1 Address (R/W) */
#define	DEVICE_INFO0_VERSION	((uint8_t)(0xF1)) /*!< Device version [7:0]; (0x55 in CUT1.0) */
#define READY    0x03
#define MAX_CMD_LEN 1

struct stsp_transfer_buffer {
	u8 rx_buf[STSP_RX_MAX_LENGTH];
	u8 tx_buf[STSP_TX_MAX_LENGTH];
};

struct stsp_dev {
	struct stsp_platform_data *pdata; 
	const char *name;
	struct device *dev;
	struct mutex lock;
	int bus_type;	
	const struct stsp_transfer_function *tf;
	struct stsp_transfer_buffer tb;
	u32 poll_interval;
};

struct stsp_transfer_function {
	int (*write) (struct stsp_dev *dev, u8 addr, int len, char *data);
	int (*read) (struct stsp_dev *dev, u8 addr, int len, u8 *data);
};

u8 char_to_int(char c)
{
	int i;
	int d = 'a' - 'A';

	i = c - 'A';
	if (i > 0)
		return ((i % d) % 6) + 10;
	i = c - '0';
	if (i > 0)
		return i % 10;
	return 0;
}

u8 ascii_to_hex(const char *c)
{
	int i, j, m = 1, n, ret = 0, d = 'a' - 'A';
	
	for (j = 1; j > -1; j--) {

   		i = c[j] - 'A';
		if (i >= 0) {
			n = ((i % d) % 6) + 10;
		}
		else {
			i = c[j] - '0';
			if (i >= 0)
				n = i % 10;
			else
				n = 0;
		}
		
		ret += n*m;
		m *= 16;
	}
	return ret;
}

static int stsp_spi_read(struct stsp_dev *device, u8 addr, int len, u8 *data)
{
	int err;
	struct spi_message msg;
	struct spi_device *spi = to_spi_device(device->dev);

	struct spi_transfer xfers[] = {
		{
			.tx_buf = device->tb.tx_buf,
			.bits_per_word = 8,
			.len = 1,
		},
		{
			.rx_buf = device->tb.rx_buf,
			.bits_per_word = 8,
			.len = len,
		}
	};

	spi_message_init(&msg);
	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);

	err = spi_sync(spi, &msg);
	if (err) {
			printk(KERN_INFO "spi read is called\n");
			}
		return err;

	memcpy(data, device->tb.rx_buf, len * sizeof(u8));
	
	return len;

}
static int stsp_spi_write(struct stsp_dev *device, u8 addr, int len, char *data)
{
	struct spi_message msg;

	struct spi_transfer xfers = {
		.tx_buf = device->tb.tx_buf,
		.bits_per_word = 8,
		.len = len + 1,
	};
	
	struct spi_device *spi = to_spi_device(device->dev);

	if (len >= STSP_TX_MAX_LENGTH)
		return -ENOMEM;

	if (len > 1)
		addr |= SPI_AUTO_INCREMENT;

	device->tb.tx_buf[0] = addr;

	memcpy(&device->tb.tx_buf[1], data, len);

	spi_message_init(&msg);
	spi_message_add_tail(&xfers, &msg);
	printk(KERN_INFO "spi write function is called\n");
	return spi_sync(spi, &msg);
}
static const struct stsp_transfer_function stsp_spi_tf = {
	.write = stsp_spi_write,
	.read = stsp_spi_read,
};
static ssize_t showOne(struct device *device,
			struct device_attribute *attr,
			char *buf)
{
	u32 val;
	struct stsp_dev *dev = dev_get_drvdata(device);
	printk(KERN_INFO "show function is called\n");
	mutex_lock(&dev->lock);
	val = dev->poll_interval;
	mutex_unlock(&dev->lock);

	return sprintf(buf, "%u\n", val);
}

static ssize_t storeOne(struct device *device, 
			struct device_attribute *attr,
		  const	char *buf, size_t size) 
{

	int err;
	char data[MAX_CMD_LEN];

	struct stsp_dev *dev = dev_get_drvdata(device);
	printk(KERN_INFO "spi store function is called\n");
	printk(" data in buff = %s\n", buf);
	struct stsp_platform_data *pdata = dev->pdata; 
//	mutex_lock(&dev->lock);
	
	data[0] = ascii_to_hex(buf);
	printk("data in 1 byte: %x\n", data[0]);
	
	err = dev->tf->write(dev, ANA_FUNC_CONF1_BASE, 1, data);

	printk(KERN_INFO "spi store2 function is called\n");
	if (err < 0) {  
		mutex_unlock(&dev->lock);
		return err;
		printk("%d", err); 
	}
	mutex_unlock(&dev->lock); 
	return size;

}


static struct device_attribute attributes[] = {
	__ATTR(fileOne, 0660, showOne, storeOne),
};

static int create_sysfs_interfaces(struct device *dev) 
{
	int i;

		printk(KERN_INFO "creating the sysfs interfaces\n");
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
static int stsp_hw_init(struct stsp_dev *dev)
{
	int err;
	u8 data;

	err = dev->tf->read(dev, DEVICE_INFO0_VERSION, 1, &data);
	if (err < 0)
	{
		dev_err(dev->dev, "error reading the version\n");
		return err;
	}

	if (data != DEVICE_INFO0_VERSION)
	{
		dev_err(dev->dev, "device unknown {0x%02x-0x%02x}\n", DEVICE_INFO0_VERSION, data);
		return -ENODEV;

	}
	return err;
}
static void remove_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
	printk(KERN_INFO"removing the sysfs interfaces\n");
}

int stsp_mag_probe(struct stsp_dev *dev)
{
	
	int err;

	err = create_sysfs_interfaces(dev->dev);
	err = stsp_hw_init(dev);
	if (err < 0)
	{
		dev_err(dev->dev, "hw init failed: %d\n", err);	
	}
	return 0; 
}

int stsp_mag_remove(struct stsp_dev *dev)
{
	printk(KERN_INFO "enter in the mag remove function\n");
	remove_sysfs_interfaces(dev->dev);
	return 0;


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
	dev->tf = &stsp_spi_tf;
	spi_set_drvdata(spi, dev);
	
	mutex_init(&dev->lock);

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

