/*
 * Bond several ethernet interfaces into a Cisco, running 'Etherchannel'.
 *
 * Portions are (c) Copyright 1995 Simon "Guru Aleph-Null" Janes
 * NCM: Network and Communications Management, Inc.
 *
 * BUT, I'm the one who modified it for ethernet, so:
 * (c) Copyright 1999, Thomas Davis, tadavis@lbl.gov
 *
 *	This software may be used and distributed according to the terms
 *	of the GNU Public License, incorporated herein by reference.
 *
 */

#ifndef _LINUX_BONDING_H
#define _LINUX_BONDING_H

#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/if_bonding.h>
#include <linux/kobject.h>
#include "bond_3ad.h"
#include "bond_alb.h"

#define DRV_VERSION	"3.2.3"
#define DRV_RELDATE	"December 6, 2007"
#define DRV_NAME	"bonding"
#define DRV_DESCRIPTION	"Ethernet Channel Bonding Driver"

#define BOND_MAX_ARP_TARGETS	16

#ifdef BONDING_DEBUG
#define dprintk(fmt, args...) \
	printk(KERN_DEBUG     \
	       DRV_NAME ": %s() %d: " fmt, __FUNCTION__, __LINE__ , ## args )
#else
#define dprintk(fmt, args...)
#endif /* BONDING_DEBUG */

#define IS_UP(dev)					   \
	      ((((dev)->flags & IFF_UP) == IFF_UP)	&& \
	       netif_running(dev)			&& \
	       netif_carrier_ok(dev))

/*
 * Checks whether bond is ready for transmit.
 *
 * Caller must hold bond->lock
 */
#define BOND_IS_OK(bond)			     \
		   (((bond)->dev->flags & IFF_UP) && \
		    netif_running((bond)->dev)	  && \
		    ((bond)->slave_cnt > 0))

/*
 * Checks whether slave is ready for transmit.
 */
#define SLAVE_IS_OK(slave)			        \
		    (((slave)->dev->flags & IFF_UP)  && \
		     netif_running((slave)->dev)     && \
		     ((slave)->link == BOND_LINK_UP) && \
		     ((slave)->state == BOND_STATE_ACTIVE))


#define USES_PRIMARY(mode)				\
		(((mode) == BOND_MODE_ACTIVEBACKUP) ||	\
		 ((mode) == BOND_MODE_TLB)          ||	\
		 ((mode) == BOND_MODE_ALB))

/*
 * Less bad way to call ioctl from within the kernel; this needs to be
 * done some other way to get the call out of interrupt context.
 * Needs "ioctl" variable to be supplied by calling context.
 */
#define IOCTL(dev, arg, cmd) ({		\
	int res = 0;			\
	mm_segment_t fs = get_fs();	\
	set_fs(get_ds());		\
	res = ioctl(dev, arg, cmd);	\
	set_fs(fs);			\
	res; })

/**
 * bond_for_each_slave_from - iterate the slaves list from a starting point
 * @bond:	the bond holding this list.
 * @pos:	current slave.
 * @cnt:	counter for max number of moves
 * @start:	starting point.
 *
 * Caller must hold bond->lock
 */
#define bond_for_each_slave_from(bond, pos, cnt, start)	\
	for (cnt = 0, pos = start;				\
	     cnt < (bond)->slave_cnt;				\
             cnt++, pos = (pos)->next)

/**
 * bond_for_each_slave_from_to - iterate the slaves list from start point to stop point
 * @bond:	the bond holding this list.
 * @pos:	current slave.
 * @cnt:	counter for number max of moves
 * @start:	start point.
 * @stop:	stop point.
 *
 * Caller must hold bond->lock
 */
#define bond_for_each_slave_from_to(bond, pos, cnt, start, stop)	\
	for (cnt = 0, pos = start;					\
	     ((cnt < (bond)->slave_cnt) && (pos != (stop)->next));	\
             cnt++, pos = (pos)->next)

/**
 * bond_for_each_slave - iterate the slaves list from head
 * @bond:	the bond holding this list.
 * @pos:	current slave.
 * @cnt:	counter for max number of moves
 *
 * Caller must hold bond->lock
 */
