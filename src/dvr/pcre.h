/*
 *  Additional PCRE helper functions
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

#include <pcre.h>

// PCRE internal character definition
typedef unsigned char uschar;

/**
 * A bit hacky and might break in future versions of PCRE.
 */
static int pcre_get_nametable(pcre *reg, uschar **nametable, int *name_count, int *name_entry_size)
{
  int rc;

  rc = pcre_fullinfo(reg, NULL, PCRE_INFO_NAMECOUNT, name_count);
  if (rc != 0)
    return rc;

  rc = pcre_fullinfo(reg, NULL, PCRE_INFO_NAMEENTRYSIZE, name_entry_size);
  if (rc != 0)
    return rc;

  rc = pcre_fullinfo(reg, NULL, PCRE_INFO_NAMETABLE, nametable);
  if (rc != 0)
    return rc;

  return 0;
}

/**
 * A bit hacky and might break in future versions of PCRE.
 */
static void pcre_nametable_get_item(uschar *nametable, int name_entry_size, int index, char **name, int *substring_index)
{
  uschar *entry = nametable + index * name_entry_size;

  *name = (char *)(entry + 2);
  *substring_index = (entry[0] << 8) + entry[1];
}
