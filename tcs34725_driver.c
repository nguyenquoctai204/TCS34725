#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#define DRIVER_NAME "tcs34725_driver"
#define CLASS_NAME "tcs34725"
#define DEVICE_NAME "tcs34725"

#define TCS34725_COMMAND_BIT   0x80
#define TCS34725_ENABLE        0x00
#define TCS34725_STATUS_AVALID 0x01  // Bit 0
#define TCS34725_ATIME         0x01 // Thời gian tích ADC (integration time)
// Thời gian tích ADC (ATIME)
#define TCS34725_ATIME_700MS   0x00  // Dộ nhạy cao
#define TCS34725_ENABLE_AEN    0x02  // Cho phép ADC
#define TCS34725_ENABLE_PON    0x01  // Bật nguồn

//////////// TCS34725 REGISTER
#define TCS34725_REG_STATUS    0x13
#define TCS34725_REG_CDATAL    0x14
#define TCS34725_REG_CDATAH    0x15
#define TCS34725_REG_RDATAL    0x16
#define TCS34725_REG_RDATH     0x17
#define TCS34725_REG_GDATAL    0x18
#define TCS34725_REG_GDATH     0x19
#define TCS34725_REG_BDATAL    0x1A
#define TCS34725_REG_BDATH     0x1B
#define TCS34725_REG_CONTROL   0x0F


// IOCTL commands
#define TCS34725_IOCTL_MAGIC 't'
#define TCS34725_IOCTL_READ_R _IOR(TCS34725_IOCTL_MAGIC, 1, int)
#define TCS34725_IOCTL_READ_G _IOR(TCS34725_IOCTL_MAGIC, 2, int)
#define TCS34725_IOCTL_READ_B _IOR(TCS34725_IOCTL_MAGIC, 3, int)
#define TCS34725_IOCTL_READ_C _IOR(TCS34725_IOCTL_MAGIC, 4, int)
#define TCS34725_IOCTL_READ_STATUS _IOR(TCS34725_IOCTL_MAGIC, 5, int)
#define TCS34725_IOCTL_SET_GAIN _IOW(TCS34725_IOCTL_MAGIC, 7, int)

static struct i2c_client *tcs34725_client; 
static struct class* tcs34725_class = NULL;
static struct device* tcs34725_device = NULL;
static int major_number;

// Function SETTUP TCS34725
static void tcs34725_init_sensor(struct i2c_client *client)
{
    // Ghi thời gian tích ADC
    i2c_smbus_write_byte_data(client, TCS34725_COMMAND_BIT | TCS34725_ATIME, TCS34725_ATIME_700MS);
    // Bật Power
    i2c_smbus_write_byte_data(client, TCS34725_COMMAND_BIT | TCS34725_ENABLE, TCS34725_ENABLE_PON);
    msleep(3);
    // Bật ADC (RGBC)
    i2c_smbus_write_byte_data(client, TCS34725_COMMAND_BIT | TCS34725_ENABLE, TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN);
    msleep(700);//ợi tích hợp dữ liệu xong
}

// Function READ RGBC VALUE
static int tcs34725_read_color(struct i2c_client *client, u8 reg_low)
{
    int low = i2c_smbus_read_byte_data(client, TCS34725_COMMAND_BIT | reg_low);
    int high = i2c_smbus_read_byte_data(client, TCS34725_COMMAND_BIT | (reg_low + 1));
    if (low < 0 || high < 0) {
        printk(KERN_ERR "TCS34725: Failed to read color data at reg 0x%02X\n", reg_low);
        return -EIO;
    }
    return (high << 8) | low;
}

// Function SET TCS34725 GAIN
static int tcs34725_set_gain(struct i2c_client *client, u8 gain)
{
    if (gain > 0x03) {
        printk(KERN_ERR "TCS34725: Invalid gain value: %d\n", gain);
        return -EINVAL;
    }
    int ret = i2c_smbus_write_byte_data(client, TCS34725_COMMAND_BIT | TCS34725_REG_CONTROL, gain);
    if (ret < 0) {
        printk(KERN_ERR "TCS34725: Failed to write gain\n");
        return ret;
    }
    printk(KERN_INFO "TCS34725: Gain set to %d\n", gain);
    return 0;
}

