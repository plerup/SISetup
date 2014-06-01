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

#include<windows.h>
#include<wininet.h>

#include <stdio.h>

#include "utils.h"

HINTERNET gHSession,
          gHConnect;

DWORD gLastError = 0;

#define SET_LAST_ERROR gLastError = GetLastError();
#define ERROR_EXIT { SET_LAST_ERROR return FALSE; }

BOOL IFileBegin(char *location)
{
   gHSession = InternetOpen("Mozilla/4.0",
                            INTERNET_OPEN_TYPE_PRECONFIG,
                            NULL, 
                            NULL,
                            0);
   if (gHSession == NULL)
      ERROR_EXIT;

   gHConnect = InternetConnect(gHSession,
                               location,
                               INTERNET_DEFAULT_HTTP_PORT,
                               "",
                               "",
                               INTERNET_SERVICE_HTTP,
                               0,
                               0);

   if (gHConnect == NULL)
   {
      SET_LAST_ERROR
      InternetCloseHandle(gHSession);
      return FALSE;
   }

   return TRUE;

}

//--------------------------------------------------------------------------

#define REQ_FLAGS INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_RELOAD

BOOL IFileGetDate(char *fileName, tString *date)
{
   BOOL ok = FALSE;
   HINTERNET hFile = HttpOpenRequest(gHConnect,
                                     "GET",
                                     fileName,
                                     HTTP_VERSION,
                                     NULL,
                                     0,
                                     REQ_FLAGS,
                                     0) ;
   if (hFile == NULL)
      ERROR_EXIT;

   if (HttpSendRequest(hFile, NULL, 0, 0, 0))
   {
      DWORD size = sizeof(tString);
      ok = HttpQueryInfo(hFile, HTTP_QUERY_LAST_MODIFIED, date, &size, 0);
   }
   SET_LAST_ERROR
   InternetCloseHandle(hFile);

   return ok;
}

//--------------------------------------------------------------------------

char gBuffer[102400];

BOOL IFileCopy(char *source, char *destination)
{
   BOOL ok = FALSE;
   HINTERNET hFile = HttpOpenRequest(gHConnect,
                                     "GET",
                                     source,
                                     HTTP_VERSION,
                                     NULL,
                                     0,
                                     REQ_FLAGS,
                                     0);

   if (hFile == NULL)
      ERROR_EXIT;
   
   if (HttpSendRequest(hFile, NULL, 0, 0, 0))
   {
      HANDLE hOut = CreateFile(destination, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
      if (hOut != INVALID_HANDLE_VALUE)
      {
         DWORD readCnt, writeCnt;
         ok = TRUE;
         while (ok && InternetReadFile(hFile, gBuffer, sizeof(gBuffer), &readCnt) && readCnt)
         {
            ok = WriteFile(hOut, gBuffer, readCnt, &writeCnt, NULL);
         }
         CloseHandle(hOut);
         if (!ok)
            DeleteFile(destination);
      }
   }
   SET_LAST_ERROR

   InternetCloseHandle(hFile);
   return ok;
}

//--------------------------------------------------------------------------


BOOL IFileGetText(char *source, char *txt)
{
   BOOL ok = FALSE;
   HINTERNET hFile = HttpOpenRequest(gHConnect,
                                     "GET",
                                     source,
                                     HTTP_VERSION,
                                     NULL,
                                     0,
                                     REQ_FLAGS,
                                     0);

   if (hFile == NULL)
      ERROR_EXIT;
   
   if (HttpSendRequest(hFile, NULL, 0, 0, 0))
   {
      DWORD readCnt;
      ok = InternetReadFile(hFile, txt, sizeof(tString), &readCnt);
      txt[readCnt] = 0;
   }
   SET_LAST_ERROR

   InternetCloseHandle(hFile);
   return ok;
}

//--------------------------------------------------------------------------


BOOL IFileEnd()
{
   BOOL ok = InternetCloseHandle(gHConnect);
   return InternetCloseHandle(gHSession) && ok;
}

//--------------------------------------------------------------------------

DWORD IFileLastError()
{
   return gLastError;
}


