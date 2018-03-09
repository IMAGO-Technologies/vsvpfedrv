/*
 * VCDrv.h
 *
 * Copyright (C) 201x IMAGO Technologies GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.*
 */

#ifndef VCDRV_H_
#define VCDRV_H_

//> defines about the Module
/******************************************************************************************/
#define MODVERSION "0.0.0.5"
#define MODDATECODE __DATE__ " - " __TIME__
#define MODLICENSE "GPL";
#define MODDESCRIPTION "Kernel module for the VisionCam VCUX VPFE(e) devices";
#define MODAUTHOR "IMAGO Technologies GmbH";

#define MODCLASSNAME	"vcdrv"
#define MODMODULENAME	"vcvpfedrv"
#define MODDEBUGOUTTEXT	"vcvpfedrv:"


/*** includes ***/
/******************************************************************************************/
//aus usr/src/linux-headers-2.6.38-8-generic/include
#include <linux/init.h>		// für module_init(),
#include <linux/module.h>	// für MODULE_LICENSE
#include <linux/version.h>	// für die Version

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32)
	#include <linux/printk.h>	// für printk
#endif

#include <linux/types.h>		// für dev_t
#include <asm/types.h>			// für u8, s8
#include <linux/sched.h>		// für current (pointer to the current process)
#include <linux/fs_struct.h>
#include <linux/kdev_t.h>		// für MAJOR/MINOR
#include <linux/cdev.h>			// für cdev_*
#include <linux/device.h>		// für class_create
#include <linux/fs.h>			// für alloc_chrdev_region /file_*
#include <linux/semaphore.h>	// für up/down ...
#include <linux/kfifo.h>		// für kfifo_*
#include <linux/pci.h>			// für pci*
#include <linux/ioport.h>		// für resource*
#include <linux/interrupt.h>	// für IRQ*
#include <linux/dma-mapping.h>	//für dma_*
#include <linux/scatterlist.h>	// sg_* ...
#include <linux/spinlock.h>		// spin_* ...
#include <linux/delay.h>		// für usleep_range
#include <linux/platform_device.h>	// für platform_driver ...
#include <linux/pm_runtime.h>	//für pm_runtim_*
#if LINUX_VERSION_CODE != KERNEL_VERSION(2,6,32)
	#include <asm/uaccess.h>	// für copy_to_user
#else
	#include <linux/uaccess.h>	// für copy_to_user
#endif

#include "am437x-vpfe_regs.h"



/*** defines ***/
/******************************************************************************************/
typedef u8 IOCTLBUFFER[128];
#define FALSE 0
#define TRUE 1

#define MIN(x,y) (x < y ? x : y)
#define MAX(x,y) (x > y ? x : y)


#define VCDRV_STATE_UNUSED	0x00	//!< Struct vor dem Init bzw. nach dem UnInit
#define VCDRV_STATE_PREINIT	0x01	//!< FIFOs /SEM erzeugt… alle Feler gültig
#define VCDRV_STATE_RUNNING	0x02	//!< AOI wurde im VPFE gesetzt, kann nicht geändert werden, allco möglich




//> Ioctl definitions siehe. "ioctl-number.txt"
/******************************************************************************************/
//magic number
#define VCDRV_IOC_MAGIC  '['

//richtung ist aus UserSicht, size ist ehr der Type 
#define VCDRV_IOC_DRV_GET_VERSION 		_IOR(VCDRV_IOC_MAGIC, 0, IOCTLBUFFER)
#define VCDRV_IOC_DRV_GET_BUILD_DATE 	_IOR(VCDRV_IOC_MAGIC, 1, IOCTLBUFFER)
#define VCDRV_IOC_DRV_SET_AOI		 	_IOW(VCDRV_IOC_MAGIC, 2, IOCTLBUFFER)

#define VCDRV_IOC_VPFE_START	 		_IOW(VCDRV_IOC_MAGIC, 3, u8)
#define VCDRV_IOC_VPFE_ABORT 			_IOW(VCDRV_IOC_MAGIC, 4, u8)
#define VCDRV_IOC_VPFE_ADD_BUFFER	 	_IOW(VCDRV_IOC_MAGIC, 5, IOCTLBUFFER)

#define VCDRV_IOC_BUFFER_ALLOC			_IOW(VCDRV_IOC_MAGIC, 6, IOCTLBUFFER)
#define VCDRV_IOC_BUFFER_FREE			_IOW(VCDRV_IOC_MAGIC, 7, IOCTLBUFFER)
#define VCDRV_IOC_BUFFER_WAIT_FOR		_IOWR(VCDRV_IOC_MAGIC,8, IOCTLBUFFER)

//max num (nur zum Testen)
#define VCDRV_IOC_MAXNR 8


//> Infos über die ...
/******************************************************************************************/
//Anzahl der Elemente im .Jobs_ToTo, .Jobs_Done FIFO
#define MAX_VPFE_JOBFIFO_SIZE	32
//es wird im Moment nur eine VPFE Unit unterstützt
#define MAX_DEVICE_COUNT		1

