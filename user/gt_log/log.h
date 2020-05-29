#ifndef _GT_TOOLS_H_
#define _GT_TOOLS_H_
/*
 * tools library for tmp nvram and log message
 *
 * Copyright 2007, Gemtek Technology Co., Ltd.
 * All Rights Reserved.                
 *                                     
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Gemtek Corporation;   
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior      
 * written permission of Gemtek Corporation.                            
 *
 * File Name: gt_tools.h
 * 2007/03/20 created by Jeff Chuang
 */

#include <stdio.h> //for stderr

enum {
_LOGNONE,
_LOGERR,
_LOGINFO,
_LOGDBG
};

/*
 *Function: Set current log level 
 *Input: 1.level: Integer, the level of this log message(1:error, 2:info, 3:Debug)
 *       2.szAppName: name of the application
 *Output: None
 */
extern void setLogLevel(int level, const char *szAppName);
/*
 *Function: Log messages to system log
 *Input: 1.level: Integer, the level of this log message(1:error, 2:info, 3:Debug)
 *			 2.szLogMsg: the log message
 *Output: None
 */
extern void addLog(int level, const char *szLogMsg);
/*
 *Function: clear log
 *Input: None
 *Output: None
 */
extern void clearLog(void);
#endif /* _GT_TOOLS_H_ */

