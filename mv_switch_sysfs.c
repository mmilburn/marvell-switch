/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

This software file (the "File") is owned and distributed by Marvell
International Ltd. and/or its affiliates ("Marvell") under the following
alternative licensing terms.  Once you have made an election to distribute the
File under one of the following license alternatives, please (i) delete this
introductory statement regarding license alternatives, (ii) delete the two
license alternatives that you have not elected to use and (iii) preserve the
Marvell copyright notice above.


********************************************************************************
Marvell GPL License Option

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File in accordance with the terms and conditions of the General
Public License Version 2, June 1991 (the "GPL License"), a copy of which is
available along with the File in the license.txt file or by writing to the Free
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
on the worldwide web at http://www.gnu.org/licenses/gpl.txt.

THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
DISCLAIMED.  The GPL License provides additional details about this warranty
disclaimer.
*******************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/capability.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>



#include <linux/init.h>

#include "common/mvTypes.h"
#include "mv_switch.h"
#include "dsdt/msApiPrototype.h"


static ssize_t mv_switch_help(char *buf)
{
	int off = 0;

	off += sprintf(buf+off, "cat help                            - show this help\n");
	off += sprintf(buf+off, "cat stats                           - show statistics for switch all ports info\n");
	off += sprintf(buf+off, "cat status                          - show switch status\n");
#ifdef CONFIG_MV_ETH_SWITCH
	off += sprintf(buf+off, "echo <eth_name>   > netdev_sts      - print network device status\n");
	off += sprintf(buf+off, "echo <eth_name> p > port_add        - map switch port to a network device\n");
	off += sprintf(buf+off, "echo <eth_name> p > port_del        - unmap switch port from a network device\n");
#endif /* CONFIG_MV_ETH_SWITCH */
	off += sprintf(buf+off, "echo p r t   > reg_r                - read switch register.  t: 1-phy, 2-port, 3-global, 4-global2, 5-smi\n");
	off += sprintf(buf+off, "echo p r t v > reg_w                - write switch register. t: 1-phy, 2-port, 3-global, 4-global2, 5-smi\n");
	return off;
}

static ssize_t mv_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const char *name = attr->attr.name;
	int off = 0;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!strcmp(name, "stats")){
		//mv_switch_stats_print();
	}else if (!strcmp(name, "status")){
		//mv_switch_status_print();
	}else
		off = mv_switch_help(buf);

	return off;
}

static ssize_t mv_switch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	const char      *name = attr->attr.name;
	unsigned long   flags;
	int             err, port, reg, type;
	unsigned int    v;
	unsigned int    val;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	/* Read arguments */
	err = port = reg = type = val = 0;
	sscanf(buf, "%d %d %d %x", &port, &reg, &type, &v);

	local_irq_save(flags);
	if (!strcmp(name, "reg_r")) {
		err = mv_switch_reg_read(port, reg, type, &val);
	} else if (!strcmp(name, "reg_w")) {
		val = (MV_U16)v;
		err = mv_switch_reg_write(port, reg, type, v);
	}
	printk(KERN_ERR "switch register access: type=%d, port=%d, reg=%d", type, port, reg);

	if (err)
		printk(KERN_ERR " - FAILED, err=%d\n", err);
	else
		printk(KERN_ERR " - SUCCESS, val=0x%04x\n", val);

	local_irq_restore(flags);

	return err ? -EINVAL : len;
}

#ifdef CONFIG_MV_ETH_SWITCH
static ssize_t mv_switch_netdev_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	const char      *name = attr->attr.name;
	int             err = 0, port = 0;
	char            dev_name[30];
	struct net_device *netdev;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	/* Read arguments */
	sscanf(buf, "%s %d", dev_name, &port);
	netdev = dev_get_by_name(&init_net, dev_name);
	if (netdev == NULL) {
		err = 1;
	} else {
		if (!strcmp(name, "netdev_sts"))
			mv_eth_netdev_print(netdev);
		else if (!strcmp(name, "port_add"))
			err = mv_eth_switch_port_add(netdev, port);
		else if (!strcmp(name, "port_del"))
			err = mv_eth_switch_port_del(netdev, port);

		dev_put(netdev);
	}

	if (err)
		printk(KERN_ERR " - FAILED, err=%d\n", err);
	else
		printk(KERN_ERR " - SUCCESS\n");

	return err ? -EINVAL : len;
}
#endif /* CONFIG_MV_ETH_SWITCH */

static DEVICE_ATTR(reg_r,       S_IWUSR, mv_switch_show, mv_switch_store);
static DEVICE_ATTR(reg_w,       S_IWUSR, mv_switch_show, mv_switch_store);
static DEVICE_ATTR(status,      S_IRUSR, mv_switch_show, mv_switch_store);
static DEVICE_ATTR(stats,       S_IRUSR, mv_switch_show, mv_switch_store);
static DEVICE_ATTR(help,        S_IRUSR, mv_switch_show, mv_switch_store);
#ifdef CONFIG_MV_ETH_SWITCH
static DEVICE_ATTR(netdev_sts,  S_IWUSR, mv_switch_show, mv_switch_netdev_store);
static DEVICE_ATTR(port_add,    S_IWUSR, mv_switch_show, mv_switch_netdev_store);
static DEVICE_ATTR(port_del,    S_IWUSR, mv_switch_show, mv_switch_netdev_store);
#endif /* CONFIG_MV_ETH_SWITCH */

