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

#define APP_NAME "SISetup"

typedef char tString[512];
typedef char tFileName[_MAX_PATH];

#define NEWLINE "\r\n"
#define APPEND_BS(str) if (str[strlen(str)-1] != '\\') strcat(str, "\\");
#define TRIM_BS(str) if (str[strlen(str)-1] == '\\') str[strlen(str)-1] = 0;

#define MY_TRACE(mess) MessageBox(gTextWindowHandle, mess, "Trace", MB_OK);

char *CopyString(char *source);
void ReplaceStr(char *src, char *match, char *repl);

BOOL FileExists(char *fileName);
BOOL WriteAllowed(char *path);
char *GetFileNameComp(char *fileName, char *type);
BOOL IsDir(char *name);
BOOL DelDir(char *dirName);
BOOL CreateDir(char *dirName);

BOOL String2File(char *fileName, char *str);

void CenterDlg(HWND hDlg);

void message(char *form, char *par = NULL);
BOOL ErrorMessage(char *form, char *par = NULL, BOOL fatal = FALSE, BOOL retry = FALSE);
void ErrorDialog(DWORD id);

char *PromptDirectory(HWND father, char *startDir, char *message);

BOOL ConfirmExit();
void DoExit(int exitCode = 0);

char *GetRegVal(char *valName, HKEY rootKey = NULL);
BOOL SetRegVal(char *valName, char *val, HKEY rootKey = NULL);
DWORD RecDelRegKey(char *pKeyName = NULL, HKEY hStartKey = NULL);
BOOL DelRegVal(char *valName, HKEY rootKey = NULL);
char *GetInstallKeyName(char *appName);
char *GetRemoveKeyName(char *appName);

BOOL GetCurrExePath(char *path);

BOOL GetProgFilesDir(char *path, char *subDir);

char *CreateURL(char *url, char *path, BOOL startMenu);

BOOL GetShortcutPath(char *type, char *name, char *path);
BOOL ShortcutExists(char *type, char *entry);
BOOL CreateShortcut(char *type,
                    char *entry, char *targetPath, char *comment = NULL, char *arguments = NULL,
                    char *workDir = NULL, char *iconFile = NULL, int iconIndex = 0, int showCommand = SW_SHOWNORMAL);
BOOL DeleteShortcut(char *type, char *entry);

BOOL DelStartMenuEntries(char *subDir = NULL);

HWND CreateTooltip(HWND win, char *text);

DWORD spawn(char *command, BOOL show);
DWORD RunElevated(char *com, char *params);

BOOL IsWin9x();
BOOL IsVista();

char* stristr(char* str, char* match);

