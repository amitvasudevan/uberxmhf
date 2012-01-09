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

// print.h - debug/non-debug switch header
//author: amit vasudevan (amitvasudevan@acm.org)
#ifndef __PRINT_H_
#define __PRINT_H_

#define print_string putstr

#ifndef __ASSEMBLY__

#define LOG_LEVEL_NONE    0x00
#define LOG_LEVEL_ALL     0xFF

#define LOG_TARGET_NONE   0x00
#define LOG_TARGET_VGA    0x01
#define LOG_TARGET_SERIAL 0x02
#define LOG_TARGET_MEMORY 0x04

extern uint8_t g_log_targets;
extern uint8_t g_log_level;

void init_uart(void);
void putstr(const char *str);

#define LOG_PROFILE (1<<0)
#define LOG_TRACE   (1<<1)
#define LOG_ERROR   (1<<2)

#define ENABLED_LOG_TYPES (LOG_PROFILE|LOG_TRACE|LOG_ERROR)

#ifdef __DEBUG_SERIAL__
extern void printf(const char *format, ...)
  __attribute__ ((format (printf, 1, 2)));
void dprintf(u32 log_type, const char *format, ...)
  __attribute__ ((format (printf, 2, 3)));
void print_hex(const char *prefix, const void *prtptr, size_t size);

#else
#define printf(format, args...) while(0)
#define dprintf(format, args...) while(0)
#define print_hex(prefix, prtptr, size) while(0)
#endif



#endif /* __ASSEMBLY__ */

#endif /* __PRINT_H_ */