/*
 * @XMHF_LICENSE_HEADER_START@
 *
 * eXtensible, Modular Hypervisor Framework (XMHF)
 * Copyright (c) 2009-2012 Carnegie Mellon University
 * Copyright (c) 2010-2012 VDG Inc.
 * All Rights Reserved.
 *
 * Developed by: XMHF Team
 *               Carnegie Mellon University / CyLab
 *               VDG Inc.
 *               http://xmhf.org
 *
 * This file is part of the EMHF historical reference
 * codebase, and is released under the terms of the
 * GNU General Public License (GPL) version 2.
 * Please see the LICENSE file for details.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @XMHF_LICENSE_HEADER_END@
 */

/*
 * EMHF base platform component interface, x86 vmx backend
 * author: amit vasudevan (amitvasudevan@acm.org)
 */

#include <emhf.h>

//initialize CPU state
void emhf_baseplatform_arch_x86vmx_cpuinitialize(void){
    	u32 bcr0;
	    txt_heap_t *txt_heap;
        os_mle_data_t *os_mle_data;
  
	    //set bit 5 (EM) of CR0 to be VMX compatible in case of Intel cores
		bcr0 = read_cr0();
		bcr0 |= 0x20;
		write_cr0(bcr0);

        // restore pre-SENTER MTRRs that were overwritten for SINIT launch 
        // NOTE: XXX TODO; BSP MTRRs ALREADY RESTORED IN SL; IS IT
        //   DANGEROUS TO DO THIS TWICE? 
        // sl.c unity-maps 0xfed00000 for 2M so these should work fine 
        txt_heap = get_txt_heap();
        printf("\ntxt_heap = 0x%08x", (u32)txt_heap);
        os_mle_data = get_os_mle_data_start(txt_heap);
        printf("\nos_mle_data = 0x%08x", (u32)os_mle_data);
    
        if(!validate_mtrrs(&(os_mle_data->saved_mtrr_state))) {
             printf("\nSECURITY FAILURE: validate_mtrrs() failed.\n");
             HALT();
        }
        restore_mtrrs(&(os_mle_data->saved_mtrr_state));
}

//VMX specific platform reboot
void emhf_baseplatform_arch_x86vmx_reboot(VCPU *vcpu){

	//shut VMX off, else CPU ignores INIT signal!
	__asm__ __volatile__("vmxoff \r\n");
	write_cr4(read_cr4() & ~(CR4_VMXE));
	
	//fall back on generic x86 reboot
	emhf_baseplatform_arch_x86_reboot();
}

