/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/usb/typec.h>

#include "inc/tcpci_typec.h"
#include <mt-plat/charger_class.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/mtk_charger.h>

#define RT_PD_MANAGER_VERSION	"1.0.6_MTK"

struct rt_pd_manager_data {
	struct device *dev;
	struct charger_device *chg_dev;
	struct charger_consumer *chg_consumer;
	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	bool tcpc_kpoc;
	int sink_mv_new;
	int sink_ma_new;
	int sink_mv_old;
	int sink_ma_old;
};

void __attribute__((weak)) usb_dpdm_pulldown(bool enable)
{
	pr_notice("%s is not defined\n", __func__);
}

static int pd_tcp_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	int ret = 0;
	struct tcp_notify *noti = data;
	struct rt_pd_manager_data *rpmd =
		container_of(nb, struct rt_pd_manager_data, pd_nb);
	uint8_t old_state = TYPEC_UNATTACHED, new_state = TYPEC_UNATTACHED;
	enum typec_pwr_opmode opmode = TYPEC_PWR_MODE_USB;

	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
		rpmd->sink_mv_new = noti->vbus_state.mv;
		rpmd->sink_ma_new = noti->vbus_state.ma;
		dev_info(rpmd->dev, "%s sink vbus %dmV %dmA type(0x%02X)\n",
				    __func__, rpmd->sink_mv_new,
				    rpmd->sink_ma_new, noti->vbus_state.type);

		if ((rpmd->sink_mv_new != rpmd->sink_mv_old) ||
		    (rpmd->sink_ma_new != rpmd->sink_ma_old)) {
			rpmd->sink_mv_old = rpmd->sink_mv_new;
			rpmd->sink_ma_old = rpmd->sink_ma_new;
			if (rpmd->sink_mv_new && rpmd->sink_ma_new) {
				charger_manager_enable_power_path(
					rpmd->chg_consumer, MAIN_CHARGER, true);
			} else if (!rpmd->tcpc_kpoc) {
				charger_manager_enable_power_path(
					rpmd->chg_consumer, MAIN_CHARGER,
					false);
			}
		}
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		break;
	case TCP_NOTIFY_PR_SWAP:
		dev_info(rpmd->dev, "%s power role swap, new role = %d\n",
				    __func__, noti->swap_state.new_role);
		break;
	case TCP_NOTIFY_DR_SWAP:
		dev_info(rpmd->dev, "%s data role swap, new role = %d\n",
				    __func__, noti->swap_state.new_role);
		break;
	case TCP_NOTIFY_VCONN_SWAP:
		dev_info(rpmd->dev, "%s vconn role swap, new role = %d\n",
				    __func__, noti->swap_state.new_role);
		break;
	case TCP_NOTIFY_EXT_DISCHARGE:
		dev_info(rpmd->dev, "%s ext discharge = %d\n",
				    __func__, noti->en_state.en);
		charger_dev_enable_discharge(rpmd->chg_dev, noti->en_state.en);
		break;
	case TCP_NOTIFY_PD_STATE:
		dev_info(rpmd->dev, "%s pd state = %d\n",
				    __func__, noti->pd_state.connected);
		break;
	case TCP_NOTIFY_WD_STATUS:
		dev_info(rpmd->dev, "%s wd status = %d\n",
				    __func__, noti->wd_status.water_detected);

		if (noti->wd_status.water_detected) {
			usb_dpdm_pulldown(false);
			if (!rpmd->tcpc_kpoc)
				break;
			dev_info(rpmd->dev, "%s Water is detected in KPOC\n",
					    __func__);
			charger_manager_enable_high_voltage_charging(
					rpmd->chg_consumer, false);
		} else {
			usb_dpdm_pulldown(true);
			if (!rpmd->tcpc_kpoc)
				break;
			dev_info(rpmd->dev, "%s Water is removed in KPOC\n",
					    __func__);
			charger_manager_enable_high_voltage_charging(
					rpmd->chg_consumer, true);
		}
		break;
	case TCP_NOTIFY_CABLE_TYPE:
		dev_info(rpmd->dev, "%s cable type = %d\n",
				    __func__, noti->cable_type.type);
		break;
	default:
		break;
	};
	return NOTIFY_OK;
}


