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
#include <process.h>

#include "resource.h"

#include "utils.h"
#include "textwin.h"
#include "IFile.h"
#include "applist.h"

#include "XUnzip.h"

#ifdef _WIN64
#define EXE_TYPE "_x64.exe"
#else
#define EXE_TYPE ".exe"
#endif

#define SHORT_DESCR "Sysinternals installation/update program"

#define APP_VERSION "1.3.2"
#define HOME_SITE "www.lerup.com"
#define APP_HOME_URL "SISetup/SISetup"EXE_TYPE
#define APP_VERSION_KEY "Version"
#define APP_VERSION_URL "SISetup/version"
#define OLD_EXT "_old"

#define INSTALL_DIR_NAME "Sysinternals"
#define INSTALL_DIR_KEY "InstallDir"

#define RUN_AT_LOGON_SHORTCUT "SISetup Check for updates"

#define UNINSTALL_REGKEY "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"APP_NAME
#define COMPASS_REGKEY "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Persisted"

HINSTANCE gHInstance;

tFileName gAppPath,
          gInstDir,
          gInstAppPath,
          gCfgFile;

BOOL gSilent = FALSE;


//--------------------------------------------------------------------------

BOOL WINAPI InstallDirDlgProc(HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
   char *choice;
   switch (message)
   {
      case WM_INITDIALOG:
         CenterDlg(hDlg);
         SetWindowText(GetDlgItem(hDlg, IDC_INST_DIR), gInstDir);
			return TRUE;
      
      case WM_COMMAND:
         switch (wParam)
         {
            case IDOK:
               GetWindowText(GetDlgItem(hDlg, IDC_INST_DIR), gInstDir, sizeof(gInstDir));
               EndDialog(hDlg, TRUE);
               return TRUE;
               break;

            case IDCANCEL:
               EndDialog(hDlg, FALSE);
               return TRUE;
               break;
               
            case ID_BROWSE:
               choice = PromptDirectory(gTextWindowHandle, gInstDir, "");
               if (strlen(choice))
                  SetWindowText(GetDlgItem(hDlg, IDC_INST_DIR), choice);
               break;
               
            default:
               break;
         }
      break;
   }
   
   return FALSE;
}

//--------------------------------------------------------------------------

void StartMenuEntries()
{
   // Sysinternals home shortcut
   CreateURL(gSettings.SIHomeSite, "Sysinternals Home", 1);

   // Remove all old entries
   DelStartMenuEntries(APP_NAME);
   // SISetup startup menu entries
   CreateShortcut("StartMenu", APP_NAME"\\Check for updates", gInstAppPath, "Check for updates on the Sysinternals web site", "-update", NULL, NULL, 2);
   CreateShortcut("StartMenu", APP_NAME"\\Install more", gInstAppPath, "Install more Sysinternals programs");
   CreateShortcut("StartMenu", APP_NAME"\\Uninstall", gInstAppPath, "Uninstall SISetup and all Sysinternals programs", "-uninstall", NULL, NULL, 1);
   CreateURL(HOME_SITE"/"APP_NAME"/help.html", APP_NAME"\\Help", 1);
}

//--------------------------------------------------------------------------

void HomePage()
{
   tString   com;
   char *fileName = CreateURL(HOME_SITE"/"APP_NAME, GetFileNameComp(gInstAppPath, "drive,dir,name"), FALSE);
   if (!fileName) return;

   strcpy(com, getenv("COMSPEC"));
   strcat(com, " /c \"");
   strcat(com, fileName);
   strcat(com, "\"");

   spawn(com, FALSE);
}

//--------------------------------------------------------------------------

char *ShortTime(char *longTime)
{
   static tString time;
   strcpy(time, " (");
   strcat(time, longTime+5);
   strcpy(time+19, ")");
   return time;
}

//--------------------------------------------------------------------------

