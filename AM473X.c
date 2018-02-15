/*
 * AM473X.c
 *
 * AM473X(e) Probe code
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
 *
 */

#include "VCDrv.h"


//struct mit den file fns
struct file_operations VCDrv_fops = {
	.owner	= THIS_MODULE,
	.open	= VCDrv_open,
	.read	= VCDrv_read,
	.write	= VCDrv_write,
	.mmap	= VCDrv_BUF_mmap,
	.unlocked_ioctl = VCDrv_unlocked_ioctl,
	.llseek = no_llseek,
};



/****************************************************************************************************************/
//< HWIRQ > hier kommt der VPFE IRQ an (da nicht geteilt kein prüfen notwendig)
/****************************************************************************************************************/
irqreturn_t VCDrv_VPFE_interrupt(int irq, void *dev)
{
//	PDEVICE_DATA pDevData = (PDEVICE_DATA)dev;
	pr_devel(MODDEBUGOUTTEXT" VCDrv_VPFE_interrupt(irq: %d)\n", irq);
#ifndef DEBUG
  implement me
#endif


	return IRQ_HANDLED;
}


/****************************************************************************************************************/
//wird aufgerufen wenn der kernel im DeviceTree einen Eintrag findet
/****************************************************************************************************************/
int VCDrv_AM473X_probe(struct platform_device *pdev)
{		
	int res, i, DevIndex, IRQNumber;
	struct resource *pResource = NULL;	
	platform_set_drvdata(pdev, NULL);	
	

	pr_devel(MODDEBUGOUTTEXT" VCDrv_AM473X_probe\n");


	//>freie Minor Nummer?
	/**********************************************************************/
	DevIndex =-1;
	for(i=0; i<MAX_DEVICE_COUNT; i++)
	{
		if(!_ModuleData.boIsMinorUsed[i])
		{
			DevIndex = i;
			VCDrv_InitDrvData(&_ModuleData.Devs[DevIndex]);			
			_ModuleData.Devs[DevIndex].VCDrv_DeviceNumber = MKDEV(MAJOR(_ModuleData.FirstDeviceNumber), DevIndex);
			_ModuleData.Devs[DevIndex].VCDrv_pDeviceDevice = &pdev->dev;
			platform_set_drvdata(pdev, &_ModuleData.Devs[DevIndex]);				//damit wir im VCDrv_AM473X_remove() wissen welches def freigebene werden soll
			break;
		}
	}
	if(DevIndex==-1){
		printk(KERN_WARNING MODDEBUGOUTTEXT" no free Minor-Number found!\n"); return -EINVAL;}
	else
		pr_devel(MODDEBUGOUTTEXT" use major/minor (%d:%d)\n", MAJOR(_ModuleData.Devs[DevIndex].VCDrv_DeviceNumber), MINOR(_ModuleData.Devs[DevIndex].VCDrv_DeviceNumber));


	//>ccdc_cfg.base_addr 
	/**********************************************************************/
	//holt sich aus dem DeviceTree die BaseAddress
	// https://elixir.free-electrons.com/linux/v4.8/source/drivers/media/platform/am437x/am437x-vpfe.c#L2544
	// https://github.com/torvalds/linux/blob/master/arch/arm/boot/dts/am4372.dtsi
	pResource = platform_get_resource(pdev, IORESOURCE_MEM/*res type*/, 0/*res index*/);
	if( pResource == NULL)
		{printk(KERN_ERR MODDEBUGOUTTEXT" platform_get_resource() failed!\n"); return -EINVAL;}
	//mapped den IOMem 
	//Note:
	// https://vthakkar1994.wordpress.com/2015/08/24/devm-functions-and-their-correct-usage/
	// http://haifux.org/lectures/323/haifux-devres.pdf
	//	> devm_* ist ein auto_ptr für kernel obj 
	_ModuleData.Devs[DevIndex].VPFE_CCDC_BaseAddr = devm_ioremap_resource(&pdev->dev, pResource);
	if (IS_ERR(_ModuleData.Devs[DevIndex].VPFE_CCDC_BaseAddr))
		{printk(KERN_ERR MODDEBUGOUTTEXT" devm_ioremap_resource() failed!\n"); return PTR_ERR(_ModuleData.Devs[DevIndex].VPFE_CCDC_BaseAddr);}
	pr_devel(MODDEBUGOUTTEXT" CCDC base: 0x%p (mapped)\n", _ModuleData.Devs[DevIndex].VPFE_CCDC_BaseAddr);

	
	//>IRQ 
	/**********************************************************************/
	//holt sich aus dem DeviceTree die IRQ nummer an
	IRQNumber = platform_get_irq(pdev, 0/*IRQ number index*/);
	if( IRQNumber <=0 )
		{printk(KERN_ERR MODDEBUGOUTTEXT" platform_get_irq() failed!\n"); return -ENODEV;}
	if(devm_request_irq(&pdev->dev, 	/* device to request interrupt for */
				IRQNumber,				/* Interrupt line to allocate */
				VCDrv_VPFE_interrupt,	/* Function to be called when the IRQ occurs*/
				0						/* IRQ Flags */,
			    MODMODULENAME,			/* name wird in /proc/interrupts angezeigt */
				&_ModuleData.Devs[DevIndex] /* wird dem CallBack mit gegeben*/) != 0 )
	{
		printk(KERN_ERR MODDEBUGOUTTEXT" devm_request_irq() failed!\n"); return -EINVAL;
	}
	pr_devel(MODDEBUGOUTTEXT" IRQ: %d (mapped)\n",IRQNumber);


	//>dev init & fügt es hinzu
	/**********************************************************************/
	cdev_init(&_ModuleData.Devs[DevIndex].VCDrv_CDev, &VCDrv_fops);
	_ModuleData.Devs[DevIndex].VCDrv_CDev.owner = THIS_MODULE;
	_ModuleData.Devs[DevIndex].VCDrv_CDev.ops 	= &VCDrv_fops;	//notwendig in den quellen wird fops gesetzt?

	//fügt ein device hinzu, nach der fn können FileFns genutzt werden
	res = cdev_add(&_ModuleData.Devs[DevIndex].VCDrv_CDev, _ModuleData.Devs[DevIndex].VCDrv_DeviceNumber, 1/*wie viele ab startNum*/);
	if(res < 0)
		printk(KERN_WARNING MODDEBUGOUTTEXT" can't add device!\n");
	else
		_ModuleData.Devs[DevIndex].VCDrv_IsCDevOpen = TRUE;


	//> in Sysfs class eintragen
	/**********************************************************************/			
	//war mal class_device_create
	if( !IS_ERR(_ModuleData.pModuleClass) )
	{		
		char devName[128];
		struct device *temp;

		sprintf(devName, "%s%d", MODMODULENAME, MINOR(_ModuleData.Devs[DevIndex].VCDrv_DeviceNumber));
		temp = device_create(
				_ModuleData.pModuleClass, 	/* die Type classe */
				NULL, 			/* pointer zum Eltern, dann wird das dev ein Kind vom parten*/
				_ModuleData.Devs[DevIndex].VCDrv_DeviceNumber, /* die nummer zum device */
				NULL,
				devName			/*string for the device's name */
				);

		if( IS_ERR(temp))
			printk(KERN_WARNING MODDEBUGOUTTEXT" can't create sysfs device!\n");
	}


	// init von allem ist durch
	printk(KERN_INFO MODDEBUGOUTTEXT" AM473X probe done (%d:%d)\n",
		MAJOR(_ModuleData.Devs[DevIndex].VCDrv_DeviceNumber), MINOR(_ModuleData.Devs[DevIndex].VCDrv_DeviceNumber));	
	_ModuleData.boIsMinorUsed[DevIndex] = TRUE;

	return 0;
}


