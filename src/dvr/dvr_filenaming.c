/*
 *  Digital Video Recorder
 *  Copyright (C) 2013 Petri Posio
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dvr_filenaming.h"

#define CHANNEL_KEY     "channel"
#define CREATOR_KEY     "creator"
#define TITLE_KEY       "title"
#define DESCRIPTION_KEY "description"
#define STARTTIME_KEY   "start"
#define STOPTIME_KEY    "end"

struct value_container {

};

static const char* dvr_filenaming_get_value(const char *key);


