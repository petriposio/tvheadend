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

#include <string.h>

#include "dvr_filenaming.h"
#include "redblack.h"
#include "pcre.h"

#define STRING_SIZE 200
#define MAX_REGEX_MATCHES 40

#define CHANNEL_KEY     "channel"
#define CREATOR_KEY     "creator"
#define TITLE_KEY       "title"
#define DESCRIPTION_KEY "description"

typedef struct key_value_container {
  char key[STRING_SIZE];
  char value[STRING_SIZE];

  RB_ENTRY(key_value_container) rb_link;
} key_value_container_t;

typedef RB_HEAD(key_value_rb, key_value_container) key_value_rb_t;

static int key_cmp(void *a, void *b)
{
  return strcmp(((key_value_container_t*)a)->key, ((key_value_container_t*)b)->key);
}

/**
 * Declarations
 */
static void dvr_filenaming_create_common_variables(key_value_rb_t *variables, dvr_entry_t *de);
static void dvr_filenaming_create_user_variables(key_value_rb_t *variables, dvr_filename_scheme_advanced_t *scheme);

static void dvr_filenaming_set_value(key_value_rb_t *variables, const char *key, const char *value);
static const char* dvr_filenaming_get_value(const key_value_rb_t *variables, const char *key);

static void dvr_filenaming_capturing_regex(key_value_rb_t *variables, const char *source, const char *regex);
static void dvr_filenaming_fprint(char *destination, size_t max_size, const char *source, const key_value_rb_t *variables);

/**
 * Definitions
 */
int dvr_filenaming_get_filename(char *destination, size_t max_size, dvr_filename_scheme_advanced_t *scheme, dvr_entry_t *de)
{
  key_value_rb_t variables;
  RB_INIT(&variables);

  dvr_filenaming_create_common_variables(&variables, de);
  dvr_filenaming_create_user_variables(&variables, scheme);

  dvr_filenaming_fprint(destination, max_size, scheme->filename_regex, &variables);
}

static void dvr_filenaming_create_common_variables(key_value_rb_t *variables, dvr_entry_t *de)
{
  dvr_filenaming_set_value(variables, CHANNEL_KEY, DVR_CH_NAME(de));
  dvr_filenaming_set_value(variables, CREATOR_KEY, de->de_creator);
  //dvr_filenaming_set_value(variables, TITLE_KEY, lang_str_get(de->de_title, NULL));
  //dvr_filenaming_set_value(variables, DESCRIPTION_KEY, lang_str_get(de->de_desc, NULL));
}

static void dvr_filenaming_create_user_variables(key_value_rb_t *variables, dvr_filename_scheme_advanced_t *scheme)
{
  char source[STRING_SIZE];
  int i;
  for (i = 0; i < scheme->size; i++)
  {
    dvr_filenaming_fprint(source, STRING_SIZE, scheme->source[i], variables);
    dvr_filenaming_capturing_regex(variables, source, scheme->regex[i]);
  }
}

/**
 * Capturing regex
 */
static void dvr_filenaming_capturing_regex(key_value_rb_t *variables, const char *source, const char *regex)
{
  const char *error;
  int error_offset;

  pcre *reg = pcre_compile(regex, PCRE_EXTENDED, &error, &error_offset, NULL);
  if (!reg)
  {
    printf("pcre_compile failed (offset: %d), %s\n", error_offset, error);
    return;
  }

  int matches[MAX_REGEX_MATCHES * 2];
  int match_count = pcre_exec(reg, 0, source, strlen(source), 0, 0, matches, MAX_REGEX_MATCHES);
  if (match_count < 0)
    return;

  if (match_count = 0) // more matches than the given array was able to hold
    match_count = MAX_REGEX_MATCHES;

  int name_count, name_entry_size;
  uschar *nametable;

  if (pcre_get_nametable(reg, &nametable, &name_count, &name_entry_size) == 0)
  {
    char *name;
    const char *value;
    int substring_index;

    int i;
    for (i = 0; i < name_count; i++)
    {
      pcre_nametable_get_item(nametable, name_entry_size, i, &name, &substring_index);
      pcre_get_substring(source, matches, MAX_REGEX_MATCHES, substring_index, &value);

      dvr_filenaming_set_value(variables, name, value);

      pcre_free_substring(value);
    }
  }

  pcre_free(reg);
}