/****************************************************************************************************************/
//wird aufgerufen wenn das AM473Xdev removed wird
/****************************************************************************************************************/
int VCDrv_AM473X_remove(struct platform_device *pdev)
{
	//Note: wenn AM473X_remove() aufgerufen wird, 
	// darf kein UserThread mehr im Teiber sein bzw. noch reinspringen weil sonst... bum 	
 	//
	// IRQ & VPFERegs (ioremap) sind per devm_ (daher wie auto_ptr:-)
	//http://haifux.org/lectures/323/haifux-devres.pdf
	PDEVICE_DATA pDevData = (PDEVICE_DATA)platform_get_drvdata(pdev);
	pr_devel(MODDEBUGOUTTEXT" VCDrv_AM473X_remove (%d:%d)\n", MAJOR(pDevData->VCDrv_DeviceNumber), MINOR(pDevData->VCDrv_DeviceNumber));
	if(pDevData == NULL){
		printk(KERN_WARNING MODDEBUGOUTTEXT" device pointer is zero!\n"); return -EFAULT;}


	//-> VPFE stoppen, User raushohlen, Buffer verschieben
	// aber keine Buffer freigeben da der User sie noch gemapped haben könnte (und wir auch dem pVMKernel nicht haben)
	VCDrv_VPFE_Abort(pDevData);


	//device in der sysfs class löschen
	if(!IS_ERR(_ModuleData.pModuleClass))	
		device_destroy(_ModuleData.pModuleClass, pDevData->VCDrv_DeviceNumber);
	//device löschen
	if(pDevData->VCDrv_IsCDevOpen)
		cdev_del(&pDevData->VCDrv_CDev);
	pDevData->VCDrv_IsCDevOpen = FALSE;

	pDevData->VCDrv_State = VCDRV_STATE_PREINIT;
	return 0;
}