#define bond_for_each_slave(bond, pos, cnt)	\
		bond_for_each_slave_from(bond, pos, cnt, (bond)->first_slave)


struct bond_params {
	int mode;
	int xmit_policy;
	int miimon;
	int arp_interval;
	int arp_validate;
	int use_carrier;
	int fail_over_mac;
	int updelay;
	int downdelay;
	int lacp_fast;
	char primary[IFNAMSIZ];
	__be32 arp_targets[BOND_MAX_ARP_TARGETS];
};

struct bond_parm_tbl {
	char *modename;
	int mode;
};

#define BOND_MAX_MODENAME_LEN 20

struct vlan_entry {
	struct list_head vlan_list;
	__be32 vlan_ip;
	unsigned short vlan_id;
};

struct slave {
	struct net_device *dev; /* first - useful for panic debug */
	struct slave *next;
	struct slave *prev;
	int    delay;
	unsigned long jiffies;
	unsigned long last_arp_rx;
	s8     link;    /* one of BOND_LINK_XXXX */
	s8     state;   /* one of BOND_STATE_XXXX */
	u32    original_flags;
	u32    original_mtu;
	u32    link_failure_count;
	u16    speed;
	u8     duplex;
	u8     perm_hwaddr[ETH_ALEN];
	struct ad_slave_info ad_info; /* HUGE - better to dynamically alloc */
	struct tlb_slave_info tlb_info;
};

/*
 * Here are the locking policies for the two bonding locks:
 *
 * 1) Get bond->lock when reading/writing slave list.
 * 2) Get bond->curr_slave_lock when reading/writing bond->curr_active_slave.
 *    (It is unnecessary when the write-lock is put with bond->lock.)
 * 3) When we lock with bond->curr_slave_lock, we must lock with bond->lock
 *    beforehand.
 */
struct bonding {
	struct   net_device *dev; /* first - useful for panic debug */
	struct   slave *first_slave;
	struct   slave *curr_active_slave;
	struct   slave *current_arp_slave;
	struct   slave *primary_slave;
	s32      slave_cnt; /* never change this value outside the attach/detach wrappers */
	rwlock_t lock;
	rwlock_t curr_slave_lock;
	s8       kill_timers;
	s8	 send_grat_arp;
	s8	 setup_by_slave;
	struct   net_device_stats stats;
#ifdef CONFIG_PROC_FS
	struct   proc_dir_entry *proc_entry;
	char     proc_file_name[IFNAMSIZ];
#endif /* CONFIG_PROC_FS */
	struct   list_head bond_list;
	struct   dev_mc_list *mc_list;
	int      (*xmit_hash_policy)(struct sk_buff *, struct net_device *, int);
	__be32   master_ip;
	u16      flags;
	u16      rr_tx_counter;
	struct   ad_bond_info ad_info;
	struct   alb_bond_info alb_info;
	struct   bond_params params;
	struct   list_head vlan_list;
	struct   vlan_group *vlgrp;
	struct   packet_type arp_mon_pt;
	struct   workqueue_struct *wq;
	struct   delayed_work mii_work;
	struct   delayed_work arp_work;
	struct   delayed_work alb_work;
	struct   delayed_work ad_work;
	struct   packet_type hoolock_pt;

	/* packet buffering */
	int	buffer_mode; /* Hoolock buffer flag */
	struct sk_buff_head	pbuff;
};

/**
 * Returns NULL if the net_device does not belong to any of the bond's slaves
 *
 * Caller must hold bond lock for read
 */
static inline struct slave *bond_get_slave_by_dev(struct bonding *bond, struct net_device *slave_dev)
{
	struct slave *slave = NULL;
	int i;

	bond_for_each_slave(bond, slave, i) {
		if (slave->dev == slave_dev) {
			break;
		}
	}

	return slave;
}

static inline struct bonding *bond_get_bond_by_slave(struct slave *slave)
{
	if (!slave || !slave->dev->master) {
		return NULL;
	}

	return (struct bonding *)slave->dev->master->priv;
}

