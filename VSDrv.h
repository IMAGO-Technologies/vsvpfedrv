/*
 * VSDrv.h
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

#ifndef VSDRV_H_
#define VSDRV_H_

//> defines about the Module
/******************************************************************************************/
#define MODVERSION "0.0.0.6"
#define MODDATECODE __DATE__ " - " __TIME__
#define MODLICENSE "GPL";
#define MODDESCRIPTION "Kernel module for the VisionSensor VSPV VPFE(e) devices";
#define MODAUTHOR "IMAGO Technologies GmbH";

#define MODCLASSNAME	"vsdrv"
#define MODMODULENAME	"vsvpfedrv"
#define MODDEBUGOUTTEXT	"vsvpfedrv:"


/*** includes ***/
/******************************************************************************************/
//aus usr/src/linux-headers-2.6.38-8-generic/include
#include <linux/init.h>		// f�r module_init(),
#include <linux/module.h>	// f�r MODULE_LICENSE
#include <linux/version.h>	// f�r die Version

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32)
	#include <linux/printk.h>	// f�r printk
#endif

#include <linux/types.h>		// f�r dev_t
#include <asm/types.h>			// f�r u8, s8
#include <linux/sched.h>		// f�r current (pointer to the current process)
#include <linux/fs_struct.h>
#include <linux/kdev_t.h>		// f�r MAJOR/MINOR
#include <linux/cdev.h>			// f�r cdev_*
#include <linux/device.h>		// f�r class_create
#include <linux/fs.h>			// f�r alloc_chrdev_region /file_*
#include <linux/semaphore.h>	// f�r up/down ...
#include <linux/kfifo.h>		// f�r kfifo_*
#include <linux/pci.h>			// f�r pci*
#include <linux/ioport.h>		// f�r resource*
#include <linux/interrupt.h>	// f�r IRQ*
#include <linux/dma-mapping.h>	//f�r dma_*
#include <linux/scatterlist.h>	// sg_* ...
#include <linux/spinlock.h>		// spin_* ...
#include <linux/delay.h>		// f�r usleep_range
#include <linux/platform_device.h>	// f�r platform_driver ...
#include <linux/pm_runtime.h>	//f�r pm_runtim_*
#if LINUX_VERSION_CODE != KERNEL_VERSION(2,6,32)
	#include <asm/uaccess.h>	// f�r copy_to_user
#else
	#include <linux/uaccess.h>	// f�r copy_to_user
#endif

#include "am437x-vpfe_regs.h"



/*** defines ***/
/******************************************************************************************/
typedef u8 IOCTLBUFFER[128];
#define FALSE 0
#define TRUE 1

#define MIN(x,y) (x < y ? x : y)
#define MAX(x,y) (x > y ? x : y)


#define VSDRV_STATE_UNUSED	0x00	//!< Struct vor dem Init bzw. nach dem UnInit
#define VSDRV_STATE_PREINIT	0x01	//!< FIFOs /SEM erzeugt� alle Feler g�ltig
#define VSDRV_STATE_RUNNING	0x02	//!< AOI wurde im VPFE gesetzt, kann nicht ge�ndert werden, allco m�glich




//> Ioctl definitions siehe. "ioctl-number.txt"
/******************************************************************************************/
//magic number
#define VSDRV_IOC_MAGIC  '['

//richtung ist aus UserSicht, size ist ehr der Type 
#define VSDRV_IOC_DRV_GET_VERSION 		_IOR(VSDRV_IOC_MAGIC, 0, IOCTLBUFFER)
#define VSDRV_IOC_DRV_GET_BUILD_DATE 	_IOR(VSDRV_IOC_MAGIC, 1, IOCTLBUFFER)
#define VSDRV_IOC_DRV_SET_AOI		 	_IOW(VSDRV_IOC_MAGIC, 2, IOCTLBUFFER)

#define VSDRV_IOC_VPFE_START	 		_IOW(VSDRV_IOC_MAGIC, 3, u8)
#define VSDRV_IOC_VPFE_ABORT 			_IOW(VSDRV_IOC_MAGIC, 4, u8)
#define VSDRV_IOC_VPFE_ADD_BUFFER	 	_IOW(VSDRV_IOC_MAGIC, 5, IOCTLBUFFER)

#define VSDRV_IOC_BUFFER_ALLOC			_IOW(VSDRV_IOC_MAGIC, 6, IOCTLBUFFER)
#define VSDRV_IOC_BUFFER_FREE			_IOW(VSDRV_IOC_MAGIC, 7, IOCTLBUFFER)
#define VSDRV_IOC_BUFFER_WAIT_FOR		_IOWR(VSDRV_IOC_MAGIC,8, IOCTLBUFFER)

//max num (nur zum Testen)
#define VSDRV_IOC_MAXNR 8


//> Infos �ber die ...
/******************************************************************************************/
//Anzahl der Elemente im .Jobs_ToTo, .Jobs_Done FIFO
#define MAX_VPFE_JOBFIFO_SIZE	32
//es wird im Moment nur eine VPFE Unit unterst�tzt
#define MAX_DEVICE_COUNT		1

