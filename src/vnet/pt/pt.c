/*
 * pt.c: Path Tracing (PT)
 *
 * Copyright (c) 2019 Cisco and/or its affiliates.
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
 */

/**
 * @file
 * @brief Path Tracing (PT)
 *
 * CLI to configure PT
 *
 */

#include <vlib/vlib.h>
#include <vnet/vnet.h>
#include <vnet/srv6/sr.h>
#include <vnet/ip/ip.h>
#include <vnet/srv6/sr_packet.h>
#include <vnet/ip/ip6_packet.h>
#include <vnet/fib/ip6_fib.h>
#include <vnet/dpo/dpo.h>
#include <vnet/adj/adj.h>
#include <vnet/pt/pt.h>

#include <vppinfra/error.h>
#include <vppinfra/elog.h>

pt_main_t pt_main;

int
pt_iface_add (u32 iface, u16 id, u8 ingress_load, u8 egress_load, u8 tts_template)
{
  pt_main_t *pt = &pt_main;
  uword *p;

  pt_iface_t *ls = 0;

  if (iface == (u32)~0)
    return PT_ERR_IFACE_INVALID;

  /* Search for the item */
  p = mhash_get (&pt->pt_iface_index_hash, &iface);

  if (p)
    return -PT_ERR_EXIST;

  if (id ==0 || id > PT_ID_MAX)
    return -PT_ERR_ID_INVALID;

  if (ingress_load > PT_LOAD_MAX || egress_load > PT_LOAD_MAX)
    return -PT_ERR_LOAD_INVALID;

  if (tts_template > PT_TTS_TEMPLATE_MAX)
    return -PT_ERR_TTS_TEMPLATE_INVALID; 

  vnet_feature_enable_disable ("ip6-output", "pt", iface, 1, 0, 0);

  /* Create a new pt_iface */
  pool_get (pt->pt_iface, ls);
  clib_memset (ls, 0, sizeof (*ls));
  ls->iface = iface;
  ls->id = id;
  ls->ingress_load = ingress_load;
  ls->egress_load = egress_load;
  ls->tts_template = tts_template;

  /* Set hash key for searching pt_iface by iface */
  mhash_set (&pt->pt_iface_index_hash, &iface, ls - pt->pt_iface,
	     NULL);
  return 0;
}

int
pt_iface_del (u32 iface)
{
  pt_main_t *pt = &pt_main;
  uword *p;

  pt_iface_t *ls = 0;

  if (iface == (u32)~0)
    return PT_ERR_IFACE_INVALID;

  /* Search for the item */
  p = mhash_get (&pt->pt_iface_index_hash, &iface);

  if (p)
    {
      /* Retrieve pt_iface */
      ls = pool_elt_at_index (pt->pt_iface, p[0]);
      vnet_feature_enable_disable ("ip6-output", "pt", iface, 0, 0, 0);
      /* Delete pt_iface */
      pool_put (pt->pt_iface, ls);
      mhash_unset (&pt->pt_iface_index_hash, &iface, NULL);
    }
  else
    {
        return -PT_ERR_NOENT;
    }
  return 0;
}

int
pt_probe_inject_iface_add (u32 iface)
{
  pt_main_t *pt = &pt_main;
  uword *p;
  pt_probe_inject_iface_t *ls = 0;

  if (iface == (u32)~0)
    return PT_ERR_IFACE_INVALID;

  /* Search for the item */
  p = mhash_get (&pt->pt_probe_inject_iface_index_hash, &iface);

  if (p)
    return -PT_ERR_EXIST; 

  /* Create a new pt probe-inject iface */
  pool_get (pt->pt_probe_inject_iface, ls);
  clib_memset (ls, 0, sizeof (*ls));
  ls->iface = iface;

  /* Set hash key for searching pt probe-inject iface by iface */
  mhash_set (&pt->pt_probe_inject_iface_index_hash, &iface,
	     ls - pt->pt_probe_inject_iface, NULL);
  return 0;
}

int
pt_probe_inject_iface_del (u32 iface)
{
  pt_main_t *pt = &pt_main;
  uword *p;

  pt_probe_inject_iface_t *ls = 0;

  if (iface == (u32)~0)
    return PT_ERR_IFACE_INVALID;

  /* Search for the item */
  p = mhash_get (&pt->pt_probe_inject_iface_index_hash, &iface);

  if (p)
    {
      /* Retrieve pt probe-inject iface */
      ls = pool_elt_at_index (pt->pt_probe_inject_iface, p[0]);
      /* Delete pt probe-inject iface */
      pool_put (pt->pt_probe_inject_iface, ls);
      mhash_unset (&pt->pt_probe_inject_iface_index_hash, &iface, NULL);
    }
  else
    {
      return -PT_ERR_NOENT; 	
    }
  return 0;
}

