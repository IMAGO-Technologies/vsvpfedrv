/*
 * FileOps.c
 *
 * handel the file read/write/io actions
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

static long do_ioctl(PDEVICE_DATA pDevData, const u32 cmd, u8 __user * pToUserMem, const u32 BufferSizeBytes);


//<====================================>
//	File fns
//<====================================>


/****************************************************************************************************************/
int VSDrv_open(struct inode *node, struct file *filp)
{
	int iMinor;
	if( (node==NULL) || (filp==NULL) )
		return -EINVAL; 
	iMinor = iminor(node);

	pr_devel(MODDEBUGOUTTEXT" open (Minor:%d)\n", iMinor);

	//setzt ins "file" den Context
	if(iMinor >= MAX_DEVICE_COUNT)
		return -EINVAL;
	filp->private_data = &_ModuleData.Devs[iMinor];
	
	return 0;
}

/****************************************************************************************************************/
ssize_t VSDrv_read (struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
	printk(KERN_WARNING MODDEBUGOUTTEXT" read, not supported!\n");
	return -EFAULT;
}

/****************************************************************************************************************/
ssize_t VSDrv_write (struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
	printk(KERN_WARNING MODDEBUGOUTTEXT" write, not supported!\n");
	return -EFAULT;
}



/****************************************************************************************************************/
//Note: alte ioctl war unter "big kernel lock"
//http://lwn.net/Articles/119652/
/****************************************************************************************************************/
long VSDrv_unlocked_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	long ret = 0;
	PDEVICE_DATA pDevData = NULL;

	//Note: beim 1. write bekommen wir ein CMD:1, Type 'T', Size: 0 <-> FIONBIO (linux/include/asm-i386/ioctls.h, line 41)
	pr_devel(MODDEBUGOUTTEXT" ioctl (CMD %d, MAGIC %c, size %d)\n",_IOC_NR(cmd), _IOC_TYPE(cmd), _IOC_SIZE(cmd));

	/* Alles gut */
	if( (filp==NULL) || (filp->private_data==NULL) ) return -EINVAL;
	pDevData = (PDEVICE_DATA) filp->private_data;
	//ist ist das CMD eins für uns?
	if (_IOC_TYPE(cmd) != VSDRV_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > VSDRV_IOC_MAXNR) return -ENOTTY;

	//bei uns ist arg ein Pointer, und testen ob wir ihn nutzen dürfen (richtung aus UserSicht)
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;


	/* jetzt die CMDs auswerten */
	ret = do_ioctl(pDevData, cmd, (u8 __user *) arg, _IOC_SIZE(cmd) );

	return ret;
}


