/*
 * VCDrv.c
 *
 * The entry point to the kernel module
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * -> module (un)load, read & write & IO, PCI probe/remove, IRQ(ISR)
 *
 */

#include "VCDrv.h"

//module defines "sudo modinfo vcvpfedrv.ko"
MODULE_VERSION(MODVERSION);
MODULE_LICENSE(MODLICENSE);
MODULE_DESCRIPTION(MODDESCRIPTION);
MODULE_AUTHOR(MODAUTHOR);

//übergibt/speichert die pointer
module_init(VCDrv_init);
module_exit(VCDrv_exit);

//static member (die gleichen für alle devices)
MODULE_DATA _ModuleData;


//für welchen DeviceTree Eintrag sind wir zuständig?
//https://stackoverflow.com/questions/38493999/how-devices-in-device-tree-and-platform-drivers-were-connected
static const struct of_device_id of_vcdrv_vpfe_match[] = {
    {
        .compatible = "IMAGO,vcdrv-vpfe",
    },
    {},
};

MODULE_DEVICE_TABLE(of, of_vcdrv_vpfe_match);	//macht dem kernel bekannt für was dieses Moduel ist


//struct mit den PCICallBacks
/****************************************************************************************************************/
static struct platform_driver vcdrv_vpfe_plaform_driver = {
	.probe      = VCDrv_AM473X_probe,
	.remove	    = VCDrv_AM473X_remove,
	.driver     = {
		.name   = MODMODULENAME,
		.of_match_table = of_vcdrv_vpfe_match,
		.owner = THIS_MODULE,
	},
};


// "Called when a device is added, removed from this class, or a few other things that generate uevents to add the environment variables."
/****************************************************************************************************************/
static int VCDrv_dev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	pr_devel(MODDEBUGOUTTEXT" VCDrv_dev_uevent\n");
    if( add_uevent_var(env, "DEVMODE=%#o", 0666) != 0)
		printk(KERN_WARNING MODDEBUGOUTTEXT" add_uevent_var() failed\n");
    return 0;
}



//<====================================>
//	Module
//<====================================>
/****************************************************************************************************************/
//wird aufgerufen wenn das Modul geladen wird
/****************************************************************************************************************/
int VCDrv_init(void)
{
	int res,i;

	pr_devel(MODDEBUGOUTTEXT " enter init\n");


	/* init member */
	/**********************************************************************/
	_ModuleData.pModuleClass =  ERR_PTR(-EFAULT);

	for(i=0; i<MAX_DEVICE_COUNT; i++){
		VCDrv_InitDrvData(&_ModuleData.Devs[i]);
		_ModuleData.boIsMinorUsed[i] = FALSE;
	}
		

	/* sich n device nummer holen */
	/**********************************************************************/
	// steht dann in /proc/devices
	res = alloc_chrdev_region(
			&_ModuleData.FirstDeviceNumber,	/* out die 1. nummer */
			0,					/* 1. minor am besten 0 */
			MAX_DEVICE_COUNT,	/* wie viele */
			MODMODULENAME);		/* name vom driver */
	if (res < 0) {
		printk(KERN_WARNING MODDEBUGOUTTEXT" can't get major!\n");
		return res;
	}
	else
		pr_devel(MODDEBUGOUTTEXT" major %d, minor %d, anz %d\n",MAJOR(_ModuleData.FirstDeviceNumber),MINOR(_ModuleData.FirstDeviceNumber), MAX_DEVICE_COUNT);
	//sicher ist sicher (wir nutzen den Minor als Index für _ModuleData_Devs[])
	if( MINOR(_ModuleData.FirstDeviceNumber) != 0){
		printk(KERN_WARNING MODDEBUGOUTTEXT" start minor must we zero!\n");
		return -EINVAL;
	}


	/* erzeugt eine Sysfs class */
	/**********************************************************************/
	_ModuleData.pModuleClass = class_create(THIS_MODULE, MODCLASSNAME);
	if( IS_ERR(_ModuleData.pModuleClass))
		printk(KERN_WARNING MODDEBUGOUTTEXT" can't create sysfs class!\n");
	
	//add the uevend handler
	if( !IS_ERR(_ModuleData.pModuleClass) )
		_ModuleData.pModuleClass->dev_uevent = VCDrv_dev_uevent;	//send uevents to udev, so it'll create the /dev/node


	/* macht dem Kernel den treiber bekannt */
	/**********************************************************************/
	if( platform_driver_register(&vcdrv_vpfe_plaform_driver) !=0)
		printk(KERN_WARNING MODDEBUGOUTTEXT" platform_driver_register failed!\n");

	printk(KERN_INFO MODDEBUGOUTTEXT" init done (%s [%s])\n", MODDATECODE, MODVERSION);
	pr_devel(MODDEBUGOUTTEXT" leave init\n");
	return 0;
}


/****************************************************************************************************************/
//wird aufgerufen wenn das Modul entladen wird
//Note: kann nicht entladen werden wenn es noch genutzt wird
/****************************************************************************************************************/
void VCDrv_exit(void)
{
	pr_devel(MODDEBUGOUTTEXT" enter exit\n");

	//wir können uns nicht mehr ums dev kümmern:-)
	platform_driver_unregister(&vcdrv_vpfe_plaform_driver);
		
	//gibt Sysfs class frei
	if(!IS_ERR(_ModuleData.pModuleClass))
		class_destroy(_ModuleData.pModuleClass);

	//gibt die dev Nnummern frei
	//Note: "cleanup_module is never called if registering failed"
	unregister_chrdev_region(_ModuleData.FirstDeviceNumber, MAX_DEVICE_COUNT);
}

//setzt alle Felder auf definierte Werte
void VCDrv_InitDrvData(PDEVICE_DATA pDevData)
{
	//Note: darf nicht wegen  FIFO/completion... 
	//memset(pDevData, 0, sizeof(DEVICE_DATA));

	//> Device	
	//***************************************************************/
	pDevData->VCDrv_State 		= VCDRV_STATE_UNUSED;
	pDevData->VCDrv_IsCDevOpen	= FALSE;
	pDevData->VCDrv_pDeviceDevice= NULL;
	//wird im probe() gemacht
	// pDevData->VCDrv_CDev;
	// pDevData->VCDrv_DeviceNumber;
	spin_lock_init(&pDevData->VCDrv_SpinLock);	

	//> FIFO
	//***************************************************************/
	//" Initialization is accomplished by initializing the wait queue and setting
	//  the default state to "not available", that is, "done" is set to 0. "
	init_completion(&pDevData->FIFO_Waiter);//darf man das n mal aufrufen?
	INIT_KFIFO(pDevData->FIFO_JobsToDo); 	//setzt nur werte (kein alloc oder co)
	INIT_KFIFO(pDevData->FIFO_JobsDone);
	
	//> VPFE
	//***************************************************************/
	pDevData->VPFE_CCDC_BaseAddr= NULL;
	pDevData->VPFE_ISRCounter	= 0;
	pDevData->VPFE_Width		= VPFE_DEFAULT_Width;
	pDevData->VPFE_Height		= VPFE_DEFAULT_VPFE_Height;
	pDevData->VPFE_Is16BitPixel	= VPFE_DEFAULT_VPFE_Is16BitPixel;
	pDevData->VPFE_ImagePointer = 0;



	pDevData->VCDrv_State = VCDRV_STATE_PREINIT;
}

