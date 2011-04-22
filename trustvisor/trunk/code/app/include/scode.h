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

/* scode.h - Module specific definitions
 * Written for TrustVisor by Ning Qu and Mark Luk
 * Edited by Zongwei Zhou for EMHF project
 */

#ifndef __SCODE_H_
#define __SCODE_H_

#include  <target.h>
#include <perf.h>
#include <pages.h>
#include <hpt.h>
//#include  <globals.h>
//#include "tpm_sw.h"

#if 0
#include "rsa.h" // for global rsa key
#include "arc4.h"  // for arc4_context in whitelist_entry, by yanlinl at Jul 28, 2009
#include "tpm_sw.h"
#include "params.h"

#ifndef STPM_RANDOM_BUFFER_SIZE
#define STPM_RANDOM_BUFFER_SIZE 128
#endif
#endif 

/* 
 * definition for scode sections info 
 * */
#define MAX_SECTION_NUM 10 
#define  MAX_REGPAGES_NUM 50

/* secure code: RWX for PAL, not-present for insecure app - measured */
#define  SECTION_TYPE_SCODE 1
/* static data: RW \ np - measured */
#define  SECTION_TYPE_SDATA 2
/* marshalled parameters: RW \ np - not measured */
#define  SECTION_TYPE_PARAM 3
/* PAL's stack: RW \ np - not measured */
#define  SECTION_TYPE_STACK 4
/* shared text segment: RWX \ RX - measured */
#define  SECTION_TYPE_STEXT 5
/* shared segment: RWX\np when pal is running, np\RWX otherwise */
#define  SECTION_TYPE_SHARED 6
/* guest page tables. (temporarily) used internally */
#define	 SECTION_TYPE_GUEST_PAGE_TABLES 7

/* bits 0 to 2 of stored pte's store the section type */
#define SCODE_PTE_TYPE_MASK (0x7ull)
#define SCODE_PTE_TYPE_GET(pte)    ((pte) & SCODE_PTE_TYPE_MASK)
#define SCODE_PTE_TYPE_SET(pte, t) (((pte) & ~SCODE_PTE_TYPE_MASK) | t)

struct scode_sections_struct{
	int type;  
	unsigned int start_addr;
	int page_num;
};

struct scode_sections_info{
	int section_num;
	struct scode_sections_struct ps_str[MAX_SECTION_NUM];
};


/* 
 * definition for scode param info 
 * */
#define MAX_PARAMS_NUM 10

/* parameter type */
#define PM_TYPE_INTEGER 1 /* integer */
#define PM_TYPE_POINTER 2 /* pointer */


struct scode_params_struct{
  u32 type;  /* 1: integer ;  2:pointer*/
  u32 size;
};

struct scode_params_info{
  u32 params_num;
  struct scode_params_struct pm_str[MAX_PARAMS_NUM];
};

/* 
 * definition for scode whitelist 
 * */
/* max size of memory that holds scode state */
#define  WHITELIST_LIMIT 8*1024

/* in order to support 4GB memory */
#define  PFN_BITMAP_LIMIT 512*1024
#define  PFN_BITMAP_2M_LIMIT 2*1024


/* softTPM related definitions */
#define TPM_PCR_SIZE                   20
#define TPM_PCR_NUM                    8
#define TPM_AES_KEY_LEN                128 /* key size is 128 bit */
#define TPM_HMAC_KEY_LEN               20

/* max len of sealed data */
#define  MAX_SEALDATA_LEN 2048
#define  SEALDATA_HEADER_LEN 48

extern perf_ctr_t g_tv_perf_ctrs[];
extern char *g_tv_perf_ctr_strings[];
#define TV_PERF_CTR_NPF 0
#define TV_PERF_CTR_SWITCH_SCODE 1
#define TV_PERF_CTR_SWITCH_REGULAR 2
#define TV_PERF_CTR_SAFEMALLOC 3
#define TV_PERF_CTR_MARSHALL 4
#define	TV_PERF_CTR_EXPOSE_ARCH 5
#define TV_PERF_CTR_NESTED_SWITCH_SCODE 6
/* insert new counters here, and update count below */
#define TV_PERF_CTRS_COUNT 7

/* scode state struct */
typedef struct whitelist_entry{
	u64 gcr3; 
	u32 id;
	u32 grsp;		/* guest reguar stack */
	u32 gssp;		/* guest sensitive code stack */
	u32 gss_size;   /* guest sensitive code stack page number */
	u32 entry_v; /* entry point virutal address */
	u32 entry_p; /* entry point physical address */
	u32 return_v; /* return point virtual address */

	u32 gpmp;     /* guest parameter page address */
	u32 gpm_size; /* guest parameter page number */
	u32 gpm_num;  /* guest parameter number */

	struct scode_sections_info scode_info; /* scode_info struct for registration function inpu */
	struct scode_params_info params_info; /* param info struct */
	pte_t* scode_pages; /* registered pte's (copied from guest page tables and additional info added) */
	u32 scode_size; /* scode size */

	pte_t * pte_page;  /* holder for guest page table entry to access scode and GDT */
	u32 pte_size;	/* total size of all PTE pages */
#ifdef __MP_VERSION__
	u32 pal_running_lock; /* PAL running lock */
	u32 pal_running_vcpu_id; /* the cpu that is running this PAL */
#endif

	/* software TPM related */
	u8  pcr[TPM_PCR_SIZE*TPM_PCR_NUM];

	/* pal page tables */
	pagelist_t pl;
	hpt_walk_ctx_t hpt_nested_walk_ctx;
	hpt_pm_t pal_hpt_root;
} __attribute__ ((packed)) whitelist_entry_t;


