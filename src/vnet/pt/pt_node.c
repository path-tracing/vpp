/*
 * Copyright (c) 2018 Cisco and/or its affiliates.
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

#include <vnet/fib/ip6_fib.h>
#include <vnet/dpo/load_balance.h>
#include <vnet/l2/feat_bitmap.h>
#include <vnet/fib/fib_table.h>
#include <vnet/srv6/sr.h>
#include <vnet/pt/pt.h>

void *
pt_probe_inject_iface_find (u32 iface)
{
  pt_main_t *pt = &pt_main;
  /* Search for the item */
  return mhash_get (&pt->pt_probe_inject_iface_index_hash, &iface);
}

void *
pt_iface_find (u32 iface)
{
//  u16 id = 0;
  pt_main_t *pt = &pt_main;
  uword *p;
  //pt_iface_t *ls = 0;
  /* Search for the item */
  p = mhash_get (&pt->pt_iface_index_hash, &iface);
  if (p)
    {
      /* Retrieve pt_iface */
      //ls = pool_elt_at_index (pt->pt_iface, p[0]);
      return pool_elt_at_index (pt->pt_iface, p[0]);
      //id = ls->id;
    }
  return NULL;
}

/**
 * @brief PT node trace
 */
typedef struct
{
  u32 iface;
  u16 id;
  u8 load;
  pt_t64_t t64;
  u8 tts_template;
  u8 tts;
  u8 behavior;
} pt_trace_t;

static u8 *
format_pt_trace (u8 * s, va_list * args)
{
  CLIB_UNUSED (vlib_main_t * vm) = va_arg (*args, vlib_main_t *);
  CLIB_UNUSED (vlib_node_t * node) = va_arg (*args, vlib_node_t *);
  pt_trace_t *t = va_arg (*args, pt_trace_t *);
  switch (t->behavior)
    {
    case PT_BEHAVIOR_SRC:
      s = format (
	s,
	"Behavior SRC, outgoing interface %U, outgoing interface id %u, "
	"outgoing interface load %u, t64_sec %u, t64_nsec %u",
	format_vnet_sw_if_index_name, vnet_get_main (), t->iface, t->id,
	t->load, htobe32 (t->t64.sec), htobe32 (t->t64.nsec));
      break;
    case PT_BEHAVIOR_MID:
      s = format (
	s,
	"Behavior Midpoint, outgoing interface %U, outgoing interface id %u, "
	"outgoing interface load %u, t64_sec %u, t64_nsec %u, tts_template %u, tts %u",
	format_vnet_sw_if_index_name, vnet_get_main (), t->iface, t->id,
	t->load, htobe32 (t->t64.sec), htobe32 (t->t64.nsec), t->tts_template, t->tts);
      break;
    default:
      break;
    }
  return s;
}

static_always_inline void
pt_src_processing (vlib_main_t *vm, vlib_node_runtime_t *node,
		   vlib_buffer_t *b0, ip6_header_t *ip0, pt_iface_t *ls, pt_t64_t t64)
{
  ip6_sr_header_t *sr0;
  ip6_sr_pt_tlv_t *srh_pt_tlv;
  sr0 = ip6_ext_header_find (vm, b0, ip0, IP_PROTOCOL_IPV6_ROUTE, NULL);
  if (sr0)
    {
      if (sr0->last_entry == 255 && sr0->length > 0)
	{
	  srh_pt_tlv =
	    (ip6_sr_pt_tlv_t *) ((u8 *) sr0 + sizeof (ip6_sr_header_t));
	  srh_pt_tlv->id_ld = htobe16 (ls->id << 4);
	  srh_pt_tlv->id_ld |= ls->egress_load;
	  srh_pt_tlv->t64.sec = htobe32 (t64.sec);
	  srh_pt_tlv->t64.nsec = htobe32 (t64.nsec);
	}
      else if ((sr0->length * 8) > ((sr0->last_entry + 1) * 16))
	{
	  srh_pt_tlv =
	    (ip6_sr_pt_tlv_t *) ((u8 *) sr0 + sizeof (ip6_sr_header_t) +
				     sizeof (ip6_address_t) *
				       (sr0->last_entry + 1));
	  srh_pt_tlv->id_ld = htobe16 (ls->id << 4);
	  srh_pt_tlv->id_ld |= ls->egress_load; 
	  srh_pt_tlv->t64.sec = htobe32 (t64.sec);
	  srh_pt_tlv->t64.nsec = htobe32 (t64.nsec);
	}
    }
  return;
}

