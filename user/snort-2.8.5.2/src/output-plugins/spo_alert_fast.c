/*
** Copyright (C) 2002-2009 Sourcefire, Inc.
** Copyright (C) 1998-2002 Martin Roesch <roesch@sourcefire.com>
** Copyright (C) 2000,2001 Andrew R. Baker <andrewb@uab.edu>
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
*/

/* $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/snort-2.8.5.2/src/output-plugins/spo_alert_fast.c#1 $ */

/* spo_alert_fast
 * 
 * Purpose:  output plugin for fast alerting
 *
 * Arguments:  alert file
 *   
 * Effect:
 *
 * Alerts are written to a file in the snort fast alert format
 *
 * Comments:   Allows use of fast alerts with other output plugin types
 *
 */

/* output plugin header file */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "event.h"
#include "decode.h"
#include "debug.h"
#include "plugbase.h"
#include "spo_plugbase.h"
#include "parser.h"
#include "util.h"
#include "log.h"
#include "mstring.h"
#include "snort.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif /* !WIN32 */

#include <sys/types.h>

#include "sfutil/sf_textlog.h"
#include "log_text.h"
#include "sf_textlog.h"

/* full buf was chosen to allow printing max size packets
 * in hex/ascii mode:
 * each byte => 2 nibbles + space + ascii + overhead
 */
#define FULL_BUF  (4*IP_MAXPACKET)
#define FAST_BUF  (4*K_BYTES)

/*
 * not defined for backwards compatibility
 * (default is produced by OpenAlertFile()
#define DEFAULT_FILE  "alert.fast"
 */
#define DEFAULT_LIMIT (128*M_BYTES)

extern char *pcap_interface;
extern SnortConfig *snort_conf_for_parsing;

typedef struct _SpoAlertFastData
{
    TextLog* log;
    uint8_t packet_flag;
} SpoAlertFastData;

static void AlertFastInit(char *);
static SpoAlertFastData *ParseAlertFastArgs(char *);
static void AlertFastCleanExitFunc(int, void *);
static void AlertFastRestartFunc(int, void *);
static void AlertFast(Packet *, char *, void *, Event *);

/*
 * Function: SetupAlertFast()
 *
 * Purpose: Registers the output plugin keyword and initialization 
 *          function into the output plugin list.  This is the function that
 *          gets called from InitOutputPlugins() in plugbase.c.
 *
 * Arguments: None.
 *
 * Returns: void function
 *
 */
void AlertFastSetup(void)
{
    /* link the preprocessor keyword to the init function in 
       the preproc list */
    RegisterOutputPlugin("alert_fast", OUTPUT_TYPE_FLAG__ALERT, AlertFastInit);
    DEBUG_WRAP(DebugMessage(DEBUG_INIT,"Output plugin: AlertFast is setup...\n"););
}


/*
 * Function: AlertFastInit(char *)
 *
 * Purpose: Calls the argument parsing function, performs final setup on data
 *          structs, links the preproc function into the function list.
 *
 * Arguments: args => ptr to argument string
 *
 * Returns: void function
 *
 */
static void AlertFastInit(char *args)
{
    SpoAlertFastData *data;

    DEBUG_WRAP(DebugMessage(DEBUG_INIT,"Output: AlertFast Initialized\n"););

    /* parse the argument list from the rules file */
    data = ParseAlertFastArgs(args);

    DEBUG_WRAP(DebugMessage(DEBUG_INIT,"Linking AlertFast functions to call lists...\n"););
    
    /* Set the preprocessor function into the function list */
    AddFuncToOutputList(AlertFast, OUTPUT_TYPE__ALERT, data);
    AddFuncToCleanExitList(AlertFastCleanExitFunc, data);
    AddFuncToRestartList(AlertFastRestartFunc, data);
}

static void AlertFast(Packet *p, char *msg, void *arg, Event *event)
{
    SpoAlertFastData *data = (SpoAlertFastData *)arg;

    LogTimeStamp(data->log, p);

    if( p != NULL && p->packet_flags & PKT_INLINE_DROP )
        TextLog_Puts(data->log, " [Drop]");

    if(msg != NULL)
    {
#ifdef MARK_TAGGED
        char c=' ';
        if ((p != NULL) && (p->packet_flags & PKT_REBUILT_STREAM))
            c = 'R';
        else if ((p != NULL) && (p->packet_flags & PKT_REBUILT_FRAG))
            c = 'F';
        TextLog_Print(data->log, " [**] %c ", c);
#else
        TextLog_Puts(data->log, " [**] ");
#endif

        if(event != NULL)
        {
            TextLog_Print(data->log, "[%lu:%lu:%lu] ",
                    (unsigned long) event->sig_generator,
                    (unsigned long) event->sig_id,
                    (unsigned long) event->sig_rev);
        }

        if (ScAlertInterface())
        {
            TextLog_Print(data->log, " <%s> ", PRINT_INTERFACE(pcap_interface));
            TextLog_Puts(data->log, msg);
        }
        else
        {
            TextLog_Puts(data->log, msg);
        }

        TextLog_Puts(data->log, " [**] ");
    }

    /* print the packet header to the alert file */
    if(p && IPH_IS_VALID(p))
    {
        LogPriorityData(data->log, 0);

        TextLog_Print(data->log, "{%s} ", protocol_names[GET_IPH_PROTO(p)]);

        if(p->frag_flag)
        {
            /* just print the straight IP header */
            TextLog_Puts(data->log, inet_ntoa(GET_SRC_ADDR(p)));
            TextLog_Puts(data->log, " -> ");
            TextLog_Puts(data->log, inet_ntoa(GET_DST_ADDR(p)));
        }
        else
        {
            switch(GET_IPH_PROTO(p))
            {
                case IPPROTO_UDP:
                case IPPROTO_TCP:
                    /* print the header complete with port information */
                    TextLog_Puts(data->log, inet_ntoa(GET_SRC_ADDR(p)));
                    TextLog_Print(data->log, ":%d -> ", p->sp);
                    TextLog_Puts(data->log, inet_ntoa(GET_DST_ADDR(p)));
                    TextLog_Print(data->log, ":%d", p->dp);
                    break;
                case IPPROTO_ICMP:
                default:
                    /* just print the straight IP header */
                    TextLog_Puts(data->log, inet_ntoa(GET_SRC_ADDR(p)));
                    TextLog_Puts(data->log, " -> ");
                    TextLog_Puts(data->log, inet_ntoa(GET_DST_ADDR(p)));
            }
        }
    }               /* end of if (p) */

    if(p && data->packet_flag)
    {
        /* Log whether or not this is reassembled data - only indicate
         * if we're actually going to show any of the payload */
        if (ScOutputAppData() && (p->dsize > 0))
        {
            if (p->packet_flags &
                (PKT_DCE_RPKT | PKT_REBUILT_STREAM | PKT_REBUILT_FRAG |
                 PKT_SMB_SEG | PKT_DCE_SEG | PKT_DCE_FRAG | PKT_SMB_TRANS))
            {
                TextLog_NewLine(data->log);
            }

            if (p->packet_flags & PKT_SMB_SEG)
                TextLog_Print(data->log, "%s", "SMB desegmented packet");
            else if (p->packet_flags & PKT_DCE_SEG)
                TextLog_Print(data->log, "%s", "DCE/RPC desegmented packet");
            else if (p->packet_flags & PKT_DCE_FRAG)
                TextLog_Print(data->log, "%s", "DCE/RPC defragmented packet");
            else if (p->packet_flags & PKT_SMB_TRANS)
                TextLog_Print(data->log, "%s", "SMB Transact reassembled packet");
            else if (p->packet_flags & PKT_DCE_RPKT)
                TextLog_Print(data->log, "%s", "DCE/RPC reassembled packet");
            else if (p->packet_flags & PKT_REBUILT_STREAM)
                TextLog_Print(data->log, "%s", "Stream reassembled packet");
            else if (p->packet_flags & PKT_REBUILT_FRAG)
                TextLog_Print(data->log, "%s", "Frag reassembled packet");
        }

        TextLog_NewLine(data->log);

        if(IPH_IS_VALID(p))
            LogIPPkt(data->log, GET_IPH_PROTO(p), p);
#ifndef NO_NON_ETHER_DECODER
        else if(p->ah)
            LogArpHeader(data->log, p);
#endif
    }
    TextLog_NewLine(data->log);
    TextLog_Flush(data->log);
}