/**
 * @brief "pt iface add" CLI function.
 *
 * @see pt_cli_iface_add
 */
static clib_error_t *
pt_cli_iface_add_command_fn (vlib_main_t * vm, unformat_input_t * input,
			    vlib_cli_command_t * cmd)
{
  vnet_main_t *vnm = vnet_get_main ();
  u32 iface = (u32) ~ 0;
  u32 id = (u32) ~ 0;
  u32 ingress_load = 0; 
  u32 egress_load = 0; 
  u32 tts_template = PT_TTS_TEMPLATE_DEFAULT; 

  int rv;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "iface %U",
			 unformat_vnet_sw_interface, vnm, &iface));
      else if (unformat (input, "id %u", &id));
      else if (unformat (input, "ingress-load %u", &ingress_load))
	;
      else if (unformat (input, "egress-load %u", &egress_load))
	;
      else if (unformat (input, "tts-template %u", &tts_template))
	;
      else
	break;
    }

  rv = pt_iface_add (iface, id, ingress_load, egress_load, tts_template);

  switch (rv)
    {
    case 0:
      break;
    case -PT_ERR_EXIST:
      return clib_error_return (0,"Error: Identicar iface already exists.");
    case -PT_ERR_IFACE_INVALID:
      return clib_error_return (0,"Error: The iface name is not valid.");
    case -PT_ERR_ID_INVALID:
      return clib_error_return (0,"Error: The iface id value is not valid."); 
    case -PT_ERR_LOAD_INVALID:
      return clib_error_return (0,"Error: The iface load value is not valid."); 
    case -PT_ERR_TTS_TEMPLATE_INVALID:
      return clib_error_return (0,"Error: The iface tts_template value is not valid."); 
    default:
      return clib_error_return (0, "Error: unknown error.");
    }
  return 0;
}

/**
 * @brief "pt del iface" CLI function.
 *
 * @see pt_cli_del_iface
 */
static clib_error_t *
pt_cli_iface_del_command_fn (vlib_main_t * vm, unformat_input_t * input,
			    vlib_cli_command_t * cmd)
{
  vnet_main_t *vnm = vnet_get_main ();
  u32 iface = (u32) ~ 0;

  int rv;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "iface %U",
			 unformat_vnet_sw_interface, vnm, &iface))
	;
      else
	break;
    }

  rv = pt_iface_del (iface);

  switch (rv)
    {
    case 0:
      break;
    case -PT_ERR_NOENT:
      return clib_error_return (0,"Error: No such iface.");
    case -PT_ERR_IFACE_INVALID:
      return clib_error_return (0,"Error: The iface name is not valid.");
    default:
      return clib_error_return (0, "Error: unknown error.");
    }
  return 0;
}

/**
 * @brief "pt probe-inject-iface add" CLI function.
 *
 */

static clib_error_t *
pt_cli_probe_inject_iface_add_command_fn (vlib_main_t * vm, unformat_input_t * input,
			     vlib_cli_command_t * cmd)
{

  vnet_main_t *vnm = vnet_get_main ();
  u32 iface = (u32) ~ 0;
  int rv;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "iface %U",
			 unformat_vnet_sw_interface, vnm, &iface))
	;
      else
	break;
    }

  rv = pt_probe_inject_iface_add (iface);

  switch (rv)
    {
    case 0:
      break;
    case -PT_ERR_EXIST:
      return clib_error_return (0, "Error: Identical iface already exists.");
    case -PT_ERR_IFACE_INVALID:
      return clib_error_return (0,"Error: The iface name is not valid.");
    default:
      return clib_error_return (0, "Error: unknown error.");
    }

  return 0;
}


/**
 * @brief "pt probe-inject-iface del" CLI function.
 *
 */

static clib_error_t *
pt_cli_probe_inject_iface_del_command_fn (vlib_main_t * vm, unformat_input_t * input,
			     vlib_cli_command_t * cmd)
{

  vnet_main_t *vnm = vnet_get_main ();
  u32 iface = (u32) ~ 0;
  int rv;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "iface %U",
			 unformat_vnet_sw_interface, vnm, &iface))
	;
      else
	break;
    }

  rv = pt_probe_inject_iface_del (iface);
  switch (rv)
    {
    case 0:
      break;
    case -PT_ERR_NOENT:
      return clib_error_return (0,"Error: No such iface.");
    case -PT_ERR_IFACE_INVALID:
      return clib_error_return (0,"Error: The iface name is not valid.");
    default:
      return clib_error_return (0, "Error: unknown error.");
    }
  return 0;
}