BOOL Init()
{
   BOOL appUpdate = FALSE;
   BOOL doPromptDir = FALSE;

   CreateTextWindow(NULL, !gSilent);
   message(NEWLINE"--- " APP_NAME " --- " SHORT_DESCR " ---"NEWLINE NEWLINE, "");

   GetCurrExePath(gAppPath);

   // Setup installation directory
   char *dir = GetRegVal(INSTALL_DIR_KEY);
   if (dir)
      // Installation done before
      strcpy(gInstDir, dir);
   else
   {
      // Try standard installation dir (Program Files)
      GetProgFilesDir(gInstDir, "");
      BOOL writable = WriteAllowed(gInstDir);
      strcat(gInstDir, INSTALL_DIR_NAME"\\");
      if (stristr(gAppPath, gInstDir))
      {
         // Already in the standard directory, most likely a pre version 1.2 installation.
         // No prompting unless the directory is write protected (Vista UAC)
         if (!writable)
         {
            doPromptDir = TRUE;
            ErrorMessage("The current installation directory:\n\"%s\"\n"
               "is readonly. Please uninstall SISetup using the coming elevated\n"
               "instance and then reinstall it from www.lerup.com/SISetup\n",
               gInstDir);
            RunElevated(gAppPath, "-uninstall");
            return FALSE;
         }
      }
      else
         doPromptDir = TRUE;
      if (doPromptDir && !writable)
      {
         // Default to user top directory
         strcpy(gInstDir, getenv("USERPROFILE"));
         strcat(gInstDir, "\\"INSTALL_DIR_NAME);
      }
   }

   // Select directory when applicable
   if (doPromptDir && !DialogBox(gHInstance, MAKEINTRESOURCE(IDD_SELECTINST), gTextWindowHandle, (DLGPROC)InstallDirDlgProc))
      return FALSE;

   APPEND_BS(gInstDir);

   if (!FileExists(gInstDir))
      if (!CreateDirectory(gInstDir, NULL))
         ErrorMessage("Failed to create installation directory: %s", gInstDir, TRUE);

   // If not running from the installation directory then do copy
   strcpy(gInstAppPath, gInstDir);
   strcat(gInstAppPath, APP_NAME);
   strcat(gInstAppPath, EXE_TYPE);
   if (!stristr(gAppPath, gInstDir))
   {
      if (!CopyFile(gAppPath, gInstAppPath, FALSE))
         ErrorMessage("Failed to copy the application to: %s", gInstDir, TRUE);
      appUpdate = TRUE;
   }

   // Configuration file
   strcpy(gCfgFile, gInstDir);
   strcat(gCfgFile, APP_NAME ".txt");
   if (!FileExists(gCfgFile))
   {
      // Get from current application directory if available
      tFileName altCfg;
      strcpy(altCfg, GetFileNameComp(gAppPath, "drive,dir"));
      strcat(altCfg, GetFileNameComp(gCfgFile, "name,type"));
      if (FileExists(altCfg) && !CopyFile(altCfg, gCfgFile, FALSE))
         ErrorMessage("Failed to copy configuration file: %s", altCfg);
   }

   if (!FileExists(gCfgFile))
   {
      // Get default from home site
      message("Retrieving default configuration file from: %s ..."NEWLINE NEWLINE, HOME_SITE);
      tFileName defCfg;
      strcpy(defCfg, "SISetup/");
      strcat(defCfg, GetFileNameComp(gCfgFile, "name,type"));
      IFileBegin(HOME_SITE);
      IFileCopy(defCfg, gCfgFile);
      IFileEnd();
      if (!FileExists(gCfgFile))
         ErrorMessage("Failed to get default configuration file", NULL, TRUE);
   }

   // Store current version and installation directory
   SetRegVal(APP_VERSION_KEY, APP_VERSION);
   SetRegVal(INSTALL_DIR_KEY, gInstDir);

   if (appUpdate)
      StartMenuEntries();

   return TRUE;
}

//--------------------------------------------------------------------------

