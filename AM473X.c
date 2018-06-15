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

#include "VSDrv.h"


//struct mit den file fns
struct file_operations VSDrv_fops = {
	.owner	= THIS_MODULE,
	.open	= VSDrv_open,
	.read	= VSDrv_read,
	.write	= VSDrv_write,
	.mmap	= VSDrv_BUF_mmap,
	.unlocked_ioctl = VSDrv_unlocked_ioctl,
	.llseek = no_llseek,
};



/****************************************************************************************************************/
// helpers fns
/****************************************************************************************************************/
/****************************************************************************************************************/


//liest ein VPFE Register (keine Tests)
static inline u32 vpfe_reg_read(PDEVICE_DATA pDevData, const u32 offset)
{
	return ioread32(pDevData->VPFE_CCDC_BaseAddr + offset);
}

//schreibt ein VPFE Register (keine Tests)
static inline void vpfe_reg_write(PDEVICE_DATA pDevData, const u32 val, const u32 offset)
{
	iowrite32(val, pDevData->VPFE_CCDC_BaseAddr + offset);
}

//start/stop der Unit (This bit is latched by VD (start of frame))
static inline void vpfe_pcr_enable(PDEVICE_DATA pDevData, const int flag)
{
	vpfe_reg_write(pDevData, !!flag, VPFE_PCR);
}


//Data write enable. Controls whether or not input raw data is written to external memory.
//This bit is latched by VD.
static inline void vpfe_wen_enable(PDEVICE_DATA pDevData, const bool boTurnOn)
{
	u32 regVal = vpfe_reg_read(pDevData, VPFE_SYNMODE);
	regVal &= (~VPFE_WEN_ENABLE);
	regVal |= (boTurnOn)?(VPFE_WEN_ENABLE):(0);
	vpfe_reg_write(pDevData, regVal, VPFE_SYNMODE);
}


#ifdef DEBUG
//gibt alle Register aus (nur zum debuggen)
static void vpfe_reg_dump(PDEVICE_DATA pDevData)
{
	pr_devel(MODDEBUGOUTTEXT" named\n");
	pr_devel(MODDEBUGOUTTEXT"------------------------\n");
	pr_devel(MODDEBUGOUTTEXT"ALAW: 0x%x\n", 		vpfe_reg_read(pDevData, VPFE_ALAW));
	pr_devel(MODDEBUGOUTTEXT"CLAMP: 0x%x\n", 		vpfe_reg_read(pDevData, VPFE_CLAMP));
	pr_devel(MODDEBUGOUTTEXT"DCSUB: 0x%x\n", 		vpfe_reg_read(pDevData, VPFE_DCSUB));
	pr_devel(MODDEBUGOUTTEXT"BLKCMP: 0x%x\n", 		vpfe_reg_read(pDevData, VPFE_BLKCMP));
	pr_devel(MODDEBUGOUTTEXT"COLPTN: 0x%x\n", 		vpfe_reg_read(pDevData, VPFE_COLPTN));
	pr_devel(MODDEBUGOUTTEXT"SDOFST: 0x%x\n", 		vpfe_reg_read(pDevData, VPFE_SDOFST));
	pr_devel(MODDEBUGOUTTEXT"SYN_MODE: 0x%x\n", 	vpfe_reg_read(pDevData, VPFE_SYNMODE));
	pr_devel(MODDEBUGOUTTEXT"HSIZE_OFF: 0x%x\n", 	vpfe_reg_read(pDevData, VPFE_HSIZE_OFF));
	pr_devel(MODDEBUGOUTTEXT"HORZ_INFO: 0x%x\n", 	vpfe_reg_read(pDevData, VPFE_HORZ_INFO));
	pr_devel(MODDEBUGOUTTEXT"VERT_START: 0x%x\n",	vpfe_reg_read(pDevData, VPFE_VERT_START));
	pr_devel(MODDEBUGOUTTEXT"VERT_LINES: 0x%x\n",	vpfe_reg_read(pDevData, VPFE_VERT_LINES));
	pr_devel(MODDEBUGOUTTEXT"VPFE_VDINT: 0x%x\n",	vpfe_reg_read(pDevData, VPFE_VDINT));
	pr_devel(MODDEBUGOUTTEXT"VPFE_CCDCFG: 0x%x\n",	vpfe_reg_read(pDevData, VPFE_CCDCFG));
	pr_devel(MODDEBUGOUTTEXT"VPFE_SYSCONFIG: 0x%x\n",vpfe_reg_read(pDevData, VPFE_SYSCONFIG));
	pr_devel(MODDEBUGOUTTEXT"VPFE_CONFIG: 0x%x\n",	vpfe_reg_read(pDevData, VPFE_CONFIG));
	pr_devel(MODDEBUGOUTTEXT"VPFE_IRQ_EN_SET: 0x%x\n",vpfe_reg_read(pDevData, VPFE_IRQ_EN_SET));
	pr_devel(MODDEBUGOUTTEXT"VPFE_IRQ_EN_CLR: 0x%x\n", vpfe_reg_read(pDevData, VPFE_IRQ_EN_CLR));
/*
 * {
	int i;
	pr_devel(MODDEBUGOUTTEXT"\n raw\n");
	pr_devel(MODDEBUGOUTTEXT"------------------------\n");
	for (i = 0; i <= 0x120; i+= 4)
		pr_devel(MODDEBUGOUTTEXT" 0x%03X 0x%8X\n", i, vpfe_reg_read(pDevData, i));
	}
*/
}

