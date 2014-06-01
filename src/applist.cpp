/*
Copyright (C) 2014 Peter Lerup

This file is part of SISetup.

SISetup is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include <windows.h>
#include <stdio.h>

#include "utils.h"
#include "applist.h"

tAppEntry *gAppList = NULL;

tSettings gSettings = {"download.sysinternals.com", "www.sysinternals.com", TRUE };

#define SKIP_SPACE { while (*pos && (*pos == ' ' || *pos == '\t')) pos++; }
#define SYNTAX_ERROR { ErrorMessage("Configuration file syntax error: %s", line); continue;}
#define GET_VAL CopyString(val)
#define GET_BOOL (toupper(val[0]) == 'Y')

BOOL NewApp(tAppEntry *app)
{
   // Validate
   if (!app->source)
   {
      ErrorMessage("No source defined for: %s", app->name);
      return FALSE;
   }
   // Insert current definition in the list
   if (!gAppList)
      gAppList = app;
   else
   {
      tAppEntry *elem = gAppList;
      while (elem->next)
         elem = elem->next;
      elem->next = app;
   }
   return TRUE;
}

BOOL ReadAppList(char *fileName)
{
   FILE *listFile = fopen(fileName, "rt");
   if (!listFile)
      return FALSE;

   tString line;
   tAppEntry *pAppEntry = NULL;
   while (fgets(line, sizeof(line), listFile))
   {
      ReplaceStr(line, "\\n", "\r\n");
      size_t len = strlen(line);
      if (len)
         line[len-1] = 0;
      char *pos = line;
      SKIP_SPACE;
      if (!*pos || *pos == '#')
         continue;

      char *key = pos, 
           *val;
      while (*pos && isalpha(*pos)) pos++;
      SKIP_SPACE;
      if (*pos != '=')
         SYNTAX_ERROR;
      pos++;
      SKIP_SPACE;
      val = pos;
      
      if (strstr(key, "site") == key)
         gSettings.SIDownloadSite = GET_VAL;
      else if (strstr(key, "home") == key)
         gSettings.SIHomeSite = GET_VAL;
      else if (strstr(key, "autoupdate") == key)
         gSettings.autoUpdate = GET_BOOL;
      else if (strstr(key, "name"))
      {
         // New application definition
         if (pAppEntry)
            if (!NewApp(pAppEntry))
               free(pAppEntry);
         pAppEntry = (tAppEntry*)malloc(sizeof(tAppEntry));
         memset(pAppEntry, 0, sizeof(tAppEntry));
         pAppEntry->name = GET_VAL;
         pAppEntry->use = GetRegVal(GetInstallKeyName(pAppEntry->name)) != NULL;
         pAppEntry->autoStart = ShortcutExists("AutoStart", pAppEntry->name);
         pAppEntry->ql = ShortcutExists("QuickLaunch", pAppEntry->name);
      }
      else if (pAppEntry)
      {
         if (strstr(key, "source"))
            pAppEntry->source = GET_VAL;
         else if (strstr(key, "destination"))
            pAppEntry->destination = GET_VAL;
         else if (strstr(key, "menu"))
            pAppEntry->menu = GET_VAL;
         else if (strstr(key, "tip"))
            pAppEntry->tip = GET_VAL;
         else if (strstr(key, "autostart"))
            pAppEntry->autoStart = GET_BOOL;
         else if (strstr(key, "quicklaunch"))
            pAppEntry->ql = GET_BOOL;
         else if (strstr(key, "default") && GetRegVal(GetRemoveKeyName(pAppEntry->name)) == NULL)
            pAppEntry->_default = GET_BOOL;
      }
   }
   if (pAppEntry)
      NewApp(pAppEntry);

   fclose(listFile);

   return TRUE;
}