BOOL DoUnzip(char *zipFileName, char *destDir)
{
   // Unpack all files in a zip file
   HZIP     hz;
   ZIPENTRY ze;
   int      tot, cnt;
   tFileName outName;

   hz = OpenZip(zipFileName, 0, ZIP_FILENAME);
   if (!hz)
   {
      ErrorMessage("Failed to read zip file: %s", zipFileName);
      return FALSE;
   }

   GetZipItem(hz, -1, &ze);
   tot = ze.index;

   BOOL ok = TRUE;
   cnt = 0;
   while (ok && cnt < tot)
   {
      GetZipItem(hz, cnt, &ze);
      strcpy(outName, destDir);
      strcat(outName, ze.name);
      if (FileExists(outName))
         SetFileAttributes(outName, FILE_ATTRIBUTE_NORMAL);
      while (ok && UnzipItem(hz, cnt, outName, 0, ZIP_FILENAME) != ZR_OK)
      {
         if (GetLastError() == 32)
            ok = ErrorMessage("File: \"%s\" is locked.\nPlease terminate the program if it is running", outName, FALSE, TRUE);
         else
         {
            ErrorMessage("Failed to extract: %s", outName);
            ok = FALSE;
         }
      }
      cnt++;
   }
   CloseZip(hz);

   return ok;
}

//--------------------------------------------------------------------------

BOOL WINAPI SelectAppsDlgProc(HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
   tAppEntry *appEntry = NULL;
   int ID = IDC_CHECK1;
   int inc, dist;

   switch (message)
   {
      case WM_INITDIALOG:
         // Insert data from application list
         {
            RECT r1, r2;
            GetWindowRect(GetDlgItem(hDlg, IDC_CHECK1), &r1);
            GetWindowRect(GetDlgItem(hDlg, IDC_CHECK4), &r2);
            inc = r2.top-r1.top;
         }
         while (appEntry = appEntry ? appEntry->next : gAppList)
         {
            HWND item = GetDlgItem(hDlg, ID);
            if (!item)
               break;
            SetWindowText(item, appEntry->name);
            CreateTooltip(item, appEntry->tip);
            BOOL enabled = appEntry->use || appEntry->_default;
            CheckDlgButton(hDlg, ID++, enabled);
            CheckDlgButton(hDlg, ID++, appEntry->autoStart && enabled);
            CheckDlgButton(hDlg, ID++, appEntry->ql && enabled);
         }
         // Update the dialog box accordingly
         dist = 0;
         while (GetDlgItem(hDlg, ID))
         {
            // Remove unused check boxes
            DestroyWindow(GetDlgItem(hDlg, ID++));
            DestroyWindow(GetDlgItem(hDlg, ID++));
            DestroyWindow(GetDlgItem(hDlg, ID++));
            dist += inc;
         }
         {
            // Resize the static frame
            RECT dlgRect, r;
            GetWindowRect(hDlg, &dlgRect); 
            HWND w = GetDlgItem(hDlg, IDC_GBOX1);
            GetWindowRect(w, &r);
            SetWindowPos(w, NULL, 0, 0, r.right-r.left, r.bottom-r.top-dist, SWP_NOMOVE | SWP_NOZORDER);

            // Move the other check box and buttons
            w = GetDlgItem(hDlg, IDC_CHECK_LOGON);
            GetWindowRect(w, &r);
            int y = r.top-dlgRect.top-GetSystemMetrics(SM_CYSIZE)-GetSystemMetrics(SM_CYSIZEFRAME)-dist;
            SetWindowPos(w, NULL, r.left-dlgRect.left, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOCOPYBITS);
            w = GetDlgItem(hDlg, IDOK);
            GetWindowRect(w, &r);
            y = r.top-dlgRect.top-GetSystemMetrics(SM_CYSIZE)-GetSystemMetrics(SM_CYSIZEFRAME)-dist;
            SetWindowPos(w, NULL, r.left-dlgRect.left, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOCOPYBITS);
            w = GetDlgItem(hDlg, IDCANCEL);
            GetWindowRect(w, &r);
            SetWindowPos(w, NULL, r.left-dlgRect.left, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOCOPYBITS);
            w = GetDlgItem(hDlg, IDHELP);
            GetWindowRect(w, &r);
            SetWindowPos(w, NULL, r.left-dlgRect.left, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOCOPYBITS);

            // And the dialog
            SetWindowPos(hDlg, NULL,0, 0, dlgRect.right-dlgRect.left, dlgRect.bottom-dlgRect.top-dist, SWP_NOMOVE | SWP_NOZORDER | SWP_NOCOPYBITS);
        }

         CheckDlgButton(hDlg, IDC_CHECK_LOGON, ShortcutExists("AutoStart", RUN_AT_LOGON_SHORTCUT));

         CenterDlg(hDlg);
			return TRUE;
      
      case WM_COMMAND:
         switch (wParam)
         {
            case IDOK:
               // Update the application list in accordance with user selection
               while (appEntry = appEntry ? appEntry->next : gAppList)
               {
                  if (IsDlgButtonChecked(hDlg, ID))
                     appEntry->use = TRUE;
                  else if (appEntry->use)
                     // Previously installed but now deselected, remove
                     appEntry->remove = TRUE;
                  ID++;
                  appEntry->autoStart = IsDlgButtonChecked(hDlg, ID);
                  ID++;
                  appEntry->ql = IsDlgButtonChecked(hDlg, ID);
                  ID++;
               }
               gSettings.runAtLogin = IsDlgButtonChecked(hDlg, IDC_CHECK_LOGON);
               EndDialog(hDlg, TRUE);
               return TRUE;
               break;

            case IDCANCEL:
               EndDialog(hDlg, FALSE);
               return TRUE;
               break;
               
            case IDHELP:
               HomePage();
               return TRUE;
               break;
               
            default:
               break;
         }
      break;
   }
   
   return FALSE;
}