#endif









/****************************************************************************************************************/
// public fns
/****************************************************************************************************************/
/****************************************************************************************************************/


/****************************************************************************************************************/
//< HWIRQ > hier kommt der VPFE IRQ an (da nicht geteilt kein prüfen notwendig)
/****************************************************************************************************************/
irqreturn_t VSDrv_VPFE_interrupt(int irq, void *dev)
{
	u32 vpfe_irq_flags;	
	unsigned long irqflags;
	PDEVICE_DATA pDevData = (PDEVICE_DATA)dev;

	vpfe_irq_flags = vpfe_reg_read(pDevData, VPFE_IRQ_STS);



	spin_lock_irqsave(&pDevData->VSDrv_SpinLock, irqflags);
//----------------------------->

	//je nach IRQFlag
	//> Bild-Anfang (nach der 1. Zeile)
	if (vpfe_irq_flags & VPFE_VDINT0)
	{
		//pr_devel(MODDEBUGOUTTEXT" VSDrv_VPFE_interrupt(VPFE_VDINT0)\n");

		//nicht mehr ins RAM schreiben  
		//Note: 
		// - ohne das würde die Einheit das Bild wieder überschreiben
		// - im Prinzip eine RaceCond, da wenn innerhalb der 1. Zeile VSDrv_VPFE_TryToAddNextBuffer_locked() aufgerufen wird
		//  bekommt der User ein schwarzes Bild
		vpfe_wen_enable(pDevData, FALSE);
	
		//BildZähler erhöhen
		pDevData->VPFE_ISRCounter++;

		//Note:
		// es muss sichergestellt werden das (wenn ein Bild eingezogen werden soll) zuerst VPFE_VDINT0 dann VPFE_VDINT1 kommt
		// sonnst kann es sein:
		//  VSDrv_VPFE_TryToAddNextBuffer_locked() setzt inner halb eines Bildes, VPFE_SDR_ADDR und WEN=true
		//  dann kommt VPFE_VDINT1 und setzt dan den ptr auf NULL zurück (ohne das gerapped wurde)  (WEN bleibt auf true da VPFE_VDINT0 verpasst wurde)
		//  mit den nächsten VD werden die Register übernommen  ==> BUM
		if( (vpfe_reg_read(pDevData, VPFE_SDR_ADDR) != 0x0000) && (pDevData->VPFE_IsStartFrameDone==FALSE) )
			pDevData->VPFE_IsStartFrameDone = TRUE;
	}

	//> Bild-Ende (nach der letzen Zeile)
	else if ( (vpfe_irq_flags & VPFE_VDINT1) && (pDevData->VPFE_IsStartFrameDone==TRUE) )
	{
		//pr_devel(MODDEBUGOUTTEXT" VSDrv_VPFE_interrupt(VPFE_VDINT1)\n");

		//warten wir auf ein Bild?
		if( vpfe_reg_read(pDevData, VPFE_SDR_ADDR) != 0x0000 )
		{
			//> ja JOB erzeugen
			VPFE_JOB tmpJob;
			tmpJob.pDMA			= vpfe_reg_read(pDevData, VPFE_SDR_ADDR);
			tmpJob.boIsOk 		= TRUE;
			tmpJob.ISRCounter 	= pDevData->VPFE_ISRCounter;

			//> und FIFO adden (es ist immer genug Platz)
			if( kfifo_avail(&pDevData->FIFO_JobsDone) < 1 ){
				pr_devel(MODDEBUGOUTTEXT" VSDrv_VPFE_interrupt()> FIFO_JobsDone is full\n");}
			else
			{
				if( kfifo_put(&pDevData->FIFO_JobsDone, tmpJob) == 0 ){
					pr_devel(MODDEBUGOUTTEXT" VSDrv_VPFE_interrupt> can't add into FIFO_JobsDone\n");}

				//pr_devel(MODDEBUGOUTTEXT" VSDrv_VPFE_interrupt(VPFE_VDINT1)> new Buffer: 0x%llx, (seq: %d) done\n", (u64) tmpJob.pDMA, tmpJob.ISRCounter);


				//> Waiter aufwecken (hat Zähler)
				complete(&pDevData->FIFO_Waiter);

			}//platz im FIFO_JobsDone

			//> Unit als "frei" marken
			vpfe_reg_write(pDevData, 0x0000, VPFE_SDR_ADDR);
			pDevData->VPFE_IsStartFrameDone = FALSE;

		}//if warten aufs Bild 

		//> versuchen neuen Buffer zu adden
		VSDrv_VPFE_TryToAddNextBuffer_locked(pDevData);

	}//if BildEnde ISR
		
//<-----------------------------
	spin_unlock_irqrestore(&pDevData->VSDrv_SpinLock, irqflags);

	//> IRQ Flags löschen
	vpfe_reg_write(pDevData, VPFE_VDINT2+VPFE_VDINT1+VPFE_VDINT0, VPFE_IRQ_STS);
	vpfe_reg_write(pDevData, 1, VPFE_IRQ_EOI);
	
	return IRQ_HANDLED;
}



