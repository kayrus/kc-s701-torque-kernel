/* 
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2012 KYOCERA Corporation
 * (C) 2014 KYOCERA Corporation
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <mach/pmic.h>
#include <mach/camera.h>
#include <mach/gpio.h>

#define ERR_LOG_SWITCH
//#undef ERR_LOG_SWITCH
#ifdef ERR_LOG_SWITCH
#define ERR_LOG(fmt, args...) printk(KERN_ERR fmt, ##args)
#else
#define ERR_LOG(fmt, args...) do{ } while(0)
#endif

#define API_LOG_SWITCH
#undef API_LOG_SWITCH
#ifdef API_LOG_SWITCH
#define API_LOG(fmt, args...) printk(KERN_INFO fmt, ##args)
#else
#define API_LOG(fmt, args...) do{ } while(0)
#endif

extern void camera_flash_test_mode_led_trigger(int mode);

static struct class *msm_camera_flash_class;

struct camera_flash_dev {
  const char *name;

  void (*enable)(struct camera_flash_dev *sdev, int timeout);

  int (*get)(struct camera_flash_dev *sdev);

  /* private data */
  struct device *dev;
  int    index;
  int    state;
};

/*********************************************************
                     CameraFlash   Class
**********************************************************/
static int cabl_mode = 0;
static void camrea_flash_set_enable(struct camera_flash_dev *dev, int value)
{
    API_LOG("%s(%d) \n", __func__, value);

    if( value == 1 )
    {
        camera_flash_test_mode_led_trigger(MSM_CAMERA_LED_LOW);
    }
    else
    {
        camera_flash_test_mode_led_trigger(MSM_CAMERA_LED_OFF);
    }
}

static int camrea_flash_get_select(struct camera_flash_dev *dev)
{
  API_LOG("%s() \n", __func__);
  return(cabl_mode);
}

static struct camera_flash_dev camera_flash_set = {
  .name     = "camera_flash_set",
  .enable   = camrea_flash_set_enable,
  .get      = camrea_flash_get_select,
};
/*********************************************************/

static atomic_t device_count;

static ssize_t camera_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct camera_flash_dev *tdev = dev_get_drvdata(dev);
  int remaining = tdev->get(tdev);

  return sprintf(buf, "%d\n", remaining);
}

static ssize_t camera_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  struct camera_flash_dev *tdev = dev_get_drvdata(dev);
  int value;

  if (sscanf(buf, "%d", &value) != 1){
    return -EINVAL;
  }

  tdev->enable(tdev, value);

  return size;
}

static DEVICE_ATTR(camera_flash, S_IRUGO | S_IWUSR, camera_flash_show, camera_flash_store);

int camera_flash_register(struct camera_flash_dev *tdev)
{
  int ret;

  if (!tdev || !tdev->name || !tdev->enable || !tdev->get){
    return -EINVAL;
  }

  tdev->index = atomic_inc_return(&device_count);
  tdev->dev = device_create(msm_camera_flash_class, NULL, MKDEV(0, tdev->index), NULL, tdev->name);
  if (IS_ERR(tdev->dev)){
    return PTR_ERR(tdev->dev);
  }

  ret = device_create_file(tdev->dev, &dev_attr_camera_flash);
  if (ret < 0){
    goto err_create_file;
  }

  dev_set_drvdata(tdev->dev, tdev);
  tdev->state = 0;
  return 0;

err_create_file:
  device_destroy(msm_camera_flash_class, MKDEV(0, tdev->index));
  ERR_LOG(KERN_ERR "cabc: Failed to register driver %s\n", tdev->name);

  return ret;
}

static int create_camera_flash_class(void)
{
  if (!msm_camera_flash_class) {
    msm_camera_flash_class = class_create(THIS_MODULE, "msm_camera_flash");
    if (IS_ERR(msm_camera_flash_class)){
      return PTR_ERR(msm_camera_flash_class);
    }
    atomic_set(&device_count, 0);
  }

  camera_flash_register(&camera_flash_set);
  return 0;
}

EXPORT_SYMBOL_GPL(camera_flash_register);

void camera_flash_unregister(struct camera_flash_dev *tdev)
{
  device_remove_file(tdev->dev, &dev_attr_camera_flash);
  device_destroy(msm_camera_flash_class, MKDEV(0, tdev->index));
  dev_set_drvdata(tdev->dev, NULL);
}
EXPORT_SYMBOL_GPL(camera_flash_unregister);

static int __init camera_flash_init(void)
{
    return create_camera_flash_class();
}

static void __exit camera_flash_exit(void)
{
    class_destroy(msm_camera_flash_class);
}

module_init(camera_flash_init);
module_exit(camera_flash_exit);

MODULE_AUTHOR("kyocera");
MODULE_DESCRIPTION("camera_flash class driver");
MODULE_LICENSE("GPL");

