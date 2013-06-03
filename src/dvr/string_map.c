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

//#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "string_map.h"

static int _key_cmp(void *a, void *b)
{
  return strcmp(((string_map_container_t*)a)->key, ((string_map_container_t*)b)->key);
}

/*static void _destroy_item(string_map_container_t *item)
{
  if(item->key)
    free(item->key);

  if(item->value)
    free(item->value);

  free(item);
}*/

void string_map_init(string_map *map)
{
  RB_INIT(map);
}

void string_map_destroy(string_map *map)
{
  // TODO: destroy
}

// TODO: better way than this?
void string_map_set(string_map *map, const char *key, const char *value)
{
  string_map_container_t *item = malloc(sizeof(string_map_container_t));
  item->key = strdup(key);
  item->value = strdup(value);

  string_map_container_t *old = RB_INSERT_SORTED(map, item, rb_link, _key_cmp);
  if(old)
  {
    if(old->key)
      free(old->key);
    if(old->value)
      free(old->value);

    old->key = item->key;
    old->value = item->value;

    free(item);
  }
}

void string_map_remove(string_map *map, const char *key)
{
  // TODO: remove
}

const char* string_map_get(const string_map *map, const char *key)
{
  string_map_container_t i;
  i.key = (char*) key;

  string_map_container_t *item = RB_FIND(map, &i, rb_link, _key_cmp);
  return item ? item->value : NULL;
}