/****************************************************************************************************************/
//wird aufgerufen wenn der kernel im DeviceTree einen Eintrag findet
/****************************************************************************************************************/
int VSDrv_AM473X_probe(struct platform_device *pdev)
{		
	int res, i, DevIndex, IRQNumber;
	struct resource *pResource = NULL;	
	platform_set_drvdata(pdev, NULL);	
	

	pr_devel(MODDEBUGOUTTEXT" VSDrv_AM473X_probe\n");


	//>freie Minor Nummer?
	/**********************************************************************/
	DevIndex =-1;
	for(i=0; i<MAX_DEVICE_COUNT; i++)
	{
		if(!_ModuleData.boIsMinorUsed[i])
		{
			DevIndex = i;
			VSDrv_InitDrvData(&_ModuleData.Devs[DevIndex]);			
			_ModuleData.Devs[DevIndex].VSDrv_DeviceNumber = MKDEV(MAJOR(_ModuleData.FirstDeviceNumber), DevIndex);
			_ModuleData.Devs[DevIndex].VSDrv_pDeviceDevice = &pdev->dev;
			platform_set_drvdata(pdev, &_ModuleData.Devs[DevIndex]);				//damit wir im VSDrv_AM473X_remove() wissen welches def freigebene werden soll
			break;
		}
	}
	if(DevIndex==-1){
		printk(KERN_WARNING MODDEBUGOUTTEXT" no free Minor-Number found!\n"); return -EINVAL;}
	else{
		/*pr_devel(MODDEBUGOUTTEXT" use major/minor (%d:%d)\n", MAJOR(_ModuleData.Devs[DevIndex].VSDrv_DeviceNumber), MINOR(_ModuleData.Devs[DevIndex].VSDrv_DeviceNumber))*/;}


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
	pr_devel(MODDEBUGOUTTEXT"  CCDC base: 0x%p (mapped)\n", _ModuleData.Devs[DevIndex].VPFE_CCDC_BaseAddr);

	
	//>IRQ 
	/**********************************************************************/
	//holt sich aus dem DeviceTree die IRQ nummer an
	IRQNumber = platform_get_irq(pdev, 0/*IRQ number index*/);
	if( IRQNumber <=0 )
		{printk(KERN_ERR MODDEBUGOUTTEXT" platform_get_irq() failed!\n"); return -ENODEV;}
	if(devm_request_irq(&pdev->dev, 	/* device to request interrupt for */
				IRQNumber,				/* Interrupt line to allocate */
				VSDrv_VPFE_interrupt,	/* Function to be called when the IRQ occurs*/
				0						/* IRQ Flags */,
			    MODMODULENAME,			/* name wird in /proc/interrupts angezeigt */
				&_ModuleData.Devs[DevIndex] /* wird dem CallBack mit gegeben*/) != 0 )
	{
		printk(KERN_ERR MODDEBUGOUTTEXT" devm_request_irq() failed!\n"); return -EINVAL;
	}
	pr_devel(MODDEBUGOUTTEXT"  IRQ: %d (mapped)\n",IRQNumber);



	//> Unit einschalten (ohne kann z.B. nicht auf dessen IOMem zugeriffen werden)
	/**********************************************************************/
	//Enabling module functional clock
	pm_runtime_enable(&pdev->dev);

	//increment the device's usage counter,
	if( pm_runtime_get_sync(&pdev->dev) < 0 ) {
		printk(KERN_ERR MODDEBUGOUTTEXT" pm_runtime_get_sync() failed!\n"); return -EINVAL;
	}




	//>dev init & fügt es hinzu
	/**********************************************************************/
	cdev_init(&_ModuleData.Devs[DevIndex].VSDrv_CDev, &VSDrv_fops);
	_ModuleData.Devs[DevIndex].VSDrv_CDev.owner = THIS_MODULE;
	_ModuleData.Devs[DevIndex].VSDrv_CDev.ops 	= &VSDrv_fops;	//notwendig in den quellen wird fops gesetzt?

	//fügt ein device hinzu, nach der fn können FileFns genutzt werden
	res = cdev_add(&_ModuleData.Devs[DevIndex].VSDrv_CDev, _ModuleData.Devs[DevIndex].VSDrv_DeviceNumber, 1/*wie viele ab startNum*/);
	if(res < 0)
		printk(KERN_WARNING MODDEBUGOUTTEXT" can't add device!\n");
	else
		_ModuleData.Devs[DevIndex].VSDrv_IsCDevOpen = TRUE;


	//> in Sysfs class eintragen
	/**********************************************************************/			
	//war mal class_device_create
	if( !IS_ERR(_ModuleData.pModuleClass) )
	{		
		char devName[128];
		struct device *temp;

		sprintf(devName, "%s%d", MODMODULENAME, MINOR(_ModuleData.Devs[DevIndex].VSDrv_DeviceNumber));
		temp = device_create(
				_ModuleData.pModuleClass, 	/* die Type classe */
				NULL, 			/* pointer zum Eltern, dann wird das dev ein Kind vom parten*/
				_ModuleData.Devs[DevIndex].VSDrv_DeviceNumber, /* die nummer zum device */
				NULL,
				devName			/*string for the device's name */
				);

		if( IS_ERR(temp))
			printk(KERN_WARNING MODDEBUGOUTTEXT" can't create sysfs device!\n");
	}


	// init von allem ist durch
	printk(KERN_INFO MODDEBUGOUTTEXT" AM473X probe done (%d:%d)\n",
		MAJOR(_ModuleData.Devs[DevIndex].VSDrv_DeviceNumber), MINOR(_ModuleData.Devs[DevIndex].VSDrv_DeviceNumber));	
	_ModuleData.boIsMinorUsed[DevIndex] = TRUE;

	return 0;
}