static struct attribute *mv_switch_attrs[] = {
	&dev_attr_reg_r.attr,
	&dev_attr_reg_w.attr,
	&dev_attr_status.attr,
	&dev_attr_stats.attr,
	&dev_attr_help.attr,
#ifdef CONFIG_MV_ETH_SWITCH
	&dev_attr_netdev_sts.attr,
	&dev_attr_port_add.attr,
	&dev_attr_port_del.attr,
#endif /* CONFIG_MV_ETH_SWITCH */
	NULL
};

static struct attribute_group mv_switch_group = {
	.name = "switch",
	.attrs = mv_switch_attrs,
};

static dev_t  base_dev;
extern GT_QD_DEV qddev; 


char* mv_str_speed_state(int port)
{
    GT_PORT_SPEED_MODE  speed;
    char*               speed_str;

    if(gprtGetSpeedMode(&qddev, port, &speed) != MV_OK) {
	printk("gprtGetSpeedMode failed (port %d)\n", port);
	speed_str = "ERR";
    }
    else
    {
        if (speed == PORT_SPEED_1000_MBPS)
            speed_str = "1000Mbps";
        else if (speed == PORT_SPEED_100_MBPS)
            speed_str = "100Mbps";
        else
            speed_str = "10Mbps";
    }
    return speed_str;
}

char* mv_str_duplex_state(int port)
{
    MV_BOOL duplex;

    if(gprtGetDuplex(&qddev, port, &duplex) != MV_OK) {
        printk("gprtGetDuplex failed (port %d)\n", port);
		return "ERR";
	}
    else 
        return (duplex) ? "Full" : "Half";
}


char* mv_str_link_state(int port)
{
    MV_BOOL link;
    
    if(gprtGetLinkState(&qddev, port, &link) != MV_OK) {
	    printk("gprtGetLinkState failed (port %d)\n", port);
	    return "ERR";
    }
    else 
        return (link) ? "Up" : "Down";
}


static ssize_t mv_carrier_show(struct device *dev, 
			       struct device_attribute *attr, char *buf)
{
    int port_no = MINOR(dev->devt) - MINOR(base_dev);
 
    if (!capable(CAP_NET_ADMIN))
  	return -EPERM;
   
    strcpy(buf, mv_str_link_state(port_no));
    return strlen(buf);
}

static ssize_t mv_link_power_set(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len)
{
    unsigned long state;
    int port_no = MINOR(dev->devt) - MINOR(base_dev);
 
    if (!capable(CAP_NET_ADMIN))
  	return -EPERM;
       
    state = simple_strtoul(buf, NULL, 10);
    gprtPortPowerSet( &qddev, port_no, state != 0 ? MV_TRUE: MV_FALSE);
    return strlen(buf);
}


static ssize_t mv_link_power_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
    int port_no = MINOR(dev->devt) - MINOR(base_dev);
 
    if (!capable(CAP_NET_ADMIN))
  	return -EPERM;
           
    return sprintf(buf, "%d\n", gprtPortPowerGet(&qddev, (GT_LPORT) port_no));
}

static struct device_attribute neta_switch[] = {
	__ATTR(carrier, S_IRUGO, mv_carrier_show, NULL),
	/*can not __ATTR(power) because linux default create the power, can not duplicate */
	__ATTR(power_config, S_IRUSR | S_IWUSR, mv_link_power_show, mv_link_power_set),
	NULL
};

static struct class *neta_switch_class;

int __devinit mv_switch_sysfs_init(void)
{
	int err, i;
	struct device *pd;

	pd = bus_find_device_by_name(&platform_bus_type, NULL, "neta");
	if (!pd) {
		platform_device_register_simple("neta", -1, NULL, 0);
		pd = bus_find_device_by_name(&platform_bus_type, NULL, "neta");
	}

	if (!pd) {
		printk(KERN_ERR "%s: cannot find neta device\n", __func__);
		pd = &platform_bus;
	}

	err = sysfs_create_group(&pd->kobj, &mv_switch_group);
	if (err) {
		printk(KERN_INFO "sysfs group failed %d\n", err);
		goto out;
	}

	err = alloc_chrdev_region(&base_dev, 0, 8, "neta-switch");
	if (err)
		printk(KERN_ERR "Allocate chrdev failed: %d\n", err);
	else {
		neta_switch_class = class_create(THIS_MODULE, "neta-switch");
		neta_switch_class->dev_attrs = neta_switch;

		for (i = 0; i < 8; ++i) {
			device_create(neta_switch_class, pd,
				      MKDEV(MAJOR(base_dev), MINOR(base_dev) + i),
				      NULL, "port%d", i);
		}
	}	
	
out:
	return err;
}

module_init(mv_switch_sysfs_init);

MODULE_AUTHOR("Dima Epshtein");
MODULE_DESCRIPTION("sysfs for Marvell switch");
MODULE_LICENSE("GPL");

