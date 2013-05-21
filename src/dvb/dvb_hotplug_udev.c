/*
 *  TV Input - Linux DVB interface
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

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <sys/time.h>
#include <sys/select.h>
#include <libudev.h>

#include "dvb_hotplug.h"

static const char *device_subsystem_dvb = "dvb";
static const char *device_action_add = "add";
static const char *device_action_remove = "remove";

#define STRING_BUFFER_SIZE 200

static regex_t dvb_regex;
static const char *dvb_regex_string = "^/dev/dvb/(adapter[^/]*?)/([^/]*?)$";
static const char *dvb_path = "/dev/dvb";

#define SUBSYSTEM_COUNT 3
static regex_t dvb_required_subsystem_regex[SUBSYSTEM_COUNT];
static const char *dvb_required_subsystems[] =
{
  "frontend.*",
  "demux.*",
  "dvr.*"
};
struct dvb_subsystem_counter
{
  struct dvb_subsystem_counter *next;
  char adapter[STRING_BUFFER_SIZE];
  int connected[SUBSYSTEM_COUNT];
};
static struct dvb_subsystem_counter *subsystem_counter_list = NULL;

static struct udev *udev;
static struct udev_monitor *udev_mon;

static int
str_prefix(const char *string, const char *prefix)
{
  return strncmp(string, prefix, strlen(prefix)) == 0;
}

static void
str_substring(char *destination, int max_size, const char *source, int start, int end)
{
  snprintf(destination, max_size, "%.*s", end - start, source + start);
}

/**
 * adapter: name of the connected adapter [eg. adapter0]
 * adapter_subsystem: subsystem that generated the notification [eg. demux0]
 *
 * Returns 1 if all of the important subsystems of the adapter are connected.
 * Returns 0 otherwise.
 */
static int
dvb_hotplug_udev_is_adapter_ready(const char *adapter, const char *adapter_subsystem)
{
  regmatch_t match[1];

  // Check whether this subsystem is important or not.
  int subsystem_index;
  for (subsystem_index = 0; subsystem_index < SUBSYSTEM_COUNT; subsystem_index++)
  {
    if (regexec(&dvb_required_subsystem_regex[subsystem_index], adapter_subsystem, 1, match, 0) == 0)
      goto important_subsystem;
  }
  return 0;

important_subsystem: ;
  // Search subsystem list if this adapter exists.
  struct dvb_subsystem_counter *adapter_counter = subsystem_counter_list;
  while (adapter_counter)
  {
    if (strcmp(adapter, adapter_counter->adapter) == 0)
      break;

    adapter_counter = adapter_counter->next;
  }

  if (!adapter_counter)
  {
    // Subsystem list didn't contain the adapter so create a new counter and add it to the list.
    adapter_counter = malloc(sizeof(struct dvb_subsystem_counter));
    if (!adapter_counter)
      ; // TODO: malloc error

    strncpy(adapter_counter->adapter, adapter, STRING_BUFFER_SIZE - 1);
    adapter_counter->adapter[STRING_BUFFER_SIZE - 1] = '\0';

    int i;
    for (i = 0; i < SUBSYSTEM_COUNT; i++)
      adapter_counter->connected[i] = 0;

    adapter_counter->next = subsystem_counter_list;
    subsystem_counter_list = adapter_counter;
  }

  adapter_counter->connected[subsystem_index] = 1;

  // Check if every important subsystem is ready.
  int i;
  for (i = 0; i < SUBSYSTEM_COUNT; i++)
  {
    if (!adapter_counter->connected[i])
      return 0;
  }

  // Adapter is ready. Remove it from the counter list.
  struct dvb_subsystem_counter **previous_counter = &subsystem_counter_list;
  while (*previous_counter != adapter_counter)
    previous_counter = &(*previous_counter)->next;

  *previous_counter = adapter_counter->next;

  free(adapter_counter);
  return 1;
}

static void
dvb_hotplug_udev_handle_device(struct udev_device *dev)
{
  const char *path, *action;
  char adapterpath[STRING_BUFFER_SIZE], adapter[STRING_BUFFER_SIZE], adapter_subsystem[STRING_BUFFER_SIZE];

  path = udev_device_get_devnode(dev);
  action = udev_device_get_action(dev);

  if (path)
  {
    regmatch_t matches[3];
    if (regexec(&dvb_regex, path, 3, matches, 0) == 0)
    {
      if (matches[1].rm_so >= 0 && matches[2].rm_so >= 0)
      {
        str_substring(adapter, STRING_BUFFER_SIZE, path, matches[1].rm_so, matches[1].rm_eo);
        str_substring(adapter_subsystem, STRING_BUFFER_SIZE, path, matches[2].rm_so, matches[2].rm_eo);

        snprintf(adapterpath, STRING_BUFFER_SIZE, "%s/%s", dvb_path, adapter);

        if (strcmp(device_action_remove, action) == 0)
        {
          dvb_hotplug_device_disconnect(adapterpath);
        }
        else if (strcmp(device_action_add, action) == 0)
        {
          if (dvb_hotplug_udev_is_adapter_ready(adapter, adapter_subsystem))
            dvb_hotplug_device_connect(adapterpath);
        }
      }
    }
  }
}

void
dvb_hotplug_udev_init()
{
  udev = udev_new();
  if(udev)
    udev_mon = udev_monitor_new_from_netlink(udev, "udev");

  if (!udev || !udev_mon)
  {
    // TODO: udev init error message
    if (udev)
    {
      udev_unref(udev);
      udev = NULL;
    }
    return;
  }

  udev_monitor_filter_add_match_subsystem_devtype(udev_mon, device_subsystem_dvb, NULL);
  udev_monitor_enable_receiving(udev_mon);

  if (regcomp(&dvb_regex, dvb_regex_string, REG_EXTENDED))
    ; // regex compiling error message? (shouldn't be needed)

  int i;
  for (i = 0; i < SUBSYSTEM_COUNT; i++)
  {
    if (regcomp(&dvb_required_subsystem_regex[i], dvb_required_subsystems[i], REG_EXTENDED))
      ; // regex compiling error message? (shouldn't be needed)
  }
}

void
dvb_hotplug_udev_destroy()
{
  if (udev_mon)
    udev_monitor_unref(udev_mon);

  if (udev)
    udev_unref(udev);

  regfree(&dvb_regex);
}

void
dvb_hotplug_udev_poll()
{
  struct udev_device *dev;

  int fd;
  fd_set fds;
  struct timeval tv;
  int retval;

  if (!udev)
    return;

  fd = udev_monitor_get_fd(udev_mon);

  while (1)
  {
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    retval = select(fd + 1, &fds, NULL, NULL, &tv);

    if (retval > 0 && FD_ISSET(fd, &fds))
    {
      dev = udev_monitor_receive_device(udev_mon);
      if (dev)
      {
        dvb_hotplug_udev_handle_device(dev);
        udev_device_unref(dev);
      }
    }
    else
    {
      if (retval == -1)
        ; // TODO: udev device read error message?

      break;
    }
  }
}

