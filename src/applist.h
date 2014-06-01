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


typedef struct tAppEntry {
	char *name;
	char *source;
	char *destination;
	char *menu;
   char *tip;
   BOOL autoStart, ql;
   BOOL _default;
   BOOL use;
   BOOL remove;
	struct tAppEntry	*next;
} tAppEntry;

extern tAppEntry *gAppList;


typedef struct {
   char *SIDownloadSite;
   char *SIHomeSite;
   BOOL autoUpdate;
   BOOL runAtLogin;
} tSettings;

extern tSettings gSettings;


BOOL ReadAppList(char *fileName);

