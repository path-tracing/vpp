/*
 *------------------------------------------------------------------
 * pt_api.c - path tracing ipv6 api
 *
 * Copyright (c) 2016 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *------------------------------------------------------------------
 */

#include <vnet/vnet.h>
#include <vnet/pt/pt.h>
#include <vlibmemory/api.h>

#include <vnet/interface.h>
#include <vnet/api_errno.h>
#include <vnet/feature/feature.h>
#include <vnet/fib/fib_table.h>

#include <vnet/ip/ip_types_api.h>

#include <vnet/vnet_msg_enum.h>

#define vl_typedefs		/* define message structures */
#include <vnet/vnet_all_api_h.h>
#undef vl_typedefs

#define vl_endianfun		/* define message structures */
#include <vnet/vnet_all_api_h.h>
#undef vl_endianfun

/* instantiate all the print functions we know about */
#define vl_print(handle, ...) vlib_cli_output (handle, __VA_ARGS__)
#define vl_printfun
#include <vnet/vnet_all_api_h.h>
#undef vl_printfun

#include <vlibapi/api_helper_macros.h>

#define foreach_vpe_api_msg                                   \
_(PT_IFACE_ADD, pt_iface_add)                                 \
_(PT_IFACE_DEL, pt_iface_del)                                 \
_(PT_PROBE_INJECT_IFACE_ADD, pt_probe_inject_iface_add)       \
_(PT_PROBE_INJECT_IFACE_DEL, pt_probe_inject_iface_del)

static void
vl_api_pt_iface_add_t_handler (vl_api_pt_iface_add_t * mp)
{
  vl_api_pt_iface_add_reply_t *rmp;
  int rv = 0;

  VALIDATE_SW_IF_INDEX (mp);

  rv = pt_iface_add (ntohl (mp->sw_if_index), ntohs (mp->id), mp->ingress_load, mp->egress_load, mp->tts_template);

  BAD_SW_IF_INDEX_LABEL;

  REPLY_MACRO (VL_API_PT_IFACE_ADD_REPLY);
}

static void
vl_api_pt_iface_del_t_handler (vl_api_pt_iface_del_t * mp)
{
  vl_api_pt_iface_del_reply_t *rmp;
  int rv = 0;

  VALIDATE_SW_IF_INDEX (mp);

  rv = pt_iface_del (ntohl (mp->sw_if_index));

  BAD_SW_IF_INDEX_LABEL;

  REPLY_MACRO (VL_API_PT_IFACE_DEL_REPLY);
}

static void
vl_api_pt_probe_inject_iface_add_t_handler (vl_api_pt_probe_inject_iface_add_t * mp)
{
  vl_api_pt_probe_inject_iface_add_reply_t *rmp;
  int rv = 0;

  VALIDATE_SW_IF_INDEX (mp);

  rv = pt_probe_inject_iface_add (ntohl (mp->sw_if_index));

  BAD_SW_IF_INDEX_LABEL;

  REPLY_MACRO (VL_API_PT_PROBE_INJECT_IFACE_ADD_REPLY);
}

static void
vl_api_pt_probe_inject_iface_del_t_handler (vl_api_pt_probe_inject_iface_del_t * mp)
{
  vl_api_pt_probe_inject_iface_del_reply_t *rmp;
  int rv = 0;

  VALIDATE_SW_IF_INDEX (mp);

  rv = pt_probe_inject_iface_del (ntohl (mp->sw_if_index));

  BAD_SW_IF_INDEX_LABEL;

  REPLY_MACRO (VL_API_PT_PROBE_INJECT_IFACE_DEL_REPLY);
}

/*
 * pt_api_hookup
 * Add vpe's API message handlers to the table.
 * vlib has already mapped shared memory and
 * added the client registration handlers.
 * See .../vlib-api/vlibmemory/memclnt_vlib.c:memclnt_process()
 */
#define vl_msg_name_crc_list
#include <vnet/vnet_all_api_h.h>
#undef vl_msg_name_crc_list

static void
setup_message_id_table (api_main_t * am)
{
#define _(id,n,crc) vl_msg_api_add_msg_name_crc (am, #n "_" #crc, id);
  foreach_vl_msg_name_crc_pt;
#undef _
}

static clib_error_t *
pt_api_hookup (vlib_main_t * vm)
{
  api_main_t *am = vlibapi_get_main ();

#define _(N,n)                                                  \
    vl_msg_api_set_handlers(VL_API_##N, #n,                     \
                           vl_api_##n##_t_handler,              \
                           vl_noop_handler,                     \
                           vl_api_##n##_t_endian,               \
                           vl_api_##n##_t_print,                \
                           sizeof(vl_api_##n##_t), 1);
  foreach_vpe_api_msg;
#undef _

  /*
   * Set up the (msg_name, crc, message-id) table
   */
  setup_message_id_table (am);

  return 0;
}

VLIB_API_INIT_FUNCTION (pt_api_hookup);

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
