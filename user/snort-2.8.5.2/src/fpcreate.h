/*
**  $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/snort-2.8.5.2/src/fpcreate.h#1 $
**
**  fpcreate.h
**
** Copyright (C) 2002-2009 Sourcefire, Inc.
** Dan Roelker <droelker@sourcefire.com>
** Marc Norton <mnorton@sourcefire.com>
**
** NOTES
** 5.7.02 - Initial Sourcecode.  Norton/Roelker
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** 6/13/05 - marc norton
**   Added plugin support for fast pattern match data, requires DYNAMIC_PLUGIN be defined
**
*/
#ifndef __FPCREATE_H__
#define __FPCREATE_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rules.h"
//#include "parser.h"
#include "pcrm.h"

struct _SnortConfig;

/*
 *  Max Number of Protocols Supported by Rules in fpcreate.c
 *  for tcp,udp,icmp,ip ... this is an array dimesnion used to
 *  map protocol-ordinals to port_groups ...
 */
/* This is now defined in sftarget_protocol_refererence.h"
 * #define MAX_PROTOCOL_ORDINAL 8192 */
#include "sftarget_protocol_reference.h"

/*
 *  This controls how many fast pattern match contents may be 
 *  used/retrieved per rule in fpcreate.c.
 */
#define PLUGIN_MAX_FPLIST_SIZE 16

#define PL_BLEEDOVER_WARNINGS_ENABLED        0x01
#define PL_DEBUG_PRINT_NC_DETECT_RULES       0x02
#define PL_DEBUG_PRINT_RULEGROWP_BUILD       0x04
#define PL_DEBUG_PRINT_RULEGROUPS_UNCOMPILED 0x08
#define PL_DEBUG_PRINT_RULEGROUPS_COMPILED   0x10
#define PL_SINGLE_RULE_GROUP                 0x20

/*
**  This structure holds the RTN and OTN
**  for a specific rule.  This way we can
**  verify each rule and stay within the
**  current snort rule architecture.
*/
typedef struct _otnx_
{

   OptTreeNode   * otn;
   //RuleTreeNode  * rtn; 
   unsigned int    content_length;

} OTNX;

typedef struct _pmx_
{

   void * RuleNode;
   void * PatternMatchData;

} PMX;

/* Used for negative content list */
typedef struct _NCListNode
{
    PMX *pmx;
    struct _NCListNode *next;

} NCListNode;

/*
**  This structure holds configuration options for the 
**  detection engine.
*/
typedef struct _FastPatternConfig
{
    int inspect_stream_insert;
    int search_method;
    int search_opt;
    int search_method_verbose;
    int debug;
    unsigned int max_queue_events;
//PORTLISTS
    unsigned int bleedover_port_limit;
    int portlists_flags;

} FastPatternConfig;

#ifdef TARGET_BASED
/*
 *  Service Rule Map Master Table
 */
typedef struct 
{
  SFGHASH * tcp_to_srv;
  SFGHASH * tcp_to_cli;
  
  SFGHASH * udp_to_srv;
  SFGHASH * udp_to_cli;

  SFGHASH * icmp_to_srv;
  SFGHASH * icmp_to_cli;

  SFGHASH * ip_to_srv;
  SFGHASH * ip_to_cli;

} srmm_table_t;

/*
 *  Service/Protocol Oridinal To PORT_GROUP table
 */
typedef struct 
{
  PORT_GROUP *tcp_to_srv[MAX_PROTOCOL_ORDINAL];
  PORT_GROUP *tcp_to_cli[MAX_PROTOCOL_ORDINAL];
  
  PORT_GROUP *udp_to_srv[MAX_PROTOCOL_ORDINAL];
  PORT_GROUP *udp_to_cli[MAX_PROTOCOL_ORDINAL];

  PORT_GROUP *icmp_to_srv[MAX_PROTOCOL_ORDINAL];
  PORT_GROUP *icmp_to_cli[MAX_PROTOCOL_ORDINAL];

  PORT_GROUP *ip_to_srv[MAX_PROTOCOL_ORDINAL];
  PORT_GROUP *ip_to_cli[MAX_PROTOCOL_ORDINAL];

} sopg_table_t; 
#endif

/*
**  This function initializes the detection engine configuration
**  options before setting them.
*/
int fpInitDetectionEngine(void);

/*
**  This is the main routine to create a FastPacket inspection
**  engine.  It reads in the snort list of RTNs and OTNs and
**  assigns them to PORT_MAPS.
*/
#ifdef PORTLISTS
int fpCreateFastPacketDetection(struct _SnortConfig *);
#else
int fpCreateFastPacketDetection(RuleListNode *);
#endif

FastPatternConfig * FastPatternConfigNew(void);
void FastPatternConfigFree(FastPatternConfig *);

/*
**  Functions that allow the detection routins to 
**  find the right classification for a given packet.
*/
int prmFindRuleGroupTcp(PORT_RULE_MAP *, int, int, PORT_GROUP **, PORT_GROUP **, PORT_GROUP **);
int prmFindRuleGroupUdp(PORT_RULE_MAP *, int, int, PORT_GROUP **, PORT_GROUP **, PORT_GROUP **);
int prmFindRuleGroupIp(PORT_RULE_MAP *, int, PORT_GROUP **, PORT_GROUP **);
int prmFindRuleGroupIcmp(PORT_RULE_MAP *, int, PORT_GROUP **, PORT_GROUP **);

int fpSetDetectSearchMethod(FastPatternConfig *, char *);
void fpSetDetectSearchOpt(FastPatternConfig *, int flag);
void fpSetDebugMode(FastPatternConfig *);
void fpSetStreamInsert(FastPatternConfig *);
void fpSetMaxQueueEvents(FastPatternConfig *, unsigned int);

void fpDetectSetSingleRuleGroup(FastPatternConfig *);
void fpDetectSetBleedOverPortLimit(FastPatternConfig *, unsigned int);
void fpDetectSetBleedOverWarnings(FastPatternConfig *);
void fpDetectSetDebugPrintNcRules(FastPatternConfig *);
void fpDetectSetDebugPrintRuleGroupBuildDetails(FastPatternConfig *);
void fpDetectSetDebugPrintRuleGroupsCompiled(FastPatternConfig *);
void fpDetectSetDebugPrintRuleGroupsUnCompiled(FastPatternConfig *);

int  fpDetectGetSingleRuleGroup(FastPatternConfig *);
int  fpDetectGetBleedOverPortLimit(FastPatternConfig *);
int  fpDetectGetBleedOverWarnings(FastPatternConfig *);
int  fpDetectGetDebugPrintNcRules(FastPatternConfig *);
int  fpDetectGetDebugPrintRuleGroupBuildDetails(FastPatternConfig *);
int  fpDetectGetDebugPrintRuleGroupsCompiled(FastPatternConfig *);
int  fpDetectGetDebugPrintRuleGroupsUnCompiled(FastPatternConfig *);

void fpDeleteFastPacketDetection(struct _SnortConfig *);
void free_detection_option_tree(detection_option_tree_node_t *node);

#ifdef PORTLISTS
int OtnHasContent( OptTreeNode * p );
int OtnHasUriContent( OptTreeNode * p );
int OtnFlowDir( OptTreeNode * p );
# ifdef TARGET_BASED
PORT_GROUP * fpGetServicePortGroupByOrdinal(sopg_table_t *, int, int, int16_t);
# endif
#endif

/*
**  Shows the event stats for the created FastPacketDetection
*/
void fpShowEventStats(struct _SnortConfig *);
typedef int (*OtnWalkFcn)(int, RuleTreeNode *, OptTreeNode *);
void fpWalkOtns(int, OtnWalkFcn);

#ifdef DYNAMIC_PLUGIN
void fpDynamicDataFree(void *);
#endif

#endif