static int rt_pd_manager_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct rt_pd_manager_data *rpmd = NULL;

	dev_info(&pdev->dev, "%s (%s)\n", __func__, RT_PD_MANAGER_VERSION);

	rpmd = devm_kzalloc(&pdev->dev, sizeof(*rpmd), GFP_KERNEL);
	if (!rpmd)
		return -ENOMEM;

	rpmd->dev = &pdev->dev;

	rpmd->chg_dev = get_charger_by_name("primary_chg");
	if (!rpmd->chg_dev) {
		dev_notice(rpmd->dev, "%s get chg dev fail\n", __func__);
		ret = -ENODEV;
		goto err_get_chg_dev;
	}

	rpmd->chg_consumer = charger_manager_get_by_name(rpmd->dev,
							 "charger_port1");
	if (!rpmd->chg_consumer) {
		dev_notice(rpmd->dev, "%s get chg consumer fail\n", __func__);
		ret = -ENODEV;
		goto err_get_chg_consumer;
	}

	rpmd->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!rpmd->tcpc) {
		dev_notice(rpmd->dev, "%s get tcpc dev fail\n", __func__);
		ret = -ENODEV;
		goto err_get_tcpc_dev;
	}

	ret = get_boot_mode();
	if (ret == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    ret == LOW_POWER_OFF_CHARGING_BOOT)
		rpmd->tcpc_kpoc = true;
	else
		rpmd->tcpc_kpoc = false;
	dev_info(rpmd->dev, "%s tcpc_kpoc = %d\n", __func__, rpmd->tcpc_kpoc);

	rpmd->sink_mv_old = -1;
	rpmd->sink_ma_old = -1;

	rpmd->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(rpmd->tcpc, &rpmd->pd_nb,
					TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		dev_notice(rpmd->dev, "%s register tcpc notifier fail(%d)\n",
				      __func__, ret);
	}

	platform_set_drvdata(pdev, rpmd);
	dev_info(rpmd->dev, "%s OK!!\n", __func__);
	return 0;
err_get_tcpc_dev:
err_get_chg_consumer:
err_get_chg_dev:
	return ret;
}

static int rt_pd_manager_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct rt_pd_manager_data *rpmd = platform_get_drvdata(pdev);

	if (!rpmd)
		return -EINVAL;

	ret = unregister_tcp_dev_notifier(rpmd->tcpc, &rpmd->pd_nb,
					  TCP_NOTIFY_TYPE_ALL);
	if (ret < 0)
		dev_notice(rpmd->dev, "%s unregister tcpc notifier fail(%d)\n",
				      __func__, ret);
	return ret;
}

static const struct of_device_id rt_pd_manager_of_match[] = {
	{ .compatible = "mediatek,rt-pd-manager" },
	{ }
};
MODULE_DEVICE_TABLE(of, rt_pd_manager_of_match);

static struct platform_driver rt_pd_manager_driver = {
	.driver = {
		.name = "rt-pd-manager",
		.of_match_table = of_match_ptr(rt_pd_manager_of_match),
	},
	.probe = rt_pd_manager_probe,
	.remove = rt_pd_manager_remove,
};

static int __init rt_pd_manager_init(void)
{
	return platform_driver_register(&rt_pd_manager_driver);
}
late_initcall(rt_pd_manager_init);

static void __exit rt_pd_manager_exit(void)
{
	platform_driver_unregister(&rt_pd_manager_driver);
}
module_exit(rt_pd_manager_exit);

MODULE_AUTHOR("Jeff Chang");
MODULE_DESCRIPTION("Richtek pd manager driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(RT_PD_MANAGER_VERSION);

/*
 * Release Note
 * 1.0.6
 * (1) Register typec_port
 * (2) Remove unused parts
 * (3) Add rt_pd_manager_remove()
 */