/****************************************************************************************************************/
//wird aufgerufen wenn das AM473Xdev removed wird
/****************************************************************************************************************/
int VSDrv_AM473X_remove(struct platform_device *pdev)
{
	PDEVICE_DATA pDevData = (PDEVICE_DATA)platform_get_drvdata(pdev);
	
	//Note: wenn AM473X_remove() aufgerufen wird, 
	// darf kein UserThread mehr im Teiber sein bzw. noch reinspringen weil sonst... bum 	
    // IRQ & VPFERegs (ioremap) sind per devm_ (daher wie auto_ptr:-)
	//http://haifux.org/lectures/323/haifux-devres.pdf
	pr_devel(MODDEBUGOUTTEXT" VSDrv_AM473X_remove (%d:%d)\n", MAJOR(pDevData->VSDrv_DeviceNumber), MINOR(pDevData->VSDrv_DeviceNumber));
	if(pDevData == NULL){
		printk(KERN_WARNING MODDEBUGOUTTEXT" device pointer is zero!\n"); return -EFAULT;}

	//-> VPFE stoppen, User raushohlen, Buffer verschieben
	// aber keine Buffer freigeben da der User sie noch gemapped haben könnte (und wir auch dem pVMKernel nicht haben)
	VSDrv_VPFE_Abort(pDevData);

	
	//power off
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);


	//device in der sysfs class löschen
	if(!IS_ERR(_ModuleData.pModuleClass))	
		device_destroy(_ModuleData.pModuleClass, pDevData->VSDrv_DeviceNumber);

	//device löschen
	if(pDevData->VSDrv_IsCDevOpen)
		cdev_del(&pDevData->VSDrv_CDev);
	pDevData->VSDrv_IsCDevOpen = FALSE;

	
	pDevData->VSDrv_State = VSDRV_STATE_PREINIT;
	return 0;
}