//führt eine IOOp durch, gibt >= 0 für OK sonst fehler code zurück,
long do_ioctl(PDEVICE_DATA pDevData, const u32 cmd, u8 __user * pToUserMem, const u32 BufferSizeBytes)
{
	long result=0;

	switch(cmd)
	{

//	DRV
//==================================================================================
//==================================================================================


		/* Gibt die Version als String zurück */
		/**********************************************************************/
		case VSDRV_IOC_DRV_GET_VERSION:
			if( sizeof(MODVERSION) > BufferSizeBytes){
				printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_GET_VERSION)> Buffer Length to short\n"); result = -EFBIG;
			}
			else
			{
				if( copy_to_user(pToUserMem,MODVERSION,sizeof(MODVERSION)) !=0 ){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_GET_VERSION)> copy_to_user faild\n"); result = -EFAULT;
				}
				else
					result = sizeof(MODVERSION);
			}

			break;


		/* Gibt das Build date/time als String zurück */
		/**********************************************************************/
		case VSDRV_IOC_DRV_GET_BUILD_DATE:
			if( sizeof(MODDATECODE) > BufferSizeBytes){
				printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_GET_BUILD_DATE)> Buffer Length to short\n"); result = -EFBIG;
			}
			else
			{
				if( copy_to_user(pToUserMem,MODDATECODE,sizeof(MODDATECODE)) !=0 ){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_GET_BUILD_DATE)> copy_to_user faild\n"); result = -EFAULT;
				}
				else
					result = sizeof(MODDATECODE);
			}

		break;


		/* Setzt die AOI */
		/**********************************************************************/
		case VSDRV_IOC_DRV_SET_AOI:
			if( (sizeof(u32)*3)> BufferSizeBytes){
				printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_SET_AOI)> Buffer Length to short\n"); result = -EFBIG;
			}
			else
			{
				unsigned long irqflags;
				u32 tmpWidth, tmpHeight, tmpPixelSize;
				if( get_user(tmpWidth,(u32*)(pToUserMem+0)) != 0){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_SET_AOI)> get_user faild\n"); result = -EFAULT; break;}
				if( get_user(tmpHeight,(u32*)(pToUserMem+4)) != 0){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_SET_AOI)> get_user faild\n"); result = -EFAULT; break;}
				if( get_user(tmpPixelSize,(u32*)(pToUserMem+8)) != 0){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_SET_AOI)> get_user faild\n"); result = -EFAULT; break;}

				if( (tmpWidth==0) || (tmpWidth>4096/*doku steht nix regs sind 16Bit*/) ){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_SET_AOI)> invalid width\n"); result = -EINVAL; break;}
				if( (tmpPixelSize==1) && ((tmpWidth&0x1F)!=0) ){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_SET_AOI)> invalid width (must be multiple 32Bytes)\n"); result = -EINVAL; break;}
				if( (tmpPixelSize==2) && ((tmpWidth&0x0F)!=0) ){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_SET_AOI)> invalid width (must be multiple 16Bytes)\n"); result = -EINVAL; break;}
				if( (tmpHeight==0) || (tmpHeight>4096/*doku steht nix regs sind 16Bit*/) ){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_SET_AOI)> invalid height\n"); result = -EINVAL; break;}
				if( (tmpPixelSize==0) || (tmpPixelSize>2) ){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_SET_AOI)> invalid pixelsize\n"); result = -EINVAL; break;}
				

				spin_lock_irqsave(&pDevData->VSDrv_SpinLock, irqflags);
//----------------------------------------------------------------------------->
				//die UNIT darf noch nicht laufen
				if( pDevData->VSDrv_State == VSDRV_STATE_PREINIT)
				{
					pDevData->VPFE_Width		= tmpWidth;
					pDevData->VPFE_Height		= tmpHeight;
					pDevData->VPFE_Is16BitPixel = (tmpPixelSize==2)?(TRUE):(FALSE);

					pr_devel(MODDEBUGOUTTEXT" - new AOI> width: %d, height: %d, dWord: %c\n", tmpWidth, tmpHeight, (pDevData->VPFE_Is16BitPixel)?('y'):('n') );
				}
				else
					{printk(KERN_WARNING MODDEBUGOUTTEXT "do_ioctl(VSDRV_IOC_SET_AOI)> state must be VSDRV_STATE_PREINIT!\n"); result = -EBUSY;}

//<----------------------------------------------------------------------------
				spin_unlock_irqrestore(&pDevData->VSDrv_SpinLock, irqflags);
			}

		break;



//	VPFE
//==================================================================================
//==================================================================================

		/* startet die Unit (wenn sie noch nicht läuft) */
		/**********************************************************************/
		case VSDRV_IOC_VPFE_START:
			result = VSDrv_VPFE_Configure(pDevData);
			break;


		/* hält Unit an, bricht Waiter ab, Buffer sind dann in FIFO_JobsDone, neuer STATE_PREINIT */
		/**********************************************************************/
		case VSDRV_IOC_VPFE_ABORT:
			result = VSDrv_VPFE_Abort(pDevData);
			break;


		/* fügt einen Buffer den FIFO hinzu und versucht der Unit einen zu adden  */
		/**********************************************************************/
		case VSDRV_IOC_VPFE_ADD_BUFFER:
			if( BufferSizeBytes < (1*sizeof(u64) )){
				printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_VPFE_ADD_BUFFER)> Buffer Length to short\n"); result = -EFBIG;
			}
			else					
			{
				//args lesen
			   	u64 pDMAKernel;
				if( get_user(pDMAKernel,(u64*)(pToUserMem+0)) != 0){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_VPFE_ADD_BUFFER)> get_user faild\n"); result = -EFAULT; break;}
				if( pDMAKernel==0 ){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_VPFE_ADD_BUFFER)> invalid args\n"); result = -EINVAL; break;}

				//gibt den buffer frei
				result = VSDrv_VPFE_AddBuffer(pDevData, (dma_addr_t) pDMAKernel);
		    }

			break;
	



