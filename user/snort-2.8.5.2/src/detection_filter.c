/* $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/snort-2.8.5.2/src/detection_filter.c#1 $ */
/****************************************************************************
 *
 * Copyright (C) 2003-2009 Sourcefire, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2 as
 * published by the Free Software Foundation.  You may not use, modify or
 * distribute this program under any other version of the GNU General
 * Public License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 ****************************************************************************/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mstring.h"
#include "util.h"
#include "parser.h"

#include "sfthd.h"
#include "snort.h"

#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

static SFXHASH *detection_filter_hash = NULL;

DetectionFilterConfig * DetectionFilterConfigNew(void)
{
    DetectionFilterConfig *df =
        (DetectionFilterConfig *)SnortAlloc(sizeof(DetectionFilterConfig));

    df->memcap = 1024 * 1024;
    df->enabled = 1;

    return df;
}

void DetectionFilterConfigFree(DetectionFilterConfig *config)
{
    if (config == NULL)
        return;

    free(config);
}

void detection_filter_cleanup(void)
{

    if (detection_filter_hash == NULL)
        return;

    sfxhash_delete(detection_filter_hash);
    detection_filter_hash = NULL;
}

/*
 *  Startup Display
 */
void detection_filter_print_config(DetectionFilterConfig *df)
{
    //int i, gcnt=0;
    //THD_NODE * thd;

    if (df == NULL)
        return;

    LogMessage("\n");
    LogMessage("+-----------------------[detection-filter-config]"
               "------------------------------\n");
    LogMessage("| memory-cap : %d bytes\n", df->memcap);

    LogMessage("+-----------------------[detection-filter-rules]"
               "-------------------------------\n");

    if( !df->count)
    {
        LogMessage("| none\n");
    }
    else
    {
        //print_thd_local(s_thd, PRINT_LOCAL );
    }

    LogMessage("------------------------------------------------"
               "-------------------------------\n");
}

int detection_filter_test (
    void* pv, 
    snort_ip_p sip, snort_ip_p dip,
    long curtime )
{
    if (pv == NULL)
        return 0;

    return sfthd_test_rule(detection_filter_hash, (THD_NODE*)pv,
                           sip, dip, curtime);
}

/* empty out active entries */
void detection_filter_reset_active(void)
{
    if (detection_filter_hash == NULL)
        return;

    sfxhash_make_empty(detection_filter_hash);
}

void * detection_filter_create(DetectionFilterConfig *df_config, THDX_STRUCT *thdx)
{
    if (df_config == NULL)
        return NULL;

    if (!df_config->enabled)
        return NULL;

    /* Auto init - memcap must be set 1st, which is not really a problem */
    if (detection_filter_hash == NULL)
    {
        detection_filter_hash = sfthd_local_new(df_config->memcap);
        if (detection_filter_hash == NULL)
            return NULL;
    }

    df_config->count++;

    return sfthd_create_rule_threshold(df_config->count, thdx->tracking,
                                       thdx->type, thdx->count, thdx->seconds);
}