//Function READ TCS34725 STATUS
static bool tcs34725_read_status(struct i2c_client *client)
{
    int status = i2c_smbus_read_byte_data(client, TCS34725_COMMAND_BIT | TCS34725_REG_STATUS);
    if (status < 0) {
        printk(KERN_ERR "Failed to read STATUS register\n");
        return false;
    }
    return (status & TCS34725_STATUS_AVALID) != 0;
}

static long tcs34725_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int value;

    switch (cmd) {
        case TCS34725_IOCTL_READ_R:
            value = tcs34725_read_color(tcs34725_client, TCS34725_REG_RDATAL);
            break;
        case TCS34725_IOCTL_READ_G:
            value = tcs34725_read_color(tcs34725_client, TCS34725_REG_GDATAL);
            break;
        case TCS34725_IOCTL_READ_B:
            value = tcs34725_read_color(tcs34725_client, TCS34725_REG_BDATAL);
            break;
        case TCS34725_IOCTL_READ_C:
            value = tcs34725_read_color(tcs34725_client, TCS34725_REG_CDATAL);
            break;
        case TCS34725_IOCTL_SET_GAIN:
        {
            int gain_val;
            if (copy_from_user(&gain_val, (int __user *)arg, sizeof(gain_val)))
                return -EFAULT;
            return tcs34725_set_gain(tcs34725_client, (u8)gain_val);
        }
        case TCS34725_IOCTL_READ_STATUS:
            value = tcs34725_read_status(tcs34725_client);
            break;           
        default:
            return -EINVAL;
    }

    if (value < 0) return value;
    if (copy_to_user((int __user *)arg, &value, sizeof(value)))
        return -EFAULT;

    return 0;
}

static int tcs34725_open(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "TCS34725 device opened\n");
    return 0;
}

static int tcs34725_release(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "TCS34725 device closed\n");
    return 0;
}

static struct file_operations fops = { 
    .open = tcs34725_open,
    .unlocked_ioctl = tcs34725_ioctl,
    .release = tcs34725_release,
};

static int tcs34725_probe(struct i2c_client *client)
{
    tcs34725_client = client;
    tcs34725_init_sensor(client);

    // Create a char device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ERR "Failed to register a major number\n");
        return major_number;
    }

    tcs34725_class = class_create(CLASS_NAME);
    if (IS_ERR(tcs34725_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to register device class\n");
        return PTR_ERR(tcs34725_class);
    }

    tcs34725_device = device_create(tcs34725_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(tcs34725_device)) {
        class_destroy(tcs34725_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to create the device\n");
        return PTR_ERR(tcs34725_device);
    }

    printk(KERN_INFO "TCS34725 driver installed\n");
    return 0;
}

static void tcs34725_remove(struct i2c_client *client)
{
    device_destroy(tcs34725_class, MKDEV(major_number, 0));
    class_unregister(tcs34725_class);
    class_destroy(tcs34725_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    printk(KERN_INFO "TCS34725 driver removed\n");
}

static const struct of_device_id tcs34725_of_match[] = {
    { .compatible = "taos,tcs34725", },
    { },
};
MODULE_DEVICE_TABLE(of, tcs34725_of_match);

static const struct i2c_device_id tcs34725_id[] = {
    { "tcs34725", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, tcs34725_id);

static struct i2c_driver tcs34725_driver = {
    .driver = {
        .name   = DRIVER_NAME,
        .owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(tcs34725_of_match),
    },
    .probe      = tcs34725_probe,
    .remove     = tcs34725_remove,
    .id_table = tcs34725_id,
};

static int __init tcs34725_init(void)
{
    printk(KERN_INFO "Initializing TCS34725 driver\n");
    return i2c_add_driver(&tcs34725_driver);
}

static void __exit tcs34725_exit(void)
{
    printk(KERN_INFO "Exiting TCS34725 driver\n");
    i2c_del_driver(&tcs34725_driver);
}

module_init(tcs34725_init);
module_exit(tcs34725_exit);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("TCS34725 I2C Client Driver with IOCTL Interface");
MODULE_LICENSE("GPL");