//--------------------------------------------------------------------------

void MakeVisible()
{
   // Show window now if in silent mode
   if (gSilent)
   {
      ShowWindow(gTextWindowHandle, SW_SHOW);
      gSilent = FALSE;
   }

}

//--------------------------------------------------------------------------

BOOL Update(BOOL showDialog)
{

   if (showDialog && !DialogBox(gHInstance, MAKEINTRESOURCE(IDD_SELECTAPPS), gTextWindowHandle, (DLGPROC)SelectAppsDlgProc))
      DoExit();

   // Install or update applications in accordance to the configuration
   IFileBegin(gSettings.SIDownloadSite);

   tAppEntry *appEntry = NULL;
   while ((appEntry = appEntry ? appEntry->next : gAppList) && !TextWindowCancel())
   {
      if (!appEntry->use)
         continue;

      message("%-20s : ", appEntry->name);

      // Destination directory
      tFileName destDir;
      strcpy(destDir, gInstDir);
      if (*(appEntry->destination) == '\\') TRIM_BS(destDir);
      strcat(destDir, appEntry->destination);
      strcat(destDir, "\\");

      tFileName targetPath;
      strcpy(targetPath, destDir);
      strcat(targetPath, appEntry->menu);

      // Registry info storage
      tString keyName;
      strcpy(keyName, GetInstallKeyName(appEntry->name));

      if (appEntry->remove)
      {
         message("Removing"NEWLINE);
         if (DelDir(destDir))
         {
            DeleteShortcut("StartMenu", appEntry->name);
            if (appEntry->autoStart)
               DeleteShortcut("AutoStart", appEntry->name);
            if (appEntry->ql)
               DeleteShortcut("QuickLaunch", appEntry->name);
            DelRegVal(keyName);
            SetRegVal(GetRemoveKeyName(appEntry->name), "1");
         }
         else
            ErrorMessage("Failed to remove: %s", destDir);
         continue;
      }
      else
         DelRegVal(GetRemoveKeyName(appEntry->name));

      tString currDate;
      if (!IFileGetDate(appEntry->source, &currDate))
      {
         ErrorMessage("%s is not available", appEntry->source);
         message("Failed"NEWLINE);
         continue;
      }

      char *lastDate = GetRegVal(keyName);
      if (!lastDate || strcmp(currDate, lastDate) || !FileExists(destDir))
      {
         // Update needed
         message(lastDate ? "Updating" : "Installing");
         message(ShortTime(currDate));

         MakeVisible();

         // Copy the zip to an intermediate location
         tFileName destFileName;
         strcpy(destFileName, gInstDir);
         strcat(destFileName, "temp.zip");
         DeleteFile(destFileName);
         IFileCopy(appEntry->source, destFileName);
         if (!FileExists(destFileName))
         {
            ErrorMessage("Failed to get: %s", appEntry->source);
            continue;
         }

         // Destination validation
         if (!FileExists(destDir))
            if (!CreateDirectory(destDir, NULL))
            {
               ErrorMessage("Failed to create directory: %s", destDir);
               continue;
            }

         // Unpack
         BOOL ok = DoUnzip(destFileName, destDir);
         // Delete intermediate file
         DeleteFile(destFileName);
         if (!ok)
         {
            message(" * Failed!"NEWLINE);
            continue;
         }

         // Update the registry for this application
         SetRegVal(keyName, currDate);
      }
      else
      {
         message("Up to date");
         message(ShortTime(currDate));
      }

      // Ensure that all applicable shortcuts are available
      if (appEntry->menu && !ShortcutExists("StartMenu", appEntry->name))
      {
         if (strlen(appEntry->menu))
            CreateShortcut("StartMenu", appEntry->name, targetPath, appEntry->tip);
         else
            CreateShortcut("StartMenu", appEntry->name, getenv("ComSpec"), appEntry->tip, NULL, targetPath);
      }
      if (appEntry->autoStart)
         CreateShortcut("AutoStart", appEntry->name, targetPath, appEntry->tip, NULL, NULL, NULL, 0, SW_SHOWMINNOACTIVE);
      else if (ShortcutExists("AutoStart", appEntry->name))
         DeleteShortcut("AutoStart", appEntry->name);
      if (appEntry->ql)
         CreateShortcut("QuickLaunch", appEntry->name, targetPath, appEntry->tip);
      else if (ShortcutExists("QuickLaunch", appEntry->name))
         DeleteShortcut("QuickLaunch", appEntry->name);

      message(NEWLINE);
   }
   IFileEnd();

   if (TextWindowCancel())
      message(NEWLINE"** Canceled **"NEWLINE);
   else if (gSettings.autoUpdate)
   {
      // Check for possible update of this program at the home site
      tFileName tmpFile;
      // Delete possible old version from previous update
      strcpy(tmpFile, gInstAppPath);
      strcat(tmpFile, OLD_EXT);
      if (DeleteFile(tmpFile))
         // New version installed last time, make sure that main menu entries are up to date
         StartMenuEntries();

      IFileBegin(HOME_SITE);
      tString latestVersion;
      if (IFileGetText(APP_VERSION_URL, latestVersion))
      {
         if (strcmp(GetRegVal(APP_VERSION_KEY), latestVersion) < 0)
         {
            // Update is available
            message(NEWLINE"Updating "APP_NAME" to version: %s"NEWLINE, latestVersion);
            strcpy(tmpFile, gInstAppPath);
            strcat(tmpFile, "_tmp");
            if (IFileCopy(APP_HOME_URL, tmpFile))
            {
               tFileName oldFile;
               strcpy(oldFile, gInstAppPath);
               strcat(oldFile, OLD_EXT);
               // Swap new and old files for later removal of the old version
               MoveFile(gInstAppPath, oldFile);
               MoveFile(tmpFile, gInstAppPath);
            }
            else
               ErrorMessage("Failed to retrieve new version from: %s", HOME_SITE);
            MakeVisible();
         }
      }
      IFileEnd();
   }

   if (showDialog)
   {
      // Handle login run shortcut
      if (gSettings.runAtLogin)
         CreateShortcut("AutoStart", RUN_AT_LOGON_SHORTCUT, gInstAppPath, RUN_AT_LOGON_SHORTCUT, "-update -silent");
      else if (ShortcutExists("AutoStart", RUN_AT_LOGON_SHORTCUT))
         DeleteShortcut("AutoStart", RUN_AT_LOGON_SHORTCUT);
   }

   // Set Windows unsintall registry keys
   SetRegVal(UNINSTALL_REGKEY"\\DisplayName", APP_NAME);
   SetRegVal(UNINSTALL_REGKEY"\\DisplayIcon", gInstAppPath);
   SetRegVal(UNINSTALL_REGKEY"\\Publisher", "Peter Lerup");
   SetRegVal(UNINSTALL_REGKEY"\\Comments", SHORT_DESCR);
   SetRegVal(UNINSTALL_REGKEY"\\DisplayVersion", APP_VERSION);
   SetRegVal(UNINSTALL_REGKEY"\\URLInfoAbout", "http://"HOME_SITE"/"APP_NAME);
   SetRegVal(UNINSTALL_REGKEY"\\InstallLocation", gInstDir);
   tFileName com;
   strcpy(com, gInstAppPath);
   strcat(com, " -uninstall");
   SetRegVal(UNINSTALL_REGKEY"\\UninstallString", com);

#ifndef _WIN64
   // The "Program Compatibility Assistant" is a real pain. Set the registry value it seems
   // to use when the "Installed correctly" option is selected, in order to avoid it
   if (IsVista())
   {
      HKEY  hKey;
      DWORD disp, val = 1;
      if (RegCreateKeyEx(HKEY_CURRENT_USER, COMPASS_REGKEY, 0, "", REG_OPTION_NON_VOLATILE,
             KEY_ALL_ACCESS, NULL, &hKey, &disp) == ERROR_SUCCESS)
      {
         RegSetValueEx(hKey, gInstAppPath, 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
         RegCloseKey(hKey);
      }
   }
#endif

   return TRUE;
}

//--------------------------------------------------------------------------

BOOL WINAPI UnInstallDlgProc(HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
   switch (message)
   {
      case WM_INITDIALOG:
         CenterDlg(hDlg);
			return TRUE;
      
      case WM_COMMAND:
         switch (wParam)
         {
            case IDOK:
               if (IsDlgButtonChecked(hDlg, IDC_CHECK1))
                  // Delete all Sysinternals registry settings
                  RecDelRegKey("Software\\Sysinternals");
               EndDialog(hDlg, TRUE);
               return TRUE;
               break;

            case IDCANCEL:
               EndDialog(hDlg, FALSE);
               return TRUE;
               break;
               
            default:
               break;
         }
      break;
   }
   
   return FALSE;
}

//--------------------------------------------------------------------------

void UnInstall()
{
   // Remove everything
   if (DialogBox(gHInstance, MAKEINTRESOURCE(IDD_REMOVE), gTextWindowHandle, (DLGPROC)UnInstallDlgProc))
   {
      if (RecDelRegKey() != ERROR_SUCCESS)
         ErrorMessage("Failed to remove program registry settings");
      DelStartMenuEntries();

      tAppEntry *appEntry = gAppList;
      while (appEntry)
      {
         tFileName shortcut;
         if (appEntry->autoStart)
         {
            GetShortcutPath("AutoStart", appEntry->name, shortcut);
            DeleteFile(shortcut);
         }
         if (appEntry->ql)
         {
            GetShortcutPath("QuickLaunch", appEntry->name, shortcut);
            DeleteFile(shortcut);
         }
         appEntry = appEntry->next;
      }

      // Delete Windows uninstall keys
      RecDelRegKey(UNINSTALL_REGKEY);

      // Create a bat file (can delete itself) to remove the program directory tree
      tFileName batFileName;
      strcpy(batFileName, GetFileNameComp(gCfgFile, "drive,dir,name"));
      strcat(batFileName, ".bat");
      FILE *batFile = fopen(batFileName, "w");
      fprintf(batFile, "rmdir /s /q \"%s\"\n", gInstDir);
      fclose(batFile);
      GetShortPathName(batFileName, batFileName, sizeof(batFileName));
      // Start the bat file and exit
      _execl(getenv("comspec"), " /c", batFileName, NULL);
      ErrorMessage("Failed to remove program directory");
   }
   else
      DoExit(1);
}

//--------------------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
   gHInstance = hInstance;

   BOOL bUninstall = stristr(lpCmdLine, "-uninstall") != NULL,
        bUpdate    = stristr(lpCmdLine, "-update") != NULL;

   gSilent = bUpdate && stristr(lpCmdLine, "-silent") != NULL;

   if (!Init())
      return 1;

   ReadAppList(gCfgFile);

   if (bUninstall)
      UnInstall();
   else
      Update(!bUpdate);

   ConfirmExit();

   return 0;
}