//defaults
#define VPFE_DEFAULT_Width 			640
#define VPFE_DEFAULT_VPFE_Height	480
#define VPFE_DEFAULT_VPFE_Is16BitPixel TRUE




/*** structs ***/
/******************************************************************************************/

//Fast alles zusammen f�r ein UserBuffer f�r die VPFE
typedef struct _VPFE_JOB
{
	//UserBuffer
 	uintptr_t 	pDMA;				//NULL ptr, dann ein DummyJob vom Abort

	//Status (nur g�ltig/gesetzt wenn in .Jobs_Done)
	bool		boIsOk;				//ohne Fehler �bertragen
	u32 		ISRCounter; 		//laufender Z�hler, wird im ISR gez�hlt
}  VPFE_JOB, *PVPFE_JOB;


//Fast alle Infos zu einem VPFE Device zusammen
typedef struct _DEVICE_DATA
{		
	//> Device	
	//***************************************************************/
	u8					VSDrv_State;		//Zustand des Devices
	struct cdev			VSDrv_CDev;			//das KernelObj vom Module	
	bool				VSDrv_IsCDevOpen;	//VSDrv_CDev g�ltig
	struct device*		VSDrv_pDeviceDevice;//platform_device::dev
	spinlock_t 			VSDrv_SpinLock;		//lock f�r ein Device (IRQSave)
	dev_t				VSDrv_DeviceNumber;	//Nummer von CHAR device

	//> FIFO
	//***************************************************************/
	struct completion 	FIFO_Waiter;								//'sem' um auf ein Element in FIFO_JobsDone zu warten
	DECLARE_KFIFO(FIFO_JobsToDo, VPFE_JOB, MAX_VPFE_JOBFIFO_SIZE);	//sind noch nicht in der VPFE Unit
	DECLARE_KFIFO(FIFO_JobsDone, VPFE_JOB, MAX_VPFE_JOBFIFO_SIZE);	//sind nicht mehr in der VPFE Unit (kann aber auch abgebrochen worden sein)
	
	//> VPFE
	//***************************************************************/
	void __iomem *VPFE_CCDC_BaseAddr;
	u32			VPFE_ISRCounter;	//wird bei jedem �FrameStart� ISR hochgez�hlt
	u32 		VPFE_Width;
	u32	 		VPFE_Height;
	bool 		VPFE_Is16BitPixel;
	bool		VPFE_IsStartFrameDone;	//sind wir in einem Bild?

} DEVICE_DATA, *PDEVICE_DATA;

//Fast alles zusammen was zu diesem Module geh�rt, (Note: n VPFEdevs f�r das Module, Module wird nur 1x geladen)
typedef struct _MODULE_DATA
{
	DEVICE_DATA		Devs[MAX_DEVICE_COUNT];			//Infos/Context f�r je device, Index ist der Minor
	bool			boIsMinorUsed[MAX_DEVICE_COUNT];//daher ist Devs[Minor] benutzt/frei?

	dev_t 			FirstDeviceNumber;				//MAJOR(devNumber),MINOR(devNumber) (eg 240 , 0)
	struct class	*pModuleClass;					// /sys/class/*

} MODULE_DATA, *PMODULE_DATA;


//Global vars
extern MODULE_DATA _ModuleData;


/*** prototypes ***/
/******************************************************************************************/
/* Module fns */
int VSDrv_init(void);
void VSDrv_exit(void);
void VSDrv_InitDrvData(PDEVICE_DATA pDevData);

/* AM473X fns */
int VSDrv_AM473X_probe(struct platform_device *pdev);
int VSDrv_AM473X_remove(struct platform_device *pdev);


/* ~IRQ fns */
irqreturn_t VSDrv_VPFE_interrupt(int irq, void *dev);


/* File fns */
int VSDrv_open(struct inode *node, struct file *filp);
ssize_t VSDrv_read (struct file *filp, char __user *buf, size_t count, loff_t *pos);
ssize_t VSDrv_write (struct file *filp, const char __user *buf, size_t count,loff_t *pos);
long VSDrv_unlocked_ioctl (struct file *filp, unsigned int cmd,unsigned long arg);


/* VPFE fns */
int VSDrv_VPFE_Configure(PDEVICE_DATA pDevData);
int VSDrv_VPFE_AddBuffer(PDEVICE_DATA pDevData, dma_addr_t pDMAKernelBuffer);
void VSDrv_VPFE_TryToAddNextBuffer_locked(PDEVICE_DATA pDevData);
int VSDrv_VPFE_Abort(PDEVICE_DATA pDevData);


/* Buffer fns */
int VSDrv_BUF_Alloc(PDEVICE_DATA pDevData, void** ppVMKernel, dma_addr_t *ppDMAKernel, size_t *panzBytes);
void VSDrv_BUF_Free(PDEVICE_DATA pDevData, void* pVMKernel, dma_addr_t pDMAKernel, size_t anzBytes);
int VSDrv_BUF_mmap(struct file *, struct vm_area_struct *);
int VSDrv_BUF_WaitFor(PDEVICE_DATA pDevData, const u32 TimeOut_ms, u32 *pIsBroken, u32 *pImageNumber, dma_addr_t *ppDMAKernel);

#endif /* VSDRV_H_ */

