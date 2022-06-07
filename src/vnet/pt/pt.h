/*
 * Copyright (c) 2015 Cisco and/or its affiliates.
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
 * @brief Path Tracing data structures definitions
 *
 */

#ifndef included_vnet_pt_h
#define included_vnet_pt_h

#define IP6_HBH_PT_TYPE                 50  

/*PT error codes*/
#define PT_ERR_NOENT                     1   /* No such entry*/
#define PT_ERR_EXIST                     2   /* Entry exists */
#define PT_ERR_IFACE_INVALID             3   /* IFACE not valid */
#define PT_ERR_ID_INVALID                4   /* ID not valid */
#define PT_ERR_LOAD_INVALID              5   /* LOAD not valid*/
#define PT_ERR_TTS_TEMPLATE_INVALID      6   /* TTS Template not valid */

/*PT paramters max values*/
#define PT_ID_MAX                     4095
#define PT_LOAD_MAX                     15
#define PT_TTS_TEMPLATE_MAX              3

/*PT TTS Templates*/
#define PT_TTS_TEMPLATE_0                0
#define PT_TTS_TEMPLATE_1                1
#define PT_TTS_TEMPLATE_2                2
#define PT_TTS_TEMPLATE_3                3
#define PT_TTS_TEMPLATE_DEFAULT          2

/*PT TTS Template shift value*/
#define PT_TTS_SHIFT_TEMPLATE_0         12 
#define PT_TTS_SHIFT_TEMPLATE_1         14
#define PT_TTS_SHIFT_TEMPLATE_2         18
#define PT_TTS_SHIFT_TEMPLATE_3         19

#define PT_LOAD_MASK                   0x000F

/*PT node behaviors*/
#define PT_BEHAVIOR_SRC 0
#define PT_BEHAVIOR_MID 1
#define PT_BEHAVIOR_SNK 2

typedef struct
{
  u32 iface;			/**< Interface */
  u16 id;			/**< PT ID of the Interface */
  u8  ingress_load;             /**< Ingress Load of the interface */
  u8  egress_load;              /**< Egress Load of the interface */
  u8  tts_template;             /**< TTS Template of the interface*/ 
} __attribute__ ((packed)) pt_iface_t;


typedef struct
{
  u32 iface;		/**< Interface */
} __attribute__ ((packed)) pt_probe_inject_iface_t;


typedef struct
{
  u32 sec;
  u32 nsec;
} __attribute__ ((packed)) pt_t64_t;

typedef struct
{
  u16 oif_oil;
  u8 tts;
} __attribute__ ((packed)) pt_cmd_t;


typedef struct
{
  pt_cmd_t cmd_stack[12];
} __attribute__ ((packed)) ip6_hop_by_hop_option_pt_t;

 /**
  * @brief Path Tracing main datastructure
  */
typedef struct
{

  /* Pool of pt_iface instances */
  pt_iface_t *pt_iface;

  /* Hash table for pt iface parameters */
  mhash_t pt_iface_index_hash;

  /* Pool of pt_probe_inject_iface instances */
  pt_probe_inject_iface_t *pt_probe_inject_iface;

  /* Hash table containing list pt probe-inject interfaces */
  mhash_t pt_probe_inject_iface_index_hash;

  /* convenience */
  vlib_main_t *vlib_main;
  vnet_main_t *vnet_main;

} pt_main_t;

extern pt_main_t pt_main;
extern vlib_node_registration_t pt_node;
extern int pt_iface_add (u32 iface, u16 id, u8 ingress_load, u8 egress_load, u8 tts_template);
extern int pt_iface_del (u32 iface);
extern int pt_probe_inject_iface_add (u32 iface);
extern int pt_probe_inject_iface_del (u32 iface);
extern void *pt_iface_find (u32 iface);
extern void *pt_probe_inject_iface_find (u32 iface);

#endif /* included_vnet_pt_h */

/*
 * * fd.io coding-style-patch-verification: ON
 * *
 * * Local Variables:
 * * eval: (c-set-style "gnu")
 * * End:
 * */
