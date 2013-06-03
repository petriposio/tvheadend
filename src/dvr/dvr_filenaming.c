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
#include "dvr.h"
#include "string_map.h"
#include "queue.h"
#include "pcre.h"

#define STRING_SIZE 200
#define MAX_REGEX_MATCHES 40

#define CHANNEL_KEY     "channel"
#define CREATOR_KEY     "creator"
#define TITLE_KEY       "title"
#define DESCRIPTION_KEY "description"

/**
 * Declarations
 */
static void dvr_filenaming_create_common_variables(string_map *variables, dvr_entry_t *de);
static void dvr_filenaming_create_user_variables(string_map *variables, dvr_filename_scheme_advanced_t *scheme);

static void dvr_filenaming_capturing_regex(string_map *variables, const char *source, const char *regex);
static void dvr_filenaming_fprint(char *destination, size_t max_size, const char *source, const string_map *variables);

/**
 * Definitions
 */
int dvr_filenaming_set_mode(dvr_config_t *cfg, int mode)
{
  if (cfg->dvr_filename_mode == mode)
    return 0;

  cfg->dvr_filename_mode = mode;
  return 1;
}

static int change_str(char **target, const char *new)
{
  if (*target == new || (*target && new && strcmp(*target, new) == 0))
    return 0;

  if (new)
  {
    free (*target);
    *target = NULL;
  }
  else
  {
    int old_length = *target ? strlen(*target) : 0;
    int new_length = new ? strlen(new) : 0;

    if (new_length == 0)
    {
      if (*target)
        free (*target);
      *target = NULL;
    }
    else
    {
      if (old_length < new_length)
      {
        if (*target)
          free (*target);
        *target = calloc(1, new_length * sizeof (char));
      }
      strcpy(*target, new);
    }
  }

  return 1;
}

int dvr_filenaming_advanced_set_filename_format(dvr_filename_scheme_advanced_t *scheme, const char *format)
{
  return change_str(&scheme->filename_format, format);
}

int dvr_filenaming_advanced_set_regex(dvr_filename_scheme_advanced_t *scheme, int index, const char *source, const char *regex)
{
  index++;
  struct dvr_filename_scheme_advanced_list *i;
  LIST_FOREACH (i, &scheme->regex_list, _link)
  {
    index--;
    if (index == 0)
      break;
  }

  if (index != 0)
  {
    struct dvr_filename_scheme_advanced_list *temp = calloc(1, sizeof(struct dvr_filename_scheme_advanced_list));
    if (i)
      LIST_INSERT_AFTER(i, temp, _link);
    else
      LIST_INSERT_HEAD(&scheme->regex_list, temp, _link);

    i = temp;
  }

  int changed = 0;
  if (change_str(&i->source, source))
    changed = 1;
  if (change_str(&i->regex, regex))
    changed = 1;

  return changed;
}

void dvr_filenaming_advanced_init(dvr_filename_scheme_advanced_t *scheme)
{
  scheme->filename_format = NULL;
  LIST_INIT(&scheme->regex_list);
}

void dvr_filenaming_advanced_destroy(dvr_filename_scheme_advanced_t *scheme)
{
  struct dvr_filename_scheme_advanced_list *i;
  while (!LIST_EMPTY(&scheme->regex_list))
  {
    i = LIST_FIRST(&scheme->regex_list);
    LIST_REMOVE(i, _link);

    if (i->source)
      free(i->source);
    if (i->regex)
      free(i->regex);
    free(i);
  }
}

int dvr_filenaming_get_filename(char *destination, size_t max_size, dvr_filename_scheme_advanced_t *scheme, dvr_entry_t *de)
{
  string_map variables;
  string_map_init(&variables);

  dvr_filenaming_create_common_variables(&variables, de);
  dvr_filenaming_create_user_variables(&variables, scheme);

  dvr_filenaming_fprint(destination, max_size, scheme->filename_format, &variables);

  string_map_destroy(&variables);

  return 1;
}

static void dvr_filenaming_create_common_variables(string_map *variables, dvr_entry_t *de)
{
  string_map_set(variables, CHANNEL_KEY, DVR_CH_NAME(de));
  string_map_set(variables, CREATOR_KEY, de->de_creator);
  //string_map_set(variables, TITLE_KEY, lang_str_get(de->de_title, NULL));
  //string_map_set(variables, DESCRIPTION_KEY, lang_str_get(de->de_desc, NULL));
}

static void dvr_filenaming_create_user_variables(string_map *variables, dvr_filename_scheme_advanced_t *scheme)
{
  char source[STRING_SIZE];

  struct dvr_filename_scheme_advanced_list *i;
  LIST_FOREACH (i, &scheme->regex_list, _link)
  {
    dvr_filenaming_fprint(source, STRING_SIZE, i->source, variables);
    dvr_filenaming_capturing_regex(variables, source, i->regex);
  }
}

/**
 * Capturing regex
 */
static void dvr_filenaming_capturing_regex(string_map *variables, const char *source, const char *regex)
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

  if (match_count == 0) // more matches than the given array was able to hold
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

      string_map_set(variables, name, value);

      pcre_free_substring(value);
    }
  }

  pcre_free(reg);
}

/**
 * Formatted printing with variables from captured regexes
 */
static void dvr_filenaming_fprint(char *destination, size_t max_size, const char *source, const string_map *variables)
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

    const char *value = string_map_get(variables, temp_key);
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

/*int main(int argc, char **argv)
{
  string_map variables;
  string_map_init(&variables);

  char temp[STRING_SIZE];

  dvr_filenaming_fprint(temp, STRING_SIZE, "S02E08 - testi (k18)", &variables);
  dvr_filenaming_capturing_regex(&variables, temp, "^(S(?<season>\\d*))?(E(?<episode>\\d*))?(?<name>.*?)(\\(k(?<classification>.*?)\\))?$");

  dvr_filenaming_fprint(temp, STRING_SIZE, "%name%", &variables);
  dvr_filenaming_capturing_regex(&variables, temp, "^[ -]*(?<name>.*?)[ ]*$");

  dvr_filenaming_fprint(temp, STRING_SIZE, "nimi: %season% %episode% %name% %classification%", &variables);
  printf("%s\n", temp);

  string_map_destroy(&variables);

  return 0;
}*/

