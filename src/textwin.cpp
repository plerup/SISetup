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
#include "resource.h"
#include "textwin.h"

struct {
   HWND     hFather,       // Possible father window to the text output window
            hDlg;          // The text output window (dialog)
   HWND     hTextCtrl;     // The text control within the window
   WNDPROC  orgWinProc;    // Original window procedure for the text control
   HANDLE   hThread;       // Text window update thread
   UINT     lastPos;       // Previous insert point
   BOOL     cancel;        // Cancel or ok button pressed
} gTextWindow;

HWND gTextWindowHandle;

HFONT GetAFont(char *name, int height, BOOL bold, BOOL italic, BOOL underLine)
{
   LOGFONT lf;

   memset(&lf, 0, sizeof(lf));

   lf.lfHeight = height;
   lf.lfWidth = 0;
   lf.lfEscapement = 0;
   lf.lfOrientation = 0;
   lf.lfWeight = bold ? 900 : 0;
   lf.lfItalic = italic;
   lf.lfUnderline = underLine;
   lf.lfStrikeOut = 0;
   lf.lfCharSet = ANSI_CHARSET;
   lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
   lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
   lf.lfQuality = DEFAULT_QUALITY;
   lf.lfPitchAndFamily = DEFAULT_PITCH;
   strcpy(lf.lfFaceName, name);
   lf.lfPitchAndFamily = DEFAULT_PITCH | FF_MODERN;

   return CreateFontIndirect(&lf);
}

//--------------------------------------------------------------------------

LRESULT APIENTRY EditSubclassProc(HWND hwnd, 
                                  UINT uMsg, 
                                  WPARAM wParam, 
                                  LPARAM lParam) 
{
   // Filter normal keybard input
   if ((!HIBYTE(GetKeyState(VK_CONTROL)) && uMsg == WM_CHAR && wParam >= ' ') || 
        uMsg == WM_PASTE)
   {
      MessageBeep(1);
      return TRUE;
   }

   return CallWindowProc(gTextWindow.orgWinProc, hwnd, uMsg, wParam, lParam); 
} 

BOOL FAR PASCAL TextWindDlgProc(HWND hDlg,
                                UINT message,
                                UINT wParam,
                                LONG lParam)
{
   switch (message)
   {
      case WM_INITDIALOG:
         SetClassLongPtr(hDlg, GCLP_HICON, 
                        (long)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1)));
         CenterDlg(hDlg);
         gTextWindow.hDlg = hDlg;
         gTextWindowHandle = hDlg;
         gTextWindow.lastPos = 0;
         {
            // Set a non proportional font
            HFONT hFont = GetAFont("Courier", 9, FALSE, FALSE, FALSE);
            SendMessage(GetDlgItem(hDlg, IDC_LINES), WM_SETFONT, (WPARAM)hFont, (LPARAM)TRUE);
         }
         SendMessage(GetDlgItem(hDlg, IDC_LINES), EM_LIMITTEXT, 0, 0);
         EnableWindow(GetDlgItem(hDlg, IDCANCEL), TRUE);
         SetFocus(GetDlgItem(hDlg, IDC_LINES));

         gTextWindow.cancel = FALSE;

         // Subclass edit control to prevent keyboard input
         gTextWindow.orgWinProc = (WNDPROC)SetWindowLongPtr(gTextWindow.hTextCtrl, GWLP_WNDPROC, (LONG)EditSubclassProc); 

         gTextWindow.hTextCtrl = GetDlgItem(hDlg, IDC_LINES);
         return FALSE;
         break;


      case WM_COMMAND:
         switch (LOWORD(wParam))
         {
            case IDCANCEL:
               gTextWindow.cancel = TRUE;
               return TRUE;
               break;

             default:
               break;
        }
            
      default:
         break;
   }
   return FALSE;
}

//--------------------------------------------------------------------------

long WINAPI TextWindThreadProc(LPARAM lParam)
{
   HWND hDlg = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_TEXTWIND), gTextWindow.hFather, (DLGPROC)TextWindDlgProc);

   MSG msg;
   while (!gTextWindow.cancel && GetMessage(&msg, NULL, 0, 0)) 
   { 
      if (!IsDialogMessage(hDlg, &msg)) 
      { 
         TranslateMessage(&msg); 
         DispatchMessage(&msg); 
      } 
   }
   DestroyWindow(hDlg);
   return 0;
}

//--------------------------------------------------------------------------

BOOL CreateTextWindow(HWND hFather, BOOL show)
{
   ULONG id;
   
   gTextWindow.hFather = hFather;
   gTextWindow.hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TextWindThreadProc, 0, 0, &id);
   if (!gTextWindow.hThread)
      ;//Fatal("Failed to create thread process for text window", "");
   else
   {
      int cnt = 0;
      // Wait for the dialog window to be created
      while (!gTextWindow.hTextCtrl && cnt++ < 100)
      {
         Sleep(50);
      }
      if (show)
         ShowWindow(gTextWindow.hDlg, SW_SHOW);
   }

   return TRUE;
}

//--------------------------------------------------------------------------

BOOL TextWindowString(char *form, char *str)
{
   char outStr[1024];
   DWORD len = (DWORD)SendMessage(gTextWindow.hTextCtrl, WM_GETTEXTLENGTH, 0, 0);

   sprintf(outStr, form, str);
   gTextWindow.lastPos = len;
   SendMessage(gTextWindow.hTextCtrl, EM_SETSEL, (WPARAM)len, (LPARAM)len);
   SendMessage(gTextWindow.hTextCtrl, EM_REPLACESEL, (WPARAM)0, (LPARAM)outStr);
   return TRUE;
}

//--------------------------------------------------------------------------

BOOL TextWindowUndoString()
{
   DWORD len = (DWORD)SendMessage(gTextWindow.hTextCtrl, WM_GETTEXTLENGTH, 0, 0);
   SendMessage(gTextWindow.hTextCtrl, EM_SETSEL, (WPARAM)gTextWindow.lastPos, (LPARAM)len);
   SendMessage(gTextWindow.hTextCtrl, EM_REPLACESEL, (WPARAM)0, (LPARAM)"");
   return TRUE;
}

//--------------------------------------------------------------------------

void CloseTextWindow()
{
   if (IsWindowVisible(gTextWindow.hDlg))
   {
      SetWindowText(GetDlgItem(gTextWindow.hDlg, IDCANCEL), "Ok");
      // Wait for user confirmation
      WaitForSingleObject(gTextWindow.hThread, INFINITE);
   }
   CloseHandle(gTextWindow.hThread);
}

//--------------------------------------------------------------------------

BOOL TextWindowCancel()
{
   return gTextWindow.cancel;
}
