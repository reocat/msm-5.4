/********************************************************************************/
/** @file		servicebus_if_ioctl.h
	@brief		interface
	@note		Copyright (C) 2013 DENSO CORPORATION. All rights reserved.
	@author		M.Hayakawa
	@date		2013/10/09
	@version	1.0
*********************************************************************************/
#if !defined(_SERVICEBUS_IF_IOCTL__INCLUDED_)
#define _SERVICEBUS_IF_IOCTL__INCLUDED_

/*--------------------------------------------------------------------------------
	Include
--------------------------------------------------------------------------------*/
#include <linux/ioctl.h>														// ioctl include

/*--------------------------------------------------------------------------------
	svc_bus_ioctl
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
 *	@brief		ioctrl callback for ioctl
 *	@param[in]	file *filp			: 
 *	@param[in]	unsigned int cmd	: 
 *	@param[in]	unsigned long arg	: 
 *	@return		int 0...OK, -ENOSYS...NG
 */
long svc_bus_ioctl(struct file* filp, unsigned int cmd, unsigned long arg);

/*--------------------------------------------------------------------------------
	svc_bus_ioctl_open
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_ioctl_open(struct inode* inode, struct file* filp)
 *	@brief		ioctrl callback for open
 *	@param[in]	inode *inode		: 
 *	@param[in]	file *filp			: 
 *	@return		int 0...OK, -ENOSYS...NG
 */
int svc_bus_ioctl_open(struct inode* inode, struct file* filp);

/*--------------------------------------------------------------------------------
	svc_bus_ioctl_close
--------------------------------------------------------------------------------*/
/**
 *	@fn			int svc_bus_ioctl_close(struct inode* inode, struct file* filp)
 *	@brief		ioctrl callback for close
 *	@param[in]	inode *inode		: 
 *	@param[in]	file *filp			: 
 *	@return		int 0...OK, -ENOSYS...NG
 */
int svc_bus_ioctl_close(struct inode* inode, struct file* filp);

#endif // _SERVICEBUS_IF_IOCTL__INCLUDED_