/****************************************************************************************************************/
//fügt wenn ins FIFO den buffer hinzu (wenn STATE_RUNNING & noch frei)
//versucht dann auch einen Buffer in die VPFE Unit zu ädden
/****************************************************************************************************************/
int VCDrv_VPFE_AddBuffer(PDEVICE_DATA pDevData, dma_addr_t pDMAKernelBuffer)
{
	unsigned long irqflags;
	int result = 0;
	spin_lock_irqsave(&pDevData->VCDrv_SpinLock, irqflags);
//----------------------------->

	//> müssen im State running sein
	if( pDevData->VCDrv_State != VCDRV_STATE_RUNNING){
		printk(KERN_WARNING MODDEBUGOUTTEXT "VCDrv_BUF_Alloc> state must be VCDRV_STATE_RUNNING!\n"); result=-EBUSY;
	}
	else
	{
		//ist noch Platz (.FIFO_JobsToDo)?
		if( kfifo_avail(&pDevData->FIFO_JobsToDo) < 1 ){
			printk(KERN_WARNING MODDEBUGOUTTEXT" Locked_ioctl> FIFO_JobsToDo is full\n"); result = -ENOMEM;}
		else
		{
			//noch Platz? (.FIFO_JobsDone), es müssen passen
			// - alle (möglichen) laufenden
			// - da beim AbortBuffer alle Jobs aus .FIFO_JobsToDo nach .FIFO_JobsDone verschoben werden
			u64 SizeNeeded = kfifo_len(&pDevData->FIFO_JobsToDo);
			SizeNeeded += 1;
			if( kfifo_avail(&pDevData->FIFO_JobsDone) < SizeNeeded ){
				printk(KERN_WARNING MODDEBUGOUTTEXT" Locked_ioctl> FIFO_JobsDone is too full\n"); result = -ENOMEM;}
			else
			{
				//> job erzeugen & adden
				VPFE_JOB tmpJob;
				tmpJob.pDMA = pDMAKernelBuffer;
				if( kfifo_put(&pDevData->FIFO_JobsToDo, tmpJob) == 0 ){
					printk(KERN_WARNING MODDEBUGOUTTEXT" Locked_ioctl> can't add into FIFO_JobsToDo\n"); result = -ENOMEM;}
			}//platz im FIFO_JobsDone
		}//platz im FIFO_JobsToDo
	}//if STATE_RUNNING


	//> versuchen Buffer der Unit zu übergeben
	VCDrv_VPFE_TryToAddNextBuffer_locked(pDevData);

//<-----------------------------
	spin_unlock_irqrestore(&pDevData->VCDrv_SpinLock, irqflags);


	return result;
}


/****************************************************************************************************************/
//wenn Unit frei, Unit läuft und ein Buffer FIFO_JobsToDo dann adden 
/****************************************************************************************************************/
void VCDrv_VPFE_TryToAddNextBuffer_locked(PDEVICE_DATA pDevData)
{
#ifndef DEBUG
  implement me
#endif
}


/****************************************************************************************************************/
// - hält Unit an, 
// - bricht alle Waiter ab, (bleibt im signaled state)
// - alle Buffer sind dann in FIFO_JobsDone,
// - neuer STATE_PREINIT  
/****************************************************************************************************************/
int VCDrv_VPFE_Abort(PDEVICE_DATA pDevData)
{
	unsigned long irqflags;
	int result = 0;

	spin_lock_irqsave(&pDevData->VCDrv_SpinLock, irqflags);
//----------------------------->

	//> hält die UNIT an
#ifndef DEBUG
  implement me
#endif
	
	//> alle Buffer aus .FIFO_JobsToDo >> .FIFO_JobsDone 
	// im VCDrv_VPFE_AddBuffer() passen wir auf das alle Buffer aus FIFO_JobsToDo i FIFO_JobsDone passen
	while ( kfifo_len(&pDevData->FIFO_JobsToDo) >= 1 )
	{
		VPFE_JOB tmpJob;

		//siche ist sicher (noch Platz im FIFO?)
		if( kfifo_avail(&pDevData->FIFO_JobsDone) < 1){
			printk(KERN_WARNING MODDEBUGOUTTEXT" VCDrv_VPFE_Abort> FIFO_JobsDone is full!\n"); result = -ENOMEM; break;}

		//Element aus FIFO_JobsToDo nehmen
		if( kfifo_get(&pDevData->FIFO_JobsToDo, &tmpJob) == 1) 
		{
			tmpJob.boIsOk = FALSE; //damit wissen wir später das es ein Abort war

			if( kfifo_put(&pDevData->FIFO_JobsDone, tmpJob) == 0){
				printk(KERN_WARNING MODDEBUGOUTTEXT" VCDrv_VPFE_Abort> kfifo_put() failed!\n"); result = -EINTR; break;}
			else
				pr_devel(MODDEBUGOUTTEXT" - move Buffer FIFO_JobsToDo >> FIFO_JobsDone [0x%llx]\n",(u64)tmpJob.pDMA);			
		}
		else{
			printk(KERN_WARNING MODDEBUGOUTTEXT" VCDrv_VPFE_Abort> kfifo_get() failed!\n"); result = -EINTR; break;}

	}//while FIFO_JobsToDo not empty


	//> alle Waiter aufwecken
	//Note: 
	//" reinit_completion ... This is especially important after complete_all() is used."
	//" Waiting threads wakeup order is the same in which they were enqueued (FIFO order)."	
	complete_all(&pDevData->FIFO_Waiter);


	//> neuer State
	pDevData->VCDrv_State = VCDRV_STATE_PREINIT;

 //<-----------------------------
	spin_unlock_irqrestore(&pDevData->VCDrv_SpinLock, irqflags);


	return result;
}