//defaults
#define VPFE_DEFAULT_Width 			640
#define VPFE_DEFAULT_VPFE_Height	480
#define VPFE_DEFAULT_VPFE_Is16BitPixel TRUE




/*** structs ***/
/******************************************************************************************/

//Fast alles zusammen für ein UserBuffer für die VPFE
typedef struct _VPFE_JOB
{
	//UserBuffer
 	uintptr_t 	pDMA;				//NULL ptr, dann ein DummyJob vom Abort

	//Status (nur gültig/gesetzt wenn in .Jobs_Done)
	bool		boIsOk;				//ohne Fehler übertragen
	u32 		ISRCounter; 		//laufender Zähler, wird im ISR gezählt
}  VPFE_JOB, *PVPFE_JOB;


//Fast alle Infos zu einem VPFE Device zusammen
typedef struct _DEVICE_DATA
{		
	//> Device	
	//***************************************************************/
	u8					VCDrv_State;		//Zustand des Devices
	struct cdev			VCDrv_CDev;			//das KernelObj vom Module	
	bool				VCDrv_IsCDevOpen;	//VCDrv_CDev gültig
	struct device*		VCDrv_pDeviceDevice;//platform_device::dev
	spinlock_t 			VCDrv_SpinLock;		//lock für ein Device (IRQSave)
	dev_t				VCDrv_DeviceNumber;	//Nummer von CHAR device

	//> FIFO
	//***************************************************************/
	struct completion 	FIFO_Waiter;								//'sem' um auf ein Element in FIFO_JobsDone zu warten
	DECLARE_KFIFO(FIFO_JobsToDo, VPFE_JOB, MAX_VPFE_JOBFIFO_SIZE);	//sind noch nicht in der VPFE Unit
	DECLARE_KFIFO(FIFO_JobsDone, VPFE_JOB, MAX_VPFE_JOBFIFO_SIZE);	//sind nicht mehr in der VPFE Unit (kann aber auch abgebrochen worden sein)
	
	//> VPFE
	//***************************************************************/
	void __iomem *VPFE_CCDC_BaseAddr;
	u32			VPFE_ISRCounter;	//wird bei jedem „FrameStart“ ISR hochgezählt
	u32 		VPFE_Width;
	u32	 		VPFE_Height;
	bool 		VPFE_Is16BitPixel;
	bool		VPFE_IsStartFrameDone;	//sind wir in einem Bild?

} DEVICE_DATA, *PDEVICE_DATA;

//Fast alles zusammen was zu diesem Module gehört, (Note: n VPFEdevs für das Module, Module wird nur 1x geladen)
typedef struct _MODULE_DATA
{
	DEVICE_DATA		Devs[MAX_DEVICE_COUNT];			//Infos/Context für je device, Index ist der Minor
	bool			boIsMinorUsed[MAX_DEVICE_COUNT];//daher ist Devs[Minor] benutzt/frei?

	dev_t 			FirstDeviceNumber;				//MAJOR(devNumber),MINOR(devNumber) (eg 240 , 0)
	struct class	*pModuleClass;					// /sys/class/*

} MODULE_DATA, *PMODULE_DATA;


//Global vars
extern MODULE_DATA _ModuleData;


/*** prototypes ***/
/******************************************************************************************/
/* Module fns */
int VCDrv_init(void);
void VCDrv_exit(void);
void VCDrv_InitDrvData(PDEVICE_DATA pDevData);

/* AM473X fns */
int VCDrv_AM473X_probe(struct platform_device *pdev);
int VCDrv_AM473X_remove(struct platform_device *pdev);


/* ~IRQ fns */
irqreturn_t VCDrv_VPFE_interrupt(int irq, void *dev);


/* File fns */
int VCDrv_open(struct inode *node, struct file *filp);
ssize_t VCDrv_read (struct file *filp, char __user *buf, size_t count, loff_t *pos);
ssize_t VCDrv_write (struct file *filp, const char __user *buf, size_t count,loff_t *pos);
long VCDrv_unlocked_ioctl (struct file *filp, unsigned int cmd,unsigned long arg);


/* VPFE fns */
int VCDrv_VPFE_Configure(PDEVICE_DATA pDevData);
int VCDrv_VPFE_AddBuffer(PDEVICE_DATA pDevData, dma_addr_t pDMAKernelBuffer);
void VCDrv_VPFE_TryToAddNextBuffer_locked(PDEVICE_DATA pDevData);
int VCDrv_VPFE_Abort(PDEVICE_DATA pDevData);


/* Buffer fns */
int VCDrv_BUF_Alloc(PDEVICE_DATA pDevData, void** ppVMKernel, dma_addr_t *ppDMAKernel, size_t *panzBytes);
void VCDrv_BUF_Free(PDEVICE_DATA pDevData, void* pVMKernel, dma_addr_t pDMAKernel, size_t anzBytes);
int VCDrv_BUF_mmap(struct file *, struct vm_area_struct *);
int VCDrv_BUF_WaitFor(PDEVICE_DATA pDevData, const u32 TimeOut_ms, u32 *pIsBroken, u32 *pImageNumber, dma_addr_t *ppDMAKernel);

#endif /* VCDRV_H_ */