#define BOND_ARP_VALIDATE_NONE		0
#define BOND_ARP_VALIDATE_ACTIVE	(1 << BOND_STATE_ACTIVE)
#define BOND_ARP_VALIDATE_BACKUP	(1 << BOND_STATE_BACKUP)
#define BOND_ARP_VALIDATE_ALL		(BOND_ARP_VALIDATE_ACTIVE | \
					 BOND_ARP_VALIDATE_BACKUP)

static inline int slave_do_arp_validate(struct bonding *bond,
					struct slave *slave)
{
	return bond->params.arp_validate & (1 << slave->state);
}

static inline unsigned long slave_last_rx(struct bonding *bond,
					struct slave *slave)
{
	if (slave_do_arp_validate(bond, slave))
		return slave->last_arp_rx;

	return slave->dev->last_rx;
}

static inline void bond_set_slave_inactive_flags(struct slave *slave)
{
	struct bonding *bond = slave->dev->master->priv;
	if (bond->params.mode != BOND_MODE_TLB &&
	    bond->params.mode != BOND_MODE_ALB)
		slave->state = BOND_STATE_BACKUP;
	slave->dev->priv_flags |= IFF_SLAVE_INACTIVE;
	if (slave_do_arp_validate(bond, slave))
		slave->dev->priv_flags |= IFF_SLAVE_NEEDARP;
}

static inline void bond_set_slave_active_flags(struct slave *slave)
{
	slave->state = BOND_STATE_ACTIVE;
	slave->dev->priv_flags &= ~(IFF_SLAVE_INACTIVE | IFF_SLAVE_NEEDARP);
}

static inline void bond_set_master_3ad_flags(struct bonding *bond)
{
	bond->dev->priv_flags |= IFF_MASTER_8023AD;
}

static inline void bond_unset_master_3ad_flags(struct bonding *bond)
{
	bond->dev->priv_flags &= ~IFF_MASTER_8023AD;
}

static inline void bond_set_master_alb_flags(struct bonding *bond)
{
	bond->dev->priv_flags |= IFF_MASTER_ALB;
}

static inline void bond_unset_master_alb_flags(struct bonding *bond)
{
	bond->dev->priv_flags &= ~IFF_MASTER_ALB;
}

struct vlan_entry *bond_next_vlan(struct bonding *bond, struct vlan_entry *curr);
int bond_dev_queue_xmit(struct bonding *bond, struct sk_buff *skb, struct net_device *slave_dev);
int bond_create(char *name, struct bond_params *params, struct bonding **newbond);
void bond_destroy(struct bonding *bond);
int  bond_release_and_destroy(struct net_device *bond_dev, struct net_device *slave_dev);
int bond_create_sysfs(void);
void bond_destroy_sysfs(void);
void bond_destroy_sysfs_entry(struct bonding *bond);
int bond_create_sysfs_entry(struct bonding *bond);
int bond_create_slave_symlinks(struct net_device *master, struct net_device *slave);
void bond_destroy_slave_symlinks(struct net_device *master, struct net_device *slave);
int bond_enslave(struct net_device *bond_dev, struct net_device *slave_dev);
int bond_release(struct net_device *bond_dev, struct net_device *slave_dev);
void bond_mii_monitor(struct work_struct *);
void bond_loadbalance_arp_mon(struct work_struct *);
void bond_activebackup_arp_mon(struct work_struct *);
void bond_set_mode_ops(struct bonding *bond, int mode);
int bond_parse_parm(const char *mode_arg, struct bond_parm_tbl *tbl);
void bond_select_active_slave(struct bonding *bond);
void bond_change_active_slave(struct bonding *bond, struct slave *new_active);
void bond_register_arp(struct bonding *);
void bond_unregister_arp(struct bonding *);

/* Hoolock specific functions */

int bond_hoolock_recv(struct sk_buff *skb, struct net_device *dev, struct packet_type* ptype, struct net_device *orig_dev);

#endif /* _LINUX_BONDING_H */