/****************************************************************************************************************/
//fügt wenn ins FIFO den buffer hinzu (wenn STATE_RUNNING & noch frei)
//versucht dann auch einen Buffer in die VPFE Unit zu ädden
/****************************************************************************************************************/
int VSDrv_VPFE_AddBuffer(PDEVICE_DATA pDevData, dma_addr_t pDMAKernelBuffer)
{
	unsigned long irqflags;
	int result = 0;
	spin_lock_irqsave(&pDevData->VSDrv_SpinLock, irqflags);
//----------------------------->

	//> müssen im State running sein
	if( pDevData->VSDrv_State != VSDRV_STATE_RUNNING){
		printk(KERN_WARNING MODDEBUGOUTTEXT "VSDrv_BUF_Alloc> state must be VSDRV_STATE_RUNNING!\n"); result=-EBUSY;
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
				memset(&tmpJob, 0, sizeof(tmpJob));	/* wegen warning */	
				tmpJob.pDMA = pDMAKernelBuffer;
				if( kfifo_put(&pDevData->FIFO_JobsToDo, tmpJob) == 0 ){
					printk(KERN_WARNING MODDEBUGOUTTEXT" Locked_ioctl> can't add into FIFO_JobsToDo\n"); result = -ENOMEM;}
			}//platz im FIFO_JobsDone
		}//platz im FIFO_JobsToDo
	}//if STATE_RUNNING


	//> versuchen Buffer der Unit zu übergeben
	VSDrv_VPFE_TryToAddNextBuffer_locked(pDevData);

//<-----------------------------
	spin_unlock_irqrestore(&pDevData->VSDrv_SpinLock, irqflags);


	return result;
}