/**
 * @brief "pt probe-inject-iface show" CLI function 
 */
static clib_error_t *
pt_probe_inject_iface_show_command_fn (vlib_main_t * vm, unformat_input_t * input,
			      vlib_cli_command_t * cmd)
{
  vnet_main_t *vnm = vnet_get_main ();
  pt_main_t *pt = &pt_main;
  pt_probe_inject_iface_t **pt_probe_inject_iface_list = 0;
  pt_probe_inject_iface_t *ls;
  int i;

  vlib_cli_output (vm, "PT probe-inject interfaces:");
  vlib_cli_output (vm, "==================================");

  /* *INDENT-OFF* */
  pool_foreach (ls, pt->pt_probe_inject_iface) { vec_add1 (pt_probe_inject_iface_list, ls); };
  /* *INDENT-ON* */

  for (i = 0; i < vec_len (pt_probe_inject_iface_list); i++)
    {
      ls = pt_probe_inject_iface_list[i];
      vlib_cli_output (vm, "\tinterface: \t%U",
		       format_vnet_sw_if_index_name, vnm, ls->iface);
      vlib_cli_output (vm, "--------------------------------");
    }

  return 0;
}

/**
 * @brief CLI function to 'show' all PT interfcaes 
 */
static clib_error_t *
pt_iface_show_command_fn (vlib_main_t * vm, unformat_input_t * input,
			     vlib_cli_command_t * cmd)
{
  vnet_main_t *vnm = vnet_get_main ();
  pt_main_t *pt = &pt_main;
  pt_iface_t **pt_iface_list = 0;
  pt_iface_t *ls;
  int i;

  vlib_cli_output (vm, "PT interfaces");
  vlib_cli_output (vm, "==================================");

  /* *INDENT-OFF* */
  pool_foreach (ls, pt->pt_iface) { vec_add1 (pt_iface_list, ls); };
  /* *INDENT-ON* */

  for (i = 0; i < vec_len (pt_iface_list); i++)
    {
      ls = pt_iface_list[i];
      vlib_cli_output (
	vm,
	"\tiface       : \t%U\n\tid          : \t%d\n\tingress-load: "
	"\t%d\n\tegress-load : \t%d\n\ttts-template: \t%d  ",
	format_vnet_sw_if_index_name, vnm, ls->iface, ls->id, ls->ingress_load,
	ls->egress_load, ls->tts_template);
      vlib_cli_output (vm, "--------------------------------");
    }

  return 0;
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (pt_iface_add_command, static) = {
  .path = "pt iface add",
  .short_help = "pt iface add iface <iface-name> id <pt-iface-id> ingress-load <ingress-load-value> egress-load <egress-load-value> tts-template <truncated-timestamp-template>",
  .function = pt_cli_iface_add_command_fn,
};
/* *INDENT-ON* */

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (pt_iface_del_command, static) = {
  .path = "pt iface del",
  .short_help = "pt iface del iface <iface-name>",
  .function = pt_cli_iface_del_command_fn,
};
/* *INDENT-ON* */

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (pt_probe_inject_iface_add_command, static) = {
  .path = "pt probe-inject-iface add",
  .short_help = "pt probe-inject-iface add iface <iface>",
  .function = pt_cli_probe_inject_iface_add_command_fn,
};
/* *INDENT-ON* */

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (pt_probe_inject_iface_del_command, static) = {
  .path = "pt probe-inject-iface del",
  .short_help = "pt probe-inject-iface del iface <iface>",
  .function = pt_cli_probe_inject_iface_del_command_fn,
};
/* *INDENT-ON* */

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (pt_probe_inject_iface_show_command, static) = {
  .path = "pt probe-inject-iface show",
  .short_help = "pt probe-inject-iface show",
  .function = pt_probe_inject_iface_show_command_fn,
};
/* *INDENT-ON* */

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (pt_iface_show_command, static) = {
  .path = "pt iface show",
  .short_help = "pt iface show",
  .function = pt_iface_show_command_fn,
};
/* *INDENT-ON* */

/**
 *  * @brief PT initialization
 *   */
clib_error_t *
pt_init (vlib_main_t * vm)
{
  pt_main_t *pt = &pt_main;
  mhash_init (&pt->pt_iface_index_hash, sizeof (uword), sizeof (u32));
  mhash_init (&pt->pt_probe_inject_iface_index_hash, sizeof (uword), sizeof (u32));
  return 0;
}

VLIB_INIT_FUNCTION (pt_init);
/*
 * * fd.io coding-style-patch-verification: ON
 * *
 * * Local Variables:
 * * eval: (c-set-style "gnu")
 * * End:
 * */
