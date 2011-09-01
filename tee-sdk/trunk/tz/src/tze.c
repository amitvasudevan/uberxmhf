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

#include <tze.h>

tz_return_t TZESvcLoadAndOpen(tze_dev_svc_sess_t *sess,
                              const void *kauiSvcData,
                              uint32_t uiSvcDataLength,
                              const tze_svc_load_and_open_options_t *options)
{
  tz_return_t rv;
  const tze_svc_load_and_open_options_t default_options =
    (tze_svc_load_and_open_options_t) { };
  if (!options)
    options = &default_options;

  rv = TZDeviceOpen(options->pkDeviceName, options->pkDeviceInit, &sess->tzDevice);
  if (rv != TZ_SUCCESS)
    return rv;

  /* download service */  
  { 
    tz_session_t tzManagerSession;

    /* open session with device manager */
    rv = TZManagerOpen(&sess->tzDevice, options->pksManagerLogin, &tzManagerSession);
    if (rv != TZ_SUCCESS)
      return rv;

    /* download */
    rv = TZManagerDownloadService(&tzManagerSession,
                                  kauiSvcData,
                                  uiSvcDataLength,
                                  &sess->tzSvcId);
    if (rv != TZ_SUCCESS) {
      TZManagerClose(&tzManagerSession);
      return rv;
    }

    /* close session */
    rv = TZManagerClose(&tzManagerSession);
    if (rv != TZ_SUCCESS)
      return rv;
  }

  /* now open a service handle to the svc */
  {
    tz_operation_t op;
    tz_return_t serviceReturn;
    rv = TZOperationPrepareOpen(&sess->tzDevice,
                                &sess->tzSvcId,
                                options->pksSvcLogin,
                                options->pksSvcTimeLimit,
                                &sess->tzSession,
                                &op);
    if (rv != TZ_SUCCESS) {
      TZDeviceClose(&sess->tzDevice);
      return rv;
    }

    rv = TZOperationPerform(&op, &serviceReturn);
    if (rv != TZ_SUCCESS) {
      TZOperationRelease(&op);
      TZDeviceClose(&sess->tzDevice);
      return rv;
    }

    TZOperationRelease(&op);
  }

  return TZ_SUCCESS;
}