/****************************************************************************************************************/
//Achtung! wird vom HW-ISR aufgerufen
// wenn Unit frei & läuft, und ein Buffer FIFO_JobsToDo dann der Unit übergeben
/****************************************************************************************************************/
void VSDrv_VPFE_TryToAddNextBuffer_locked(PDEVICE_DATA pDevData)
{	
	VPFE_JOB tmpJob;

	//> Unit am laufen? und Frei? und Buffer im FIFO
	if(pDevData->VSDrv_State != VSDRV_STATE_RUNNING)
		return;
	if(vpfe_reg_read(pDevData, VPFE_SDR_ADDR) != 0x0000)
		return;
	if( kfifo_len( &pDevData->FIFO_JobsToDo) == 0 )
		return;

	//> ok, Buffer aus FIFO der Unit adden
	if( kfifo_get(&pDevData->FIFO_JobsToDo, &tmpJob) != 1) /*sicher ist sicher*/
		{ pr_devel(MODDEBUGOUTTEXT"VSDrv_VPFE_TryToAddNextBuffer_locked> kfifo_get() failed!\n"); return;}
	vpfe_reg_write(pDevData, tmpJob.pDMA, VPFE_SDR_ADDR);

	smp_mb();//ist nicht notwendig?

	//> nächstes Bild darf ins RAM
	vpfe_wen_enable(pDevData, TRUE);

	//pr_devel(MODDEBUGOUTTEXT" VSDrv_VPFE_TryToAddNextBuffer_locked> add Buffer: 0x%llx\n", (u64)tmpJob.pDMA);
}



/****************************************************************************************************************/
// - hält Unit an, 
// - bricht alle Waiter ab, (bleibt im signaled state)
// - alle Buffer sind dann in FIFO_JobsDone,
// - neuer STATE_PREINIT  
/****************************************************************************************************************/
int VSDrv_VPFE_Abort(PDEVICE_DATA pDevData)
{
	const u32 MAX_WAIT_TIME_US = 100*1000;
	u32 TimeDone_us = 0;
	unsigned long irqflags;
	int result = 0;


	spin_lock_irqsave(&pDevData->VSDrv_SpinLock, irqflags);
//----------------------------->

	//> hält die UNIT an
	//sicher ist sicher (damit wir in die regs schreiben können)
	vpfe_reg_write(pDevData, (VPFE_CONFIG_EN_ENABLE << VPFE_CONFIG_EN_SHIFT), VPFE_CONFIG);


	//nächstes Bild darf nicht mehr ins RAM 
	vpfe_wen_enable(pDevData, FALSE); //"This bit is latched by VD."


	//> das Stoppen Syncen 
	// - wenn ein Pointer übergeben wurde > Job mit Pointer erzeugen und FIFO adden (wir halten Lock daher kann es keiner entnehmen)
	// - auf Ende warten (BusyWaiting nicht schön, kann max ein Bild dauern)
	//
	//" The BUSY status bit in the VPFE_PCR register is set when the start of frame occurs (if the ENABLE bit in
	//  the VPFE_PCR register is 1 at that time). It automatically resets to 0 at the end of a frame."
	//

	//Pointer übergeben Bild? (Unit könnte gerade noch rein schreiben ist aber kein Problem, da sync)
	if( vpfe_reg_read(pDevData, VPFE_SDR_ADDR) != 0x0000 )
	{
		VPFE_JOB tmpJob;
		tmpJob.pDMA			= vpfe_reg_read(pDevData, VPFE_SDR_ADDR);
		tmpJob.boIsOk 		= FALSE;
		tmpJob.ISRCounter 	= pDevData->VPFE_ISRCounter;

		if( kfifo_put(&pDevData->FIFO_JobsDone, tmpJob) == 0 ){
			printk(KERN_WARNING MODDEBUGOUTTEXT" VSDrv_VPFE_Abort> can't add into FIFO_JobsDone\n");}
		pr_devel(MODDEBUGOUTTEXT" VSDrv_VPFE_Abort> new Buffer: 0x%llx, (seq: %d) done\n", (u64)tmpJob.pDMA, tmpJob.ISRCounter);
	}
	vpfe_reg_write(pDevData, 0x0000, VPFE_SDR_ADDR); //"This bit field is latched by VD."


	//warten das die Unit steht
	while( TimeDone_us < MAX_WAIT_TIME_US)
	{	
		//Unit ON & Busy?
		if( (vpfe_reg_read(pDevData, VPFE_PCR) & 0x3) != 0x3 ) 
			break;

		//warten 
		udelay(100);
		TimeDone_us +=100;
	}

	//IRQ Flags löschen & abschalten
	vpfe_reg_write(pDevData, VPFE_VDINT2+VPFE_VDINT1+VPFE_VDINT0, VPFE_IRQ_EN_CLR);
	vpfe_reg_write(pDevData, VPFE_VDINT2+VPFE_VDINT1+VPFE_VDINT0, VPFE_IRQ_STS);
	vpfe_reg_write(pDevData, 1, VPFE_IRQ_EOI);
	
	//Unit abschalten 
	vpfe_pcr_enable(pDevData, 0);


	//> alle Buffer aus .FIFO_JobsToDo >> .FIFO_JobsDone 
	// im VSDrv_VPFE_AddBuffer() passen wir auf das alle Buffer aus FIFO_JobsToDo in FIFO_JobsDone passen
	while ( kfifo_len(&pDevData->FIFO_JobsToDo) >= 1 )
	{
		VPFE_JOB tmpJob;

		//siche ist sicher (noch Platz im FIFO?)
		if( kfifo_avail(&pDevData->FIFO_JobsDone) < 1){
			printk(KERN_WARNING MODDEBUGOUTTEXT" VSDrv_VPFE_Abort> FIFO_JobsDone is full!\n"); result = -ENOMEM; break;}

		//Element aus FIFO_JobsToDo nehmen
		if( kfifo_get(&pDevData->FIFO_JobsToDo, &tmpJob) == 1) 
		{
			tmpJob.boIsOk = FALSE; //damit wissen wir später das es ein Abort war

			if( kfifo_put(&pDevData->FIFO_JobsDone, tmpJob) == 0){
				printk(KERN_WARNING MODDEBUGOUTTEXT" VSDrv_VPFE_Abort> kfifo_put() failed!\n"); result = -EINTR; break;}
			else
				pr_devel(MODDEBUGOUTTEXT" - move Buffer FIFO_JobsToDo >> FIFO_JobsDone [0x%llx]\n",(u64)tmpJob.pDMA);			
		}
		else{
			printk(KERN_WARNING MODDEBUGOUTTEXT" VSDrv_VPFE_Abort> kfifo_get() failed!\n"); result = -EINTR; break;}

	}//while FIFO_JobsToDo not empty


	//> alle Waiter aufwecken
	//Note: 
	//" reinit_completion ... This is especially important after complete_all() is used."
	//" Waiting threads wakeup order is the same in which they were enqueued (FIFO order)."	
	complete_all(&pDevData->FIFO_Waiter);


	//> neuer State
	pDevData->VSDrv_State = VSDRV_STATE_PREINIT;

 //<-----------------------------
	spin_unlock_irqrestore(&pDevData->VSDrv_SpinLock, irqflags);


	return result;
}



