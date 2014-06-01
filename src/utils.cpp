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

#include <ShlObj.h>
#include <wchar.h>

#include <lm.h>
#include <Shellapi.h>
#include <tlhelp32.h>

#include <io.h>
#include <stdio.h>
#include <conio.h>

#include "textwin.h"
#include "utils.h"
 

char *CopyString(char *source)
{
   char *str = (char*)malloc(strlen(source)+1);
   strcpy(str, source);
   return str;
}

//--------------------------------------------------------------------------

void ReplaceStr(char *src, char *match, char *repl)
{
   char *pos;
   char *tmp = (char*)malloc(sizeof(tString) + 1024);
   while (pos = strstr(src, match))
   {
      strncpy(tmp, src, pos-src);
      strncpy(&tmp[pos-src], repl, strlen(repl));
      strcpy(&tmp[pos-src+strlen(repl)], pos+strlen(match));
      strcpy(src, tmp);
   }
   free(tmp);
}

//--------------------------------------------------------------------------

BOOL FileExists(char *fileName)
{
   return (_access(fileName, 0) != -1);
}

//--------------------------------------------------------------------------

int WriteAllowed(char *path)
{
   tFileName tst;
   strcpy(tst, path);
   APPEND_BS(tst);
   strcat(tst, "___tmp.tmp");
   HANDLE hf = CreateFile(tst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
   return hf != INVALID_HANDLE_VALUE;
}

//--------------------------------------------------------------------------

char *GetFileNameComp(char *fileName, char *type)
{
   // Get file name component
   static char compStr[_MAX_PATH];
   char drive[_MAX_DRIVE], dir[_MAX_DIR], name[_MAX_FNAME], ext[_MAX_EXT];
   _splitpath(fileName[0] == '"' ? &fileName[1] : fileName, drive, dir, name, ext);
   strcpy(compStr, "");
   if (strstr(type, "drive"))
      strcat(compStr, drive);
   if (strstr(type, "dir"))
      strcat(compStr, dir);
   if (strstr(type, "name"))
      strcat(compStr, name);
   if (strstr(type, "type"))
      strcat(compStr, ext);
   if (compStr[strlen(compStr)-1] == '\"')
      compStr[strlen(compStr)-1] = 0;
   return compStr;
}

//--------------------------------------------------------------------------

BOOL IsDir(char *name)
{
   int attr = GetFileAttributes(name);
   return attr == -1 ? FALSE : (attr & FILE_ATTRIBUTE_DIRECTORY);
}

//--------------------------------------------------------------------------

#define NEXT_DIR(dir) strchr(dir, '\\')
BOOL CreateDir(char *dirName)
{
   tFileName currDir;
   strcpy(currDir, dirName);

   char *pos = NEXT_DIR(currDir);
   if (pos) pos = NEXT_DIR(++pos);
   while (pos)
   {
      char tmp = *pos;
      *pos = 0;
      if (!IsDir(currDir))
         CreateDirectory(currDir, NULL);
      *pos = tmp;
      pos++;
      pos = NEXT_DIR(pos);
   }
   if (!IsDir(dirName))
      CreateDirectory(dirName, NULL);

   return IsDir(dirName);
}

//--------------------------------------------------------------------------

BOOL DelDir(char *dirName)
{

   tFileName wild;
   strcpy(wild, dirName);
   APPEND_BS(wild);
   strcat(wild, "*.*");

   WIN32_FIND_DATA findData;
   HANDLE hFind = FindFirstFile(wild, &findData);
   BOOL ok = hFind != INVALID_HANDLE_VALUE,
        moreFiles = TRUE;

   while (ok && moreFiles)
   {
      tFileName path;
      strcpy(path, dirName);
      APPEND_BS(path);
      strcat(path, findData.cFileName);
      if (!strcmp(findData.cFileName, ".") || !strcmp(findData.cFileName, ".."))
         ;
      else if (IsDir(path))
         ok = DelDir(path);
      else
         ok = SetFileAttributes(path, FILE_ATTRIBUTE_NORMAL) && DeleteFile(path);

      if (ok)
         moreFiles = FindNextFile(hFind, &findData);
   }
   if (hFind != INVALID_HANDLE_VALUE)  
      FindClose(hFind);

   if (ok)
      RemoveDirectory(dirName);
   else
      ErrorMessage("Failed to remove directory: %s", dirName);


   return ok;
}

//--------------------------------------------------------------------------

BOOL EnsureDir(char *fileName)
{
   char *dir = GetFileNameComp(fileName, "drive,dir");
   if (FileExists(dir))
      return TRUE;
   return CreateDir(dir);
}

//--------------------------------------------------------------------------

BOOL String2File(char *fileName, char *str)
{
   FILE *out;
   if (!(out = fopen(fileName, "wt")))
   {
      ErrorMessage("Failed to create file: %s", fileName);
      return FALSE;
   }
   fwrite(str, strlen(str), 1, out);
   fclose(out);
   return TRUE;
}

//--------------------------------------------------------------------------

void CenterDlg(HWND hDlg)
{
   RECT rc;

   GetWindowRect(hDlg, &rc);
   SetWindowPos(hDlg, NULL, 
         ((GetSystemMetrics(SM_CXSCREEN)-(rc.right-rc.left))/2),
         ((GetSystemMetrics(SM_CYSCREEN)-(rc.bottom-rc.top))/3),
            0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

//--------------------------------------------------------------------------

void message(char *form, char *par)
{
   TextWindowString(form, par);
}

//--------------------------------------------------------------------------

BOOL ErrorMessage(char *form, char *par, BOOL fatal, BOOL retry)
{
   tString errMsg;
   sprintf(errMsg, form, par);
   if (retry)
      return MessageBox(gTextWindowHandle, errMsg, APP_NAME" Error", MB_RETRYCANCEL | MB_ICONERROR) == IDRETRY;
   else
      MessageBox(gTextWindowHandle, errMsg, fatal ? APP_NAME" Fatal Error" : APP_NAME" Error", MB_OK | MB_ICONERROR);

   if (fatal)
      DoExit(1);

   return FALSE;
}

//--------------------------------------------------------------------------

void ErrorDialog(DWORD id)
{
   LPVOID msgString;
   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                 FORMAT_MESSAGE_FROM_SYSTEM | 
                 FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL,
                 id,
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 (LPTSTR) &msgString,
                 0,
                 NULL);
   MessageBox(NULL, (LPCTSTR)msgString, "Error", MB_OK | MB_ICONINFORMATION);
   LocalFree(msgString);
}

//--------------------------------------------------------------------------

BOOL ConfirmExit()
{
   message(NEWLINE"Press Ok button to continue"NEWLINE);
   CloseTextWindow();
   return TRUE;
}

//--------------------------------------------------------------------------

void DoExit(int exitCode)
{
   exit(exitCode);
}

//--------------------------------------------------------------------------

#define APP_REG_ROOT "Software\\Peter Lerup\\SISetup"
HKEY gRegRootKey = HKEY_CURRENT_USER;
#define GET_ROOT_KEY(hKey) (hKey ? hKey : gRegRootKey)

void EncodeRegName(char *keyName, char *path, char *name)
{
   char *pos = strrchr(keyName, '\\');
   if (pos)
   {
      strcpy(path, keyName);
      path[pos-keyName] = 0;
      strcpy(name, pos+1);
   }
   else
   {
      strcpy(path, APP_REG_ROOT);
      strcpy(name, keyName);
   }
}

//--------------------------------------------------------------------------

char *GetRegVal(char *valName, HKEY rootKey)
{
   BOOL  ok = FALSE;
   HKEY  hKey;
   DWORD type, size;
   tString path, name;;
   static tString val;

   EncodeRegName(valName, path, name);

   strcpy(val, "");
   if (RegOpenKeyEx(GET_ROOT_KEY(rootKey), path, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
   {
      size = sizeof(val);
      ok = RegQueryValueEx(hKey, name, 0, &type, (unsigned char*)val, &size) == ERROR_SUCCESS;
      RegCloseKey(hKey);
   }
   return ok ? val : NULL;
}

//--------------------------------------------------------------------------

BOOL SetRegVal(char *valName, char *val, HKEY rootKey)
{
   BOOL ok = FALSE;
   HKEY  hKey;
   DWORD disp, size;

   tString path, name;
   EncodeRegName(valName, path, name);

   if (RegCreateKeyEx(GET_ROOT_KEY(rootKey), path, 0, "", REG_OPTION_NON_VOLATILE,
          KEY_ALL_ACCESS, NULL, &hKey, &disp) == ERROR_SUCCESS)
   {
      size = (DWORD)strlen(val)+1;
      ok = (RegSetValueEx(hKey, name, 0, REG_SZ, (unsigned char*)val, size) == ERROR_SUCCESS);
      RegCloseKey(hKey);
   }
   return ok;
}

//--------------------------------------------------------------------------

DWORD RecDelRegKey(char *pKeyName, HKEY hStartKey)
{
   DWORD   dwRtn, dwSubKeyLength;
   tString szSubKey;
   HKEY    hKey;

   if (!hStartKey) hStartKey = gRegRootKey;
   if (!pKeyName) pKeyName = APP_REG_ROOT;

   if (pKeyName && lstrlen(pKeyName))
   {
      if((dwRtn=RegOpenKeyEx(hStartKey,pKeyName, 0, KEY_ENUMERATE_SUB_KEYS | DELETE, &hKey )) == ERROR_SUCCESS)
      {
         while (dwRtn == ERROR_SUCCESS)
         {
            dwSubKeyLength = sizeof(szSubKey);
            dwRtn=RegEnumKeyEx(
                           hKey,
                           0,       // always index zero
                           szSubKey,
                           &dwSubKeyLength,
                           NULL,
                           NULL,
                           NULL,
                           NULL
                         );
 
            if(dwRtn == ERROR_NO_MORE_ITEMS)
            {
               dwRtn = RegDeleteKey(hStartKey, pKeyName);
               break;
            }
            else if (dwRtn == ERROR_SUCCESS)
               dwRtn = RecDelRegKey(szSubKey, hKey);
         }
         RegCloseKey(hKey);
      }
   }
   else
      dwRtn = ERROR_BADKEY;
 
   return dwRtn;
}

//--------------------------------------------------------------------------

BOOL DelRegVal(char *valName, HKEY rootKey)
{
   BOOL  ok = FALSE;
   HKEY  hKey;
   tString path, name;;

   EncodeRegName(valName, path, name);

   if (RegOpenKeyEx(GET_ROOT_KEY(rootKey), path, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
   {
      ok = RegDeleteValue(hKey, name) == ERROR_SUCCESS;
      RegCloseKey(hKey);
   }
   return ok;
}

//--------------------------------------------------------------------------

char *GetInstallKeyName(char *appName)
{
   static tString keyName;
   strcpy(keyName, "installed__");
   strcat(keyName, appName);
   return keyName;
}

//--------------------------------------------------------------------------

char *GetRemoveKeyName(char *appName)
{
   static tString keyName;
   strcpy(keyName, "removed__");
   strcat(keyName, appName);
   return keyName;
}

//--------------------------------------------------------------------------

BOOL GetCurrExePath(char *path)
{
   char *start = GetCommandLine();
   char *end = start;
   if (*start == '"')
   {
      start++;
      end = strchr(start, '"');
      if (!end)
         end = start + strlen(start)+1;
   }
   else
      while (*end && *end != ' ') end++;
   DWORD len = (DWORD)(end-start);
   strncpy(path, start, len);
   path[len] = 0;
   return TRUE;
}

//--------------------------------------------------------------------------

BOOL GetProgFilesDir(char *path, char *subDir)
{
   BOOL ok = FALSE;
   strcpy(path, GetRegVal("Software\\Microsoft\\Windows\\CurrentVersion\\ProgramFilesDir", HKEY_LOCAL_MACHINE));

   ok = path != NULL;
   if (ok)
   {
      if (!FileExists(path))
         ErrorMessage("Program files directory does not exist: %s", path, TRUE);

      APPEND_BS(path);
      if(subDir)
         strcat(path, subDir);
   }

   return ok;
}

//--------------------------------------------------------------------------

BOOL GetStartMenuRoot(char *root, BOOL forceAllUsers = FALSE)
{
   tFileName orgRoot;
   char *regVal;

   if (IsWin9x())
      regVal = GetRegVal("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders\\Programs", HKEY_CURRENT_USER);
   else if (gRegRootKey == HKEY_LOCAL_MACHINE || forceAllUsers)
      regVal = GetRegVal("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders\\Common Programs", HKEY_LOCAL_MACHINE);
   else
      regVal = GetRegVal("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders\\Programs", HKEY_CURRENT_USER);

   if (!regVal)
      return FALSE;

   strcpy(orgRoot, regVal);
   ExpandEnvironmentStrings(orgRoot, root, sizeof(tFileName));
   strcat(root, "\\Sysinternals\\");
   return TRUE;

}

//--------------------------------------------------------------------------

BOOL GetShortcutPath(char *type, char *name, char *path)
{
   strcpy(path, "");
   if (!_stricmp(type, "StartMenu"))
      GetStartMenuRoot(path);
   else if (!_stricmp(type, "AutoStart"))
   {
      tFileName val;
      if (IsWin9x())
         strcpy(val, 
            GetRegVal("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders\\Startup", HKEY_CURRENT_USER));
      else
         strcpy(val, 
                GetRegVal(gRegRootKey == HKEY_CURRENT_USER ? 
                           "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders\\Startup" :
                           "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders\\Common Startup"));
      ExpandEnvironmentStrings(val, path, sizeof(tFileName));
      if (strlen(path))
         strcat(path, "\\");
   }
   else if (!_stricmp(type, "QuickLaunch"))
   {
      tFileName val;
      if (IsWin9x())
         strcpy(val, 
            GetRegVal("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders\\AppData", HKEY_CURRENT_USER));
      else
         strcpy(val, 
                GetRegVal("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders\\AppData"));
      ExpandEnvironmentStrings(val, path, sizeof(tFileName));
      if (strlen(path))
         strcat(path, "\\Microsoft\\Internet Explorer\\Quick Launch\\");
   }

   if (!path || !strlen(path))
   {
      ErrorMessage("Invalid shortcut: %s", type);
      return FALSE;
   }

   strcat(path, name);
   strcat(path, ".lnk");

   return TRUE;
}

//--------------------------------------------------------------------------

BOOL ShortcutExists(char *type, char *entry)
{
   tFileName linkName;
   if (!GetShortcutPath(type, entry, linkName))
      return FALSE;
   return FileExists(linkName);
}

//--------------------------------------------------------------------------

BOOL CreateShortcut(char *type,
                    char *entry, char *targetPath, char *comment, char *arguments,
                    char *workDir, char *iconFile, int iconIndex, int showCommand)
{
   tFileName linkName;
   if (!GetShortcutPath(type, entry, linkName))
      return FALSE;

   EnsureDir(linkName);

   HRESULT hRes; 
   IShellLink* psl; 

   CoInitialize(NULL);
   hRes = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID *)&psl); 
   if (SUCCEEDED(hRes))
   { 
      IPersistFile* ppf; 

      psl->SetPath(targetPath); 
      if (arguments)
         psl->SetArguments(arguments);
      if (workDir)
         psl->SetWorkingDirectory(workDir);
      if (iconFile || iconIndex)
         psl->SetIconLocation(iconFile ? iconFile : targetPath, iconIndex);
      if (comment)
         psl->SetDescription(comment);

      psl->SetShowCmd(showCommand);



      hRes = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf); 

      if (SUCCEEDED(hRes))
      { 
         WCHAR wsz[MAX_PATH]; 
         MultiByteToWideChar(CP_ACP, 0, linkName, -1, wsz, MAX_PATH); 

         hRes = ppf->Save(wsz, TRUE); 
         ppf->Release(); 
      } 
     psl->Release(); 
   } 
   return TRUE; 
} 

//--------------------------------------------------------------------------

BOOL DeleteShortcut(char *type, char *entry)
{
   tFileName linkName;
   if (!GetShortcutPath(type, entry, linkName))
      return FALSE;
   return DeleteFile(linkName);
}

//--------------------------------------------------------------------------

BOOL DelStartMenuEntries(char *subDir)
{
   tString root;
   if (GetStartMenuRoot(root))
   {
      if (subDir)
         strcat(root, subDir);
      if (FileExists(root))
         DelDir(root);
   }
   // Earlier versions created entries for all user when elevated for some reason.
   // Delete if available and possible
   if (GetStartMenuRoot(root, TRUE) && FileExists(root))
      DelDir(root);

   return TRUE;
}

//--------------------------------------------------------------------------

char *CreateURL(char *url, char *path, BOOL startMenu)
{
   static tFileName fileName;

   if (startMenu)
   {
      GetStartMenuRoot(fileName);
      strcat(fileName, path);
   }
   else
      strcpy(fileName, path);

   strcat(fileName, ".url");

   EnsureDir(fileName);

   FILE *urlFile = fopen(fileName, "wt");
   if (!urlFile)
   {
      ErrorMessage("Failed to create URL shortcut: %s", fileName);
      return NULL;
   }
   fprintf(urlFile, "[InternetShortcut]\nURL=http://%s", url);
   fclose(urlFile);
   return fileName;
}


//--------------------------------------------------------------------------

HWND CreateTooltip(HWND win, char *text)
{
   INITCOMMONCONTROLSEX icc;
   HWND ttWin;
   TOOLINFO toolInfo;
   unsigned int uid = 0;       // for ti initialization
   RECT rect;                  // for client area coordinates

   icc.dwICC = ICC_WIN95_CLASSES;
   icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
   InitCommonControlsEx(&icc);

   ttWin = CreateWindowEx(WS_EX_TOPMOST,
                          TOOLTIPS_CLASS,
                          NULL,
                          WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          (HWND)win,
                          NULL,
                          NULL,
                          NULL);

   SetWindowPos(ttWin, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

   // Hotzone is entire widget window
   GetClientRect((HWND) win, &rect);

   toolInfo.cbSize = sizeof(TOOLINFO);
   toolInfo.uFlags = TTF_SUBCLASS;
   toolInfo.hwnd = (HWND) win;
   toolInfo.hinst = NULL;
   toolInfo.uId = uid;
   toolInfo.lpszText = text;
   toolInfo.rect.left = rect.left;
   toolInfo.rect.top = rect.top;
   toolInfo.rect.right = rect.right;
   toolInfo.rect.bottom = rect.bottom;

   SendMessage(ttWin, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &toolInfo);
   SendMessage(ttWin, TTM_SETMAXTIPWIDTH, 0, (LPARAM)(INT) 2048);

   return ttWin;
}

//--------------------------------------------------------------------------

DWORD spawn(char *command, BOOL show)
{
   PROCESS_INFORMATION	stProcInfo;
   STARTUPINFO				stStartInfo;
   SECURITY_ATTRIBUTES  stSecAttr;
   BOOL						ok = FALSE;
   DWORD						status = 0;

   memset(&stStartInfo, 0, sizeof(stStartInfo));
   stStartInfo.cb = sizeof(stStartInfo);
   stStartInfo.dwFlags = STARTF_USESHOWWINDOW;
   stStartInfo.wShowWindow  = show ? SW_SHOW : SW_HIDE;

   stSecAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
   stSecAttr.lpSecurityDescriptor = NULL;
   stSecAttr.bInheritHandle = TRUE;

   ok = CreateProcess(NULL, command, &stSecAttr, &stSecAttr, FALSE, NORMAL_PRIORITY_CLASS,
	                   NULL, NULL, &stStartInfo, &stProcInfo);
   if (ok)
   {
	   CloseHandle(stProcInfo.hProcess);
	   CloseHandle(stProcInfo.hThread);
   }
   else
     status = GetLastError();

   return status;
}

//--------------------------------------------------------------------------

DWORD RunElevated(char *com, char *params)
{
   SHELLEXECUTEINFO shExecInfo;

   shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);

   shExecInfo.fMask = NULL;
   shExecInfo.hwnd = NULL;
   shExecInfo.lpVerb = "runas";
   shExecInfo.lpFile = com;
   shExecInfo.lpParameters = params;
   shExecInfo.lpDirectory = NULL;
   shExecInfo.nShow = SW_SHOWNORMAL;
   shExecInfo.hInstApp = NULL;

   return ShellExecuteEx(&shExecInfo);
}

//--------------------------------------------------------------------------

BOOL IsWin9x()
{
   OSVERSIONINFO osvi;
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
   GetVersionEx(&osvi);
   return osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS;
}

//--------------------------------------------------------------------------

BOOL IsVista()
{
   OSVERSIONINFO osvi;
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
   GetVersionEx(&osvi);
   return osvi.dwMajorVersion > 5;
}

//--------------------------------------------------------------------------

char* stristr(char* str, char* match)
{
   char *src = str;
   size_t srcLen, matchLen;
   // Validate parameters
   if (!str || !str[0] || !match || !match[0])
      return NULL;
   srcLen = strlen(src);
   matchLen = strlen(match);
   while (*src)
   {
      char *srcPos = src,
           *matchPos = match;
      if (matchLen > srcLen)
         // Match not possible
         return NULL;
      // Search from current position
      while (*srcPos && toupper(*srcPos) == toupper(*matchPos))
      {
         srcPos++;
         matchPos++;
         if (!*matchPos)
            // At end, match found
            return src;
      }
      src++;
      srcLen--;
   }
	return NULL;
}

//--------------------------------------------------------------------------

int CALLBACK PromptDirDlgProc(HWND hwnd,UINT uMsg,LPARAM lp, LPARAM pData)
{
   TCHAR szDir[_MAX_PATH];
   switch(uMsg)
   {
      case BFFM_INITIALIZED:
         // Center dialog and set inial directory
         CenterDlg(hwnd);
         strcpy(szDir, (char*)pData);
         if (szDir[strlen(szDir)-1] != '\\')
            strcat(szDir, "\\");
         SendMessage(hwnd,BFFM_SETSELECTION, TRUE, (LPARAM)szDir);
         break;

      case BFFM_SELCHANGED:
         // Set the status window to the currently selected path.
         if (SHGetPathFromIDList((LPITEMIDLIST) lp ,szDir))
            SendMessage(hwnd,BFFM_SETSTATUSTEXT,0,(LPARAM)szDir);
         break;

      default:
         break;
   }
   return 0;
}

//--------------------------------------------------------------------------

char *PromptDirectory(HWND father, char *startDir, char *message)
{
   BROWSEINFO bi;
   LPITEMIDLIST pidl;
   LPMALLOC pMalloc;
   BOOL ok = FALSE;
   static char result[_MAX_PATH];

   strcpy(result, "");

   if (SUCCEEDED(SHGetMalloc(&pMalloc)))
   {
      ZeroMemory(&bi,sizeof(bi));
      bi.hwndOwner = father;
      bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT | BIF_EDITBOX | BIF_NEWDIALOGSTYLE;
      bi.lpfn = PromptDirDlgProc;
      bi.lParam = (LPARAM)startDir;
      bi.lpszTitle = message;

      pidl = SHBrowseForFolder(&bi);

      ok = pidl && SHGetPathFromIDList(pidl, result);
   }

   pMalloc->Free(pidl);
   pMalloc->Release();

   return result;
}