/* 
 * definition for VMM call
 * */
enum VMMcmd
{
	VMMCMD_REG		=1,
	VMMCMD_UNREG	=2,
	VMMCMD_SEAL		=3,
	VMMCMD_UNSEAL	=4,
	VMMCMD_QUOTE	=5,
	VMMCMD_SHARE	=6,
	VMMCMD_PCRREAD	=7,
	VMMCMD_PCREXT	=8,
	VMMCMD_GENRAND	=9,
	VMMCMD_TEST		=255,
};

/* template page table context */
extern hpt_walk_ctx_t hpt_nested_walk_ctx;

/* nested paging handlers (hpt) */
void hpt_insert_pal_pmes(VCPU *vcpu,
												 hpt_walk_ctx_t *walk_ctx,
												 hpt_pm_t pal_pm,
												 int pal_pm_lvl,
												 gpa_t gpas[],
												 size_t num_gpas);
void hpt_nested_set_prot(VCPU * vcpu, u64 gpaddr);
void hpt_nested_clear_prot(VCPU * vcpu, u64 gpaddr);
void hpt_nested_switch_scode(VCPU * vcpu, pte_t *pte_pages, u32 size, pte_t *pte_page2, u32 size2);
void hpt_nested_switch_regular(VCPU * vcpu, pte_t *pte_page, u32 size, pte_t *pte_page2, u32 size2);
void hpt_nested_make_pt_unaccessible(pte_t *gpaddr_list, u32 gpaddr_count, pdpt_t npdp, u32 is_pal);
void hpt_nested_make_pt_accessible(pte_t *gpaddr_list, u32 gpaddr_count, u64 * npdp, u32 is_pal);

/* several help functions to access guest address space */
u16 get_16bit_aligned_value_from_guest(VCPU * vcpu, u32 gvaddr);
u32 get_32bit_aligned_value_from_guest(VCPU * vcpu, u32 gvaddr);
void put_32bit_aligned_value_to_guest(VCPU * vcpu, u32 gvaddr, u32 value);

/* guest paging handlers */
int guest_pt_copy(VCPU * vcpu, pte_t *pte_page, u32 gvaddr, u32 size, int type);
#define gpt_vaddr_to_paddr(vcpu, vaddr)	guest_pt_walker_internal(vcpu, vaddr, NULL, NULL, NULL, NULL, NULL, NULL, NULL)
#define  gpt_get_ptpages(vcpu, vaddr, pdp, pd, pt) guest_pt_walker_internal(vcpu, vaddr, pdp, pd, pt, NULL, NULL, NULL, NULL)
#define  gpt_get_ptentries(vcpu, vaddr, pdpe, pde, pte, is_pae) guest_pt_walker_internal(vcpu, vaddr, NULL, NULL, NULL, pdpe, pde, pte, is_pae)

u32 guest_pt_walker_internal(VCPU *vcpu, u32 vaddr, u64 *pdp, u64 *pd, u64 *pt, u64 *pdpe, u64 * pde, u64 * pte, u32 * is_pae);

u32 guest_pt_check_user_rw(VCPU * vcpu, u32 vaddr, u32 page_num);

/* operations from hypervisor to guest paging */
void * __gpa2hva__(u32 gpaddr);
void copy_from_guest(VCPU * vcpu, u8 *dst, u32 gvaddr, u32 len);
void copy_to_guest(VCPU * vcpu, u32 gvaddr, u8 *src, u32 len);

/* PAL operations (HPT) */
u32 hpt_scode_set_prot(VCPU *vcpu, pte_t *pte_pages, u32 size);
void hpt_scode_clear_prot(VCPU * vcpu, pte_t *pte_pages, u32 size);
u32 hpt_scode_switch_scode(VCPU * vcpu);
u32 hpt_scode_switch_regular(VCPU * vcpu);
u32 hpt_scode_npf(VCPU * vcpu, u32 gpaddr, u64 errorcode);

/* PAL operations */
void init_scode(VCPU * vcpu);
u32 scode_register(VCPU * vcpu, u32 scode_info, u32 scode_pm, u32 gventry);
u32 scode_unregister(VCPU * vcpu, u32 gvaddr);
u32 scode_seal(VCPU * vcpu, u32 input_addr, u32 input_len, u32 pcrhash_addr, u32 output_addr, u32 output_len_addr);
u32 scode_unseal(VCPU * vcpu, u32 input_addr, u32 input_len, u32 output_addr, u32 output_len_addr);
u32 scode_quote(VCPU * vcpu, u32 nonce_addr, u32 tpmsel_addr, u32 out_addr, u32 out_len_addr);
u32 scode_share(VCPU * vcpu, u32 scode_entry, u32 addr, u32 len);
#endif /* __SCODE_H_ */
/* Local Variables: */
/* mode:c           */
/* indent-tabs-mode:'t */
/* tab-width:2      */
/* End:             */
