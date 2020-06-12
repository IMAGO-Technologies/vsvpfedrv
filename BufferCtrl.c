/*
 * VisionSensor PV VPFE driver - image buffer control
 *
 * Copyright (C) IMAGO Technologies GmbH
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
 */

#include "VSDrv.h"

/****************************************************************************************************************/
//legt DMASpeicher an
/****************************************************************************************************************/
int VSDrv_BUF_Alloc(PDEVICE_DATA pDevData, void** ppVMKernel, dma_addr_t * ppDMAKernel, size_t *panzBytes)
{
	unsigned long irqflags;
	unsigned int i;

	//> die AOI muss gültig und fix sein
	if (pDevData->VSDrv_State != VSDRV_STATE_RUNNING) {
		dev_warn(pDevData->dev, "VSDrv_BUF_Alloc: state must be VSDRV_STATE_RUNNING!\n");
		return -EBUSY;
	}

	//Bild größe errechnen
	*panzBytes = pDevData->VPFE_Width * pDevData->VPFE_Height;
	*panzBytes *= (pDevData->VPFE_Is16BitPixel) ? (2) : (1);
	*panzBytes = PAGE_ALIGN(*panzBytes);

	// Allocate buffer from CMA. We need no kernel mapping, the returned value is only a cookie:
	*ppVMKernel = dma_alloc_attrs(pDevData->dev, *panzBytes, ppDMAKernel, GFP_KERNEL, DMA_ATTR_NO_KERNEL_MAPPING);
	
	if( (*ppVMKernel) == NULL ) {
		dev_warn(pDevData->dev, "VSDrv_BUF_Alloc: dma_alloc_attrs() failed!\n");
		return -ENOMEM;
	}

	dev_dbg(pDevData->dev, "VSDrv_BUF_Alloc: pVM: 0x%p, pDMA: 0x%llx, Bytes: %zu\n", *ppVMKernel, (u64)(*ppDMAKernel), *panzBytes);

	// Store buffer context
	spin_lock_irqsave(&pDevData->VSDrv_SpinLock, irqflags);
	for (i = 0; i < MAX_VPFE_JOBFIFO_SIZE; i++) {
		if (pDevData->dma_buffer[i].pVMKernel == NULL) {
			pDevData->dma_buffer[i].pVMKernel = *ppVMKernel;
			pDevData->dma_buffer[i].pDMAKernel = *ppDMAKernel;
			pDevData->dma_buffer[i].anzBytes = *panzBytes;
			break;
		}
	}
	spin_unlock_irqrestore(&pDevData->VSDrv_SpinLock, irqflags);
	if (i == MAX_VPFE_JOBFIFO_SIZE) {
		dev_warn(pDevData->dev, "VSDrv_BUF_Alloc: Maximum number of bufferes exceeded!\n");
		VSDrv_BUF_Free(pDevData, *ppVMKernel, *ppDMAKernel, *panzBytes);
		return -ENOMEM;
	}

	return 0;
}


/****************************************************************************************************************/
//gibt den Speicher frei(keie Prüfung ob gültig)
/****************************************************************************************************************/
void VSDrv_BUF_Free(PDEVICE_DATA pDevData, void* pVMKernel, dma_addr_t pDMAKernel, size_t anzBytes)
{
	dev_dbg(pDevData->dev, "free buffer VM: 0x%p, DMA: 0x%x, bytes: %zu\n", pVMKernel, (u32)pDMAKernel, anzBytes);
	dma_free_attrs(pDevData->dev, anzBytes, pVMKernel, pDMAKernel, DMA_ATTR_NO_KERNEL_MAPPING);
}


/****************************************************************************************************************/
//wird aufgerufen wenn der user mmap aufruft
//https://stackoverflow.com/questions/9798008/connection-between-mmap-user-call-to-mmap-kernel-call
/****************************************************************************************************************/
int VSDrv_BUF_mmap(struct file *pFile, struct vm_area_struct *vma)
{
	PDEVICE_DATA pDevData = (PDEVICE_DATA)pFile->private_data;
	size_t size = vma->vm_end - vma->vm_start;	
	phys_addr_t phys_addr = (phys_addr_t)vma->vm_pgoff << PAGE_SHIFT; // vm_pgoff; <>  "Offset (within vm_file) in PAGE_SIZE"

	//Notes:
	//https://stackoverflow.com/questions/6967933/mmap-mapping-in-user-space-a-kernel-buffer-allocated-with-kmalloc
	//https://www.oreilly.de/german/freebooks/linuxdrive2ger/memmap.html
	//
	//https://static.lwn.net/images/pdf/LDD3/ch15.pdf
	// "pfn the page frame number is simply the physical address right-shifted by PAGE_SHIFT"
	//
	// remap_pfn_range " this is only safe if the mm semaphore is held when called" 
	// https://www.kernel.org/doc/htmldocs/kernel-api/API-remap-pfn-range.html
	//
	// aber keiner macht das
	// https://stackoverflow.com/questions/34516991/how-to-acquire-the-mm-semaphore-in-c
	// http://elixir.free-electrons.com/linux/latest/source/drivers/char/mem.c
	//  und wen hier down_write(&vma->vm_mm->mmap_sem); aufgerufen wird dann dead lock!

	vma->vm_page_prot = __pgprot_modify(vma->vm_page_prot, L_PTE_MT_MASK, L_PTE_MT_WRITEBACK | L_PTE_XN);

	//> mappen
 	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot) < 0) {
		dev_warn(pDevData->dev, "VSDrv_BUF_mmap: remap_pfn_range() failed!\n");
		return -EAGAIN;
	}

	vma->vm_ops = NULL;	//sicher ist sicher

	dev_dbg(pDevData->dev, "VSDrv_BUF_mmap: pVM: 0x%llx, pDMA: 0x%llx, bytes: %zu, flags: 0x%X\n", (u64)(vma->vm_start), (u64)(phys_addr), size, (uint) pgprot_val(vma->vm_page_prot) );

	return 0;	
}



