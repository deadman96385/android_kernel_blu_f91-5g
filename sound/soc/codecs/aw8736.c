/*******************************************************************************
dts sample
&odm {
	ext_speaker {
		compatible = "awinic,aw8736";
		mode = <CONFIG_SND_SOC_AW8736_MODE>;
		en-gpio = <&pio 132 0>;
		pn = "aw8736";
	};
};
*******************************************************************************/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#define PRINTK_I(fmt,args...) printk(KERN_INFO"aw8736 %s:"fmt,__func__,##args)
#define PRINTK_E(fmt,args...) printk(KERN_ERR"aw8736 %s:"fmt,__func__,##args)


static int extspk_mode = 3;//CONFIG_SND_SOC_AW8736_MODE;
module_param(extspk_mode,int,0644);

enum {
	AW8736,
	AW8155,
	AW8733,
};

struct aw8736_data {
	int en_gpio;
	int mode;
	int pn;
	struct mutex mutex;
};

struct aw8736_data *aw8736;


int aw8736_spk_enable_set(int enable){

	int extamp_mode = extspk_mode;
	int i = 0;
	int retval = 0;

	mutex_lock(&aw8736->mutex);
	if (gpio_is_valid(aw8736->en_gpio)){
		if (enable){
			for (i = 0; i < extamp_mode; i++) {
				retval = gpio_direction_output(aw8736->en_gpio, 0);
				if (retval){
					PRINTK_E("could not set aw8736 en pins\n");
				}
				udelay(2);
				retval = gpio_direction_output(aw8736->en_gpio, 1);
				if (retval){
					PRINTK_E("could not set aw8736 en pins\n");
				}
				udelay(2);
			}
		}else{
			retval = gpio_direction_output(aw8736->en_gpio, 0);
			if (retval){
				PRINTK_E("could not set aw8736 en pins\n");
			}
			//prize added by huarui, audio pa ctl, 20190111-start
			if (aw8736->pn == AW8736){
				udelay(500);
			}else{
				udelay(250);
			}
			//prize added by huarui, audio pa ctl, 20190111-end
		}
	}
	mutex_unlock(&aw8736->mutex);

	return 0;
}

static int aw8736_probe(struct platform_device *pdev)
{
	struct aw8736_data *pdata = dev_get_platdata(&pdev->dev);
	int err = 0;
	const char *pn = NULL;

	PRINTK_I("Probe start.\n");

	/* init platform data */
	if (!pdata) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			PRINTK_E("alloc mem fail\n");
			err = -ENOMEM;
		}
	}

	if (pdev->dev.of_node){

		if (of_property_read_u32(pdev->dev.of_node, "mode", &pdata->mode)){
			PRINTK_E("get mode(%d) fail, use default 3\n",pdata->mode);
			pdata->mode = 3;
		}else{
			PRINTK_I("get mode(%d) success\n",pdata->mode);
		}
		extspk_mode = pdata->mode;

		if (of_property_read_string(pdev->dev.of_node, "pn", &pn) < 0) {
			PRINTK_E("get pn fail, default aw8736\n");
			pdata->pn = AW8736;
		}else{
			if (strcmp(pn,"aw8736") == 0){
				pdata->pn = AW8736;
			}else if (strcmp(pn,"aw8155") == 0){
				pdata->pn = AW8155;
			}else if (strcmp(pn,"aw8733") == 0){
				pdata->pn = AW8733;
			}
			PRINTK_I("get pn(%s:%d) success\n",pn,pdata->pn);
		}

		pdata->en_gpio = of_get_named_gpio(pdev->dev.of_node, "en-gpio", 0);
		if (gpio_is_valid(pdata->en_gpio)){
			PRINTK_I("get en-gpio(%d) success\n",pdata->en_gpio);
			gpio_request(pdata->en_gpio, "aw8736_en");
			gpio_direction_output(pdata->en_gpio, 0);
		}else{
			PRINTK_E("get en-gpio(%d) fail\n",pdata->en_gpio);
			err = -EINVAL;
			goto err_gpio;
		}
	}else{
		PRINTK_E("null of node\n",pdata->en_gpio);
		err = -EINVAL;
		goto err_gpio;
	}

	mutex_init(&pdata->mutex);

	aw8736 = pdata;

	return 0;
err_gpio:
	kfree(pdata);
	return err;
}

static int aw8736_remove(struct platform_device *pdev)
{
	struct aw8736_data *pdata = dev_get_platdata(&pdev->dev);

	PRINTK_I("Remove start.\n");

	if (!IS_ERR_OR_NULL(pdata)){
		if (gpio_is_valid(pdata->en_gpio)){
			gpio_free(pdata->en_gpio);
		}
		pdev->dev.platform_data = NULL;
		kfree(pdata);
	}

	return 0;
}

static const struct of_device_id aw8736_of_match[] = {
	{.compatible = "awinic,aw8736"},
	{},
};
MODULE_DEVICE_TABLE(of, aw8736_of_match);

static struct platform_driver aw8736_platform_driver = {
	.probe = aw8736_probe,
	.remove = aw8736_remove,
	.driver = {
		.name = "AW8736",
		.owner = THIS_MODULE,
		.of_match_table = aw8736_of_match,
	},
};

static int __init aw8736_init(void)
{
	int ret;

	PRINTK_I("Init start.\n");

	ret = platform_driver_register(&aw8736_platform_driver);
	if (ret) {
		PRINTK_E("Failed to register platform driver\n");
		return ret;
	}

	PRINTK_I("Init done.\n");

	return 0;
}

static void __exit aw8736_exit(void)
{
	PRINTK_I("Exit start.\n");

	platform_driver_unregister(&aw8736_platform_driver);

	PRINTK_I("Exit done.\n");
}

module_init(aw8736_init);
module_exit(aw8736_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<huarui@szprize.com>");