/*
 * Function: ParseAlertFastArgs(char *)
 *
 * Purpose: Process positional args, if any.  Syntax is:
 * output alert_fast: [<logpath> ["packet"] [<limit>]]
 * limit ::= <number>('G'|'M'|K')
 *
 * Arguments: args => argument list
 *
 * Returns: void function
 *
 */
static SpoAlertFastData *ParseAlertFastArgs(char *args)
{
    char **toks;
    int num_toks;
    SpoAlertFastData *data;
    char* filename = NULL;
    unsigned long limit = DEFAULT_LIMIT;
    unsigned int bufSize = FAST_BUF;
    int i;

    DEBUG_WRAP(DebugMessage(DEBUG_LOG, "ParseAlertFastArgs: %s\n", args););
    data = (SpoAlertFastData *)SnortAlloc(sizeof(SpoAlertFastData));

    if ( !data )
    {
        FatalError("alert_fast: unable to allocate memory!\n");
    }
    if ( !args ) args = "";
    toks = mSplit((char *)args, " \t", 0, &num_toks, '\\');

    for (i = 0; i < num_toks; i++)
    {
        const char* tok = toks[i];
        char *end;

        switch (i)
        {
            case 0:
                if ( !strcasecmp(tok, "stdout") )
                    filename = SnortStrdup(tok);

                else
                    filename = ProcessFileOption(snort_conf_for_parsing, tok);
                break;

            case 1:
                if ( !strcasecmp("packet", tok) )
                {
                    data->packet_flag = 1;
                    bufSize = FULL_BUF;
                    break;
                }
                /* in this case, only 2 options allowed */
                else i++;
                /* fall thru so "packet" is optional ... */

            case 2:
                limit = strtol(tok, &end, 10);

                if ( tok == end )
                    FatalError("alert_fast error in %s(%i): %s\n",
                        file_name, file_line, tok);

                if ( end && toupper(*end) == 'G' )
                    limit <<= 30; /* GB */

                else if ( end && toupper(*end) == 'M' )
                    limit <<= 20; /* MB */

                else if ( end && toupper(*end) == 'K' )
                    limit <<= 10; /* KB */
                break;

            case 3:
                FatalError("alert_fast: error in %s(%i): %s\n",
                    file_name, file_line, tok);
                break;
        }
    }
    mSplitFree(&toks, num_toks);

#ifdef DEFAULT_FILE
    if ( !filename ) filename = ProcessFileOption(snort_conf_for_parsing, DEFAULT_FILE);
#endif

    DEBUG_WRAP(DebugMessage(
        DEBUG_INIT, "alert_fast: '%s' %d %ld\n",
        filename?filename:"alert", data->packet_flag, limit
    ););
    data->log = TextLog_Init(filename, bufSize, limit);
    if ( filename ) free(filename);

    return data;
}

static void AlertFastCleanup(int signal, void *arg, const char* msg)
{
    SpoAlertFastData *data = (SpoAlertFastData *)arg;
    DEBUG_WRAP(DebugMessage(DEBUG_LOG, "%s\n", msg););

    /*free memory from SpoAlertFastData */
    if ( data->log ) TextLog_Term(data->log);
    free(data);
}

static void AlertFastCleanExitFunc(int signal, void *arg)
{
    AlertFastCleanup(signal, arg, "AlertFastCleanExitFunc");
}

static void AlertFastRestartFunc(int signal, void *arg)
{
    AlertFastCleanup(signal, arg, "AlertFastRestartFunc");
}