/**
 * Formatted printing with variables from captured regexes
 */
static void dvr_filenaming_fprint(char *destination, size_t max_size, const char *source, const key_value_rb_t *variables)
{
  char temp_key[STRING_SIZE];

  const char special[] = "%";

  size_t next_special;
  size_t copy_size;
  while (1)
  {
    // Part before special character, copy it all
    next_special = strcspn(source, special);
    copy_size = next_special < max_size ? next_special : max_size - 1;

    strncpy(destination, source, copy_size);

    destination += copy_size;
    max_size -= copy_size;
    source += next_special;

    if (*source == '\0' || max_size <= 0)
    {
      *destination = '\0';
      break;
    }
    source++;

    // Get the key name between special characters
    next_special = strcspn(source, special);

    if (next_special == 0)
    {
      *destination = *source;
      if (*source == '\0')
        break;
      source++;
      destination++;
      max_size--;
      continue;
    }

    copy_size = next_special < STRING_SIZE ? next_special : STRING_SIZE - 1;
    strncpy(temp_key, source, copy_size);
    temp_key[copy_size] = '\0';
    source += next_special;

    const char *value = dvr_filenaming_get_value(variables, temp_key);
    if (value)
    {
      copy_size = strlen(value);
      copy_size = copy_size < max_size ? copy_size : max_size - 1;

      strncpy(destination, value, copy_size);

      destination += copy_size;
      max_size -= copy_size;
    }

    if (*source == '\0' || max_size <= 0)
    {
      *destination = '\0';
      break;
    }
    source++;
  }
}

/**
 * Key-value redblack tree helper functions
 */
// TODO: better way than this? create new, check if it collides and free new item and modify the colliding old element
static void dvr_filenaming_set_value(key_value_rb_t *variables, const char *key, const char *value)
{
  key_value_container_t *item = calloc(1, sizeof(key_value_container_t));
  strncpy(item->key, key, STRING_SIZE - 1);
  strncpy(item->value, value, STRING_SIZE - 1);

  key_value_container_t *old = RB_INSERT_SORTED(variables, item, rb_link, key_cmp);
  if(old)
  {
    free(item);
    strncpy(old->value, value, STRING_SIZE - 1);
  }
}

static const char* dvr_filenaming_get_value(const key_value_rb_t *variables, const char *key)
{
  key_value_container_t i;
  strncpy(i.key, key, STRING_SIZE - 1);
  i.key[STRING_SIZE - 1] = '\0';

  key_value_container_t *item = RB_FIND(variables, &i, rb_link, key_cmp);
  return item ? item->value : NULL;
}

int main(int argc, char **argv)
{
  key_value_rb_t variables;
  RB_INIT(&variables);

  char temp[STRING_SIZE];

  dvr_filenaming_fprint(temp, STRING_SIZE, "S02E08 - testi (k18)", &variables);
  dvr_filenaming_capturing_regex(&variables, temp, "^(S(?<season>\\d*))?(E(?<episode>\\d*))?(?<name>.*?)(\\(k(?<classification>.*?)\\))?$");

  dvr_filenaming_fprint(temp, STRING_SIZE, "%name%", &variables);
  dvr_filenaming_capturing_regex(&variables, temp, "^[ -]*(?<name>.*?)[ ]*$");

  dvr_filenaming_fprint(temp, STRING_SIZE, "nimi: %season% %episode% %name% %classification%", &variables);

  printf("%s\n", temp);

  return 0;
}