//	Buffer
//==================================================================================
//==================================================================================

		/* legt einen Buffer im Kernel Speicher an  */
		/**********************************************************************/
		case VSDRV_IOC_BUFFER_ALLOC:
			if( BufferSizeBytes < (3*sizeof(u64) )){
				printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_ALLOC)> Buffer Length to short\n"); result = -EFBIG;
			}
			else
			{
				void* 		pVMKernel 	= NULL;
				dma_addr_t 	pDMAKernel	= 0;
			   	size_t 		panzBytes	= 0;

				// Buffer anlegen
				result = VSDrv_BUF_Alloc(pDevData, &pVMKernel, &pDMAKernel, &panzBytes);

		
				// an User schicken (wenn Ok)
				if( result == 0 )
				{
					if( put_user((uintptr_t)pVMKernel, (u64*)(pToUserMem+0)) != 0){
						printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_ALLOC)> put_user faild\n"); result = -EFAULT; break;}
					if( put_user( pDMAKernel, (u64*)(pToUserMem+8)) != 0){
						printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_ALLOC)> put_user faild\n"); result = -EFAULT; break;}
					if( put_user( panzBytes, (u64*)(pToUserMem+16)) != 0){
						printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_ALLOC)> put_user faild\n"); result = -EFAULT; break;}
					result =  3 * sizeof(u64);
				}		
		    }

			break;


		/* gibt einen Buffer (aus dem KernelSpeicher) frei  */
		/**********************************************************************/
		case VSDRV_IOC_BUFFER_FREE:
			if( BufferSizeBytes < (3*sizeof(u64) )){
				printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_FREE)> Buffer Length to short\n"); result = -EFBIG;
			}
			else					
			{
				//args lesen
			   	u64 pVMKernel, pDMAKernel, anzBytes;
				if( get_user(pVMKernel,(u64*)(pToUserMem+0)) != 0){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_FREE)> get_user faild\n"); result = -EFAULT; break;}
				if( get_user(pDMAKernel,(u64*)(pToUserMem+8)) != 0){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_FREE)> get_user faild\n"); result = -EFAULT; break;}
				if( get_user(anzBytes,(u64*)(pToUserMem+16)) != 0){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_FREE)> get_user faild\n"); result = -EFAULT; break;}
		
				if( pVMKernel==0 || pDMAKernel==0 || anzBytes==0 || (!PAGE_ALIGNED(anzBytes))  ){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_FREE)> invalid args\n"); result = -EINVAL; break;}


				//gibt den buffer frei
				VSDrv_BUF_Free(pDevData, (void*) ((uintptr_t)pVMKernel), (dma_addr_t) pDMAKernel, anzBytes);
			
		    }

			break;


		/* wartet die angegebene Zeit auf einen neuen Buffer  */
		/**********************************************************************/
		case VSDRV_IOC_BUFFER_WAIT_FOR:
			if( BufferSizeBytes < (3*sizeof(u64) )){
				printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_WAIT_FOR)> Buffer Length to short\n"); result = -EFBIG;
			}
			else
			{
				u32 		TimeOut_ms, IsBroken, ImageNumber;				
				dma_addr_t 	pDMAKernel;
				//args lesen
				if( get_user(TimeOut_ms,(u32*)(pToUserMem+0)) != 0){
					printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_WAIT_FOR)> get_user faild\n"); result = -EFAULT; break;}


				// auf Buffer warten
				result = VSDrv_BUF_WaitFor(pDevData, TimeOut_ms, &IsBroken, &ImageNumber, &pDMAKernel);

		
				// an User schicken (wenn Ok) [die ersten beiden u64 sind u32 Werte darum 64Bit setzen]
				if( result == 0 )
				{	u64 tmp = IsBroken;
					if( put_user(tmp, (u64*)(pToUserMem+0)) != 0){
						printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_WAIT_FOR)> put_user faild\n"); result = -EFAULT; break;}
					tmp = ImageNumber;
					if( put_user(tmp, (u64*)(pToUserMem+8)) != 0){
						printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_WAIT_FOR)> put_user faild\n"); result = -EFAULT; break;}
					if( put_user(pDMAKernel, (u64*)(pToUserMem+16)) != 0){
						printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(VSDRV_IOC_BUFFER_WAIT_FOR)> put_user faild\n"); result = -EFAULT; break;}
					result =  3 * sizeof(u64);
				}		
		    }

			break;

		//sollte nie sein (siehe oben bzw. Ebene höher)
		default:
			printk(KERN_WARNING MODDEBUGOUTTEXT" do_ioctl(0x%08X)> invalid code!\n", cmd);
			return -ENOTTY;
	}


	return result;
}
