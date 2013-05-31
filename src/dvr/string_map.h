/*
 *  Red-black tree mapping key-value strings.
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

#ifndef STRING_MAP_H_
#define STRING_MAP_H_

#include "redblack.h"

// TODO: change to radix tree or something else that's more efficient with strings
typedef struct string_map_container {
  char *key;
  char *value;

  RB_ENTRY(string_map_container) rb_link;
} string_map_container_t;

typedef RB_HEAD(string_map_rb, string_map_container) string_map;

void string_map_init(string_map *map);
void string_map_destroy(string_map *map);

void string_map_set(string_map *map, const char *key, const char *value);
void string_map_remove(string_map *map, const char *key);
const char* string_map_get(const string_map *map, const char *key);

#endif