/****************************************************************************************************************/
//wartet max. die übergebene Zeit auf ein neuen Buffer (eagl ob im RunningState)
// 4 Mölgichkeiten
//  Fehler:		res <0 (Rest dont'care)
//  TimeOut:	res =0 (ptr=0, Rest don't care)
//  Abort:		res =0 (prt=valid, IsBroken=true Rest don't care)
//  Ok:			res =0 (ptr=valid, IsBroken=false, SeqNr=valid)
/****************************************************************************************************************/
int VSDrv_BUF_WaitFor(PDEVICE_DATA pDevData, const u32 TimeOut_ms, u32 *pIsBroken, u32 *pImageNumber, dma_addr_t *ppDMAKernel)
{
	unsigned long irqflags;
	VPFE_JOB job;
	int result = 0;

	//> sind die FIFOs gültig?
	if (pDevData->VSDrv_State != VSDRV_STATE_PREINIT && pDevData->VSDrv_State != VSDRV_STATE_RUNNING) {
		dev_warn(pDevData->dev, "VSDrv_BUF_WaitFor: state must be VSDRV_STATE_PREINIT or RUNNING!\n");
		return -EBUSY;
	}

	//> warten
	// aufwachen durch Signal, complete() vom HWI, oder User [Abort], bzw TimeOut
	// Achtung! wenn im VSDrv_VPFE_Abort() complete_all() aufgerufen wurde dann springen wait_for... direkt zurück
	
	//damit wir beim TimeOut einfach zurück springen können
	*ppDMAKernel = 0; 

	if (TimeOut_ms == 0xFFFFFFFF)
	{	
		//Note: unterbrechbar(durch gdb), abbrechbar durch kill -9 & kill -15(term) 
		//  noch ist nix passiert, Kernel darf den Aufruf wiederhohlen ohne den User zu benachrichtigen							
		if (wait_for_completion_interruptible(&pDevData->FIFO_Waiter) != 0) {
			dev_dbg(pDevData->dev, "VSDrv_BUF_WaitFor: wait_for_completion_interruptible() failed\n");
			return -ERESTARTSYS;
		}
	}
	else if ( TimeOut_ms == 0 )
	{
		if (!try_wait_for_completion(&pDevData->FIFO_Waiter)) {
			dev_dbg(pDevData->dev, "VSDrv_BUF_WaitFor: try_wait_for_completion() timeout\n");
			return 0;
		}
	}
	else
	{
		//Note: nicht (durch GDB) abbrechbar > wait_for_common(x, timeout, TASK_UNINTERRUPTIBLE);
		unsigned long jiffiesTimeOut = msecs_to_jiffies(TimeOut_ms);
		int waitRes = wait_for_completion_timeout(&pDevData->FIFO_Waiter, jiffiesTimeOut);
		if (waitRes == 0) {
			dev_dbg(pDevData->dev, "VSDrv_BUF_WaitFor: wait_for_completion_timeout() timeout\n");
			return 0;
		}
		else if (waitRes < 0) {
			dev_warn(pDevData->dev, "VSDrv_BUF_WaitFor: wait_for_completion_timeout() failed\n");
			return -EINTR;
		}
	}


	//> Buffer aus FIFO entnehmen (warten war erfolgreich [könnte aber auch durch ein abort sein])
	spin_lock_irqsave(&pDevData->VSDrv_SpinLock, irqflags);
//----------------------------->
	//beim Abort, muss es keinen Job im FIFO geben
	*pIsBroken 	= TRUE;

	//im FIFO was drin?
	if (kfifo_get(&pDevData->FIFO_JobsDone, &job)) {
		*pIsBroken 		= !job.boIsOk;
		*pImageNumber	= job.ISRCounter;
		*ppDMAKernel	= job.pDMA;
	}
	else {
		dev_dbg(pDevData->dev, "VSDrv_BUF_WaitFor: wake up without buffer\n");
	}
//<-----------------------------
	spin_unlock_irqrestore(&pDevData->VSDrv_SpinLock, irqflags);

	// handle cache
	if (*ppDMAKernel != 0) {
		size_t dma_size = pDevData->VPFE_Width * pDevData->VPFE_Height;
		if (pDevData->VPFE_Is16BitPixel)
			dma_size *= 2;
		dma_size = PAGE_ALIGN(dma_size);
		dma_sync_single_for_cpu(pDevData->dev, *ppDMAKernel, dma_size, DMA_FROM_DEVICE);
	}

	return result;
}