static_always_inline void
pt_midpoint_processing (vlib_main_t *vm, vlib_node_runtime_t *node,
			vlib_buffer_t *b0, ip6_header_t *ip0, pt_iface_t *ls, pt_t64_t t64)
{
  ip6_hop_by_hop_header_t *hbh;
  ip6_hop_by_hop_option_t *hbh_opt;
  ip6_hop_by_hop_option_pt_t *hbh_opt_pt;

  if (ip0->protocol == IP_PROTOCOL_IP6_HOP_BY_HOP_OPTIONS)
    {
      hbh = (void *) (ip0 + 1);
      hbh_opt = (void *) (hbh + 1);
      if (hbh_opt->type == IP6_HBH_PT_TYPE)
	{
	  hbh_opt_pt = (void *) (hbh_opt + 1);
	  clib_memcpy_fast (&hbh_opt_pt->cmd_stack[1],
			    &hbh_opt_pt->cmd_stack[0], 33);
	  hbh_opt_pt->cmd_stack[0].oif_oil = htobe16 (ls->id << 4);
	  hbh_opt_pt->cmd_stack[0].oif_oil |= ls->egress_load;
	  switch (ls->tts_template)
	  {
          case PT_TTS_TEMPLATE_0: 
	    hbh_opt_pt->cmd_stack[0].tts = t64.nsec >> PT_TTS_SHIFT_TEMPLATE_0;
	    break; 
	  case PT_TTS_TEMPLATE_1: 
            hbh_opt_pt->cmd_stack[0].tts = t64.nsec >> PT_TTS_SHIFT_TEMPLATE_1;
	    break; 
	  case PT_TTS_TEMPLATE_2: 
	    hbh_opt_pt->cmd_stack[0].tts = t64.nsec >> PT_TTS_SHIFT_TEMPLATE_2;
	    break; 
	  case PT_TTS_TEMPLATE_3: 
            hbh_opt_pt->cmd_stack[0].tts = t64.nsec >> PT_TTS_SHIFT_TEMPLATE_0;
	    break;
	  default: 
	    break; 
	  }
	}
    }
  return;
}

VLIB_NODE_FN (pt_node) (vlib_main_t * vm,
			vlib_node_runtime_t * node, vlib_frame_t * from_frame)
{
  u32 n_left_from, next_index, *from, *to_next;
  from = vlib_frame_vector_args (from_frame);
  n_left_from = from_frame->n_vectors;
  next_index = node->cached_next_index;
  u8 pt_behavior = ~(u8) 0;
  pt_iface_t *ls = 0 ;
  while (n_left_from > 0)
    {
      u32 n_left_to_next;
      vlib_get_next_frame (vm, node, next_index, to_next, n_left_to_next);

      // Single loop for potentially the last three packets
      while (n_left_from > 0 && n_left_to_next > 0)
	{
	  u32 bi0;
	  u32 iface;
	  vlib_buffer_t *b0;
	  u32 next0 = 0;
	  ethernet_header_t *en0;
	  ip6_header_t *ip0 = 0;
	  bi0 = from[0];
	  to_next[0] = bi0;
	  from += 1;
	  to_next += 1;
	  n_left_from -= 1;
	  n_left_to_next -= 1;
	  pt_t64_t t64;

	  b0 = vlib_get_buffer (vm, bi0);
	  iface = vnet_buffer (b0)->sw_if_index[VLIB_TX];
	  ls = pt_iface_find (iface);
	  if (ls)
	    {
	      en0 = vlib_buffer_get_current (b0);
	      ip0 = (void *) (en0 + 1);

	      unix_time_now_nsec_fraction (&t64.sec, &t64.nsec);
	      if (pt_probe_inject_iface_find (
		    vnet_buffer (b0)->sw_if_index[VLIB_RX]))
		{
		  pt_src_processing (vm, node, b0, ip0, ls, t64);
		  pt_behavior = PT_BEHAVIOR_SRC;
		}
	      else
		{
		  pt_midpoint_processing (vm, node, b0, ip0, ls, t64);
		  pt_behavior = PT_BEHAVIOR_MID;
		}
	    }
	  if (PREDICT_FALSE (b0->flags & VLIB_BUFFER_IS_TRACED))
	    {
	      pt_trace_t *tr = vlib_add_trace (vm, node, b0, sizeof (*tr));
	      tr->iface = iface;
	      tr->id = ls->id;
	      tr->load = ls->egress_load;
	      tr->t64.sec = t64.sec;
	      tr->t64.nsec = t64.nsec;
	      tr->tts = t64.nsec >> 20;
	      tr->behavior = pt_behavior;
	    }
	  vlib_validate_buffer_enqueue_x1 (vm, node, next_index, to_next,
					   n_left_to_next, bi0, next0);
	}
      vlib_put_next_frame (vm, node, next_index, n_left_to_next);
    }

  return from_frame->n_vectors;
}

VLIB_REGISTER_NODE (pt_node) = {
  .name = "pt",
  .vector_size = sizeof (u32),
  .format_trace = format_pt_trace,
  .type = VLIB_NODE_TYPE_INTERNAL,
  .n_errors = 0,
  .n_next_nodes = 1,
  .next_nodes = { [0] = "interface-output" },
};

VNET_FEATURE_INIT (pt_node, static) = {
  .arc_name = "ip6-output",
  .node_name = "pt",
};

/*
 * * fd.io coding-style-patch-verification: ON
 * *
 * * Local Variables:
 * * eval: (c-set-style "gnu")
 * * End:
 * */