/****************************************************************************************************************/
//- konfiguriert die Unit
//- reset des Waiters 
//- neuer State (Running)
/****************************************************************************************************************/
int VSDrv_VPFE_Configure(PDEVICE_DATA pDevData)
{
	unsigned long irqflags;
	int result =0;

	spin_lock_irqsave(&pDevData->VSDrv_SpinLock, irqflags);
//----------------------------->

	//> steht die Unit noch?
	if( pDevData->VSDrv_State != VSDRV_STATE_PREINIT)
		{printk(KERN_WARNING MODDEBUGOUTTEXT "VSDrv_VPFE_Configure> state must be VSDRV_STATE_PREINIT!\n"); result = -EBUSY;}
	else
	{
		u32 iReg, regVal;

		//sollte nicht notwendig sein
		pDevData->VPFE_IsStartFrameDone=FALSE;

		//> Unit config
		/**********************************************************/
		//VPFE Master OCP interface enable
		vpfe_reg_write(pDevData, (VPFE_CONFIG_EN_ENABLE << VPFE_CONFIG_EN_SHIFT), VPFE_CONFIG);

		//Disable CCDC
		vpfe_pcr_enable(pDevData, 0);

		//"defaults" (VPFE_CONFIG=0x108)
		for (iReg = 4; iReg <= 0x94; iReg += 4)
			vpfe_reg_write(pDevData, 0,  iReg);
		vpfe_reg_write(pDevData, VPFE_NO_CULLING, VPFE_CULLING);

		//IRQ Flags löschen & abschalten
		vpfe_reg_write(pDevData, VPFE_VDINT2+VPFE_VDINT1+VPFE_VDINT0, VPFE_IRQ_EN_CLR);
		vpfe_reg_write(pDevData, VPFE_VDINT2+VPFE_VDINT1+VPFE_VDINT0, VPFE_IRQ_STS);
		vpfe_reg_write(pDevData, 1, VPFE_IRQ_EOI);


		//Disable latching function registers on VSYNC (Sensor könnte ja stehen)
		vpfe_reg_write(pDevData, VPFE_LATCH_ON_VSYNC_DISABLE, VPFE_CCDCFG);

		
		regVal = /*VPFE_WEN_ENABLE noch nicht*/ 	/* Data write enable, this bit is latched by VD */
				  VPFE_VDHDEN_ENABLE	/* VD/HD enable, this bit should be set to 1 when HD and VD signals are used at any time */
				+ ((pDevData->VPFE_Is16BitPixel)?(VPFE_SYN_MODE_16BITS):(VPFE_SYN_MODE_8BITS))	/* kein shift nur Maske */
				+ ((pDevData->VPFE_Is16BitPixel)?(0):(VPFE_DATA_PACK_ENABLE));	/* 0 = PACK8 = Normal (16 bits/pixel), 1 = Pack to 8 bits/pixel */
			/* durch  0. 
			 *  - FLDSTAT=Odd field (egal weil progressive)
			 *  - LPF 	 = 3-tap low-pass (anti-aliasing) filter 0ff 
			 *  - INPMOD = RAW Data
			 *  - FLDMODE= Non-interlaced (progressive)
			 *  - DATAPOL= Input data polarity, Normal (no charge)
			 *  - EXWEN	 = Do not use external WEN (write enable)
			 *  - FLDPOL/HDPOL/VDPOL = polarity, Positive
			 *  - FLDOUT = Field ID Direction, Input
			 *  - VDHDOUT= VD/HD Sync Direction, Input*/
		vpfe_reg_write(pDevData, regVal, VPFE_SYNMODE);

		vpfe_reg_write(pDevData, pDevData->VPFE_Width-1, VPFE_HORZ_INFO);	//Anzahl pixel horizontal ( ==> RegWert+1 <==)
		vpfe_reg_write(pDevData, pDevData->VPFE_Height-1, VPFE_VERT_LINES);	//Anzahl Pixel vertical   ( ==> RegWert+1 <==)
		regVal = (pDevData->VPFE_Is16BitPixel)?(pDevData->VPFE_Width*2):(pDevData->VPFE_Width);
		vpfe_reg_write(pDevData, regVal, VPFE_HSIZE_OFF);					//Offset im RAM pro zeile (ACHTUNG muss auf 32Byte ausgerichtet sein)


		//IRQ, nach der 1 und nach der letzten Zeile
		regVal = (0 << VPFE_VDINT_VDINT0_SHIFT) + ( (pDevData->VPFE_Height-1) << 0);
		vpfe_reg_write(pDevData, regVal, VPFE_VDINT);
		vpfe_reg_write(pDevData, VPFE_VDINT0+VPFE_VDINT1, VPFE_IRQ_EN_SET);

		//Enable latching function registers on VSYNC (TI empfiehlt das dringend)
		vpfe_reg_write(pDevData, vpfe_reg_read(pDevData, VPFE_CCDCFG) & (~VPFE_LATCH_ON_VSYNC_DISABLE), VPFE_CCDCFG);

#ifdef DEBUG		
		vpfe_reg_dump(pDevData);
#endif

		//enable unit (ab jetzt kommen IRQs [wenn der Sensor läuft])
		vpfe_pcr_enable(pDevData, 1);



		//> für den Fall das ein Abort() zuvor gelaufen war
		/**********************************************************/
		reinit_completion(&pDevData->FIFO_Waiter);

		//> neuer State ist running
		/**********************************************************/
		pDevData->VSDrv_State = VSDRV_STATE_RUNNING;

	}//state == VSDRV_STATE_PREINIT

//<-----------------------------
	spin_unlock_irqrestore(&pDevData->VSDrv_SpinLock, irqflags);


	return result;
}
