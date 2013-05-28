/*
 *  udev hotplug monitoring
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
#include <string.h>
#include <regex.h>
#include <libudev.h>

#include "queue.h"
#include "dvb_hotplug.h"
#include "tvhlog.h"

static const char *device_subsystem_dvb = "dvb";
static const char *device_action_add = "add";
static const char *device_action_remove = "remove";

#define STRING_BUFFER_SIZE 200

static regex_t dvb_regex;
static const char *dvb_regex_string = "^/dev/dvb/(adapter[^/]*?)/([^/]*?)$";
static const char *dvb_path = "/dev/dvb";

#define SUBSYSTEM_COUNT 3
static regex_t dvb_required_subsystem_regex[SUBSYSTEM_COUNT];
static const char *dvb_required_subsystem_regex_string[] = {
  "frontend.*",
  "demux.*",
  "dvr.*"
};
struct dvb_subsystem_counter
{
  char adapter[STRING_BUFFER_SIZE];
  int connected[SUBSYSTEM_COUNT];
  LIST_ENTRY(dvb_subsystem_counter) subsystem_link;
};
static LIST_HEAD(dvb_subsystem_counter_list, dvb_subsystem_counter) subsystem_counter_head =
    LIST_HEAD_INITIALIZER(subsystem_counter_head);

static struct udev *udev;
static struct udev_monitor *udev_mon;

/**
 * Static helper function declarations
 */
static void str_substring(char *destination, int max_size, const char *source, int start, int end);

static void dvb_hotplug_udev_handle_device(struct udev_device *dev);
static int dvb_hotplug_udev_is_adapter_ready(const char *adapter, const char *adapter_subsystem);

static int get_required_subsystem_index(const char *subsystem);
static struct dvb_subsystem_counter* create_subsystem_counter(const char *adapter);
static struct dvb_subsystem_counter* find_subsystem_counter_by_adapter(const char *adapter);


/**
 * Init
 * Starts udev monitoring and compiles regexes.
 */
void
dvb_hotplug_udev_init()
{
  udev = udev_new();
  if(!udev)
    goto udev_init_failure;

  udev_mon = udev_monitor_new_from_netlink(udev, "udev");
  if (!udev_mon)
    goto udev_init_failure;

  if (udev_monitor_filter_add_match_subsystem_devtype(udev_mon, device_subsystem_dvb, NULL) != 0)
    goto udev_init_failure;

  if (udev_monitor_enable_receiving(udev_mon) != 0)
    goto udev_init_failure;

  tvhlog(LOG_INFO, "udev", "Hotplug monitoring started");

  if (regcomp(&dvb_regex, dvb_regex_string, REG_EXTENDED))
    goto udev_init_failure;

  int i;
  for (i = 0; i < SUBSYSTEM_COUNT; i++)
  {
    if (regcomp(&dvb_required_subsystem_regex[i], dvb_required_subsystem_regex_string[i], REG_EXTENDED))
      goto udev_init_failure;
  }
  return;

udev_init_failure: ;
  tvhlog(LOG_ERR, "udev", "Failed to initialise hotplug monitoring");
  if (udev)
  {
    udev_unref(udev);
    udev = NULL;
  }
  if (udev_mon)
  {
    udev_monitor_unref(udev_mon);
    udev_mon = NULL;
  }
}

/**
 * Destroy
 */
void
dvb_hotplug_udev_destroy()
{
  if (udev_mon)
    udev_monitor_unref(udev_mon);

  if (udev)
    udev_unref(udev);

  regfree(&dvb_regex);
  int i;
  for (i = 0; i < SUBSYSTEM_COUNT; i++)
    regfree(&dvb_required_subsystem_regex[i]);

  struct dvb_subsystem_counter *temp;
  while (!LIST_EMPTY(&subsystem_counter_head))
  {
    temp = LIST_FIRST(&subsystem_counter_head);
    LIST_REMOVE(temp, subsystem_link);
    free(temp);
  }
}

/**
 * Poll
 */
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
      else
        ; // TODO: udev monitor device read error message?
    }
    else
    {
      if (retval < 0)
        ; // TODO: select read error message?

      break;
    }
  }
}

/**
 * Does the actual dvb device checking and creates hotplug events.
 *
 * On an add action:
 *  - Mark only this subsystem ready.
 *  - Sends one hotplug even when all the required subsystems are ready.
 *  - Fails on a race condition if hotplug monitoring and device are connected at the same time
 *
 * On a remove action:
 *  - Inform hotplug event handler as soon as possible.
 *  - Causes event spam because of multiple subsystems.
 */
static void
dvb_hotplug_udev_handle_device(struct udev_device *dev)
{
  const char *path, *action;
  char adapterpath[STRING_BUFFER_SIZE], adapter[STRING_BUFFER_SIZE], adapter_subsystem[STRING_BUFFER_SIZE];

  path = udev_device_get_devnode(dev);
  action = udev_device_get_action(dev);

  tvhlog(LOG_DEBUG, "udev", "dvb device event %s (%s)", path, action);

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

/**
 * Marks this subsystem ready and returns whether every required subsystem
 * is ready or not.
 *
 * adapter: name of the connected adapter [eg. adapter1]
 * adapter_subsystem: subsystem that generated the notification [eg. demux0]
 *
 * Returns 1 if all of the required subsystems of the adapter are connected.
 * Returns 0 otherwise.
 */
static int
dvb_hotplug_udev_is_adapter_ready(const char *adapter, const char *adapter_subsystem)
{
  // Check whether this subsystem is required or not.
  int subsystem_index = get_required_subsystem_index(adapter_subsystem);
  if (subsystem_index < 0)
    return 0;

  // Search subsystem list if this adapter exists.
  struct dvb_subsystem_counter *adapter_counter = find_subsystem_counter_by_adapter(adapter);
  if (!adapter_counter)
  {
    // Subsystem list didn't contain the adapter so create a new counter and add it to the list.
    adapter_counter = create_subsystem_counter(adapter);
    LIST_INSERT_HEAD(&subsystem_counter_head, adapter_counter, subsystem_link);
  }

  // Mark the subsystem ready.
  adapter_counter->connected[subsystem_index] = 1;

  // Check if every important subsystem is ready.
  int i;
  for (i = 0; i < SUBSYSTEM_COUNT; i++)
  {
    if (!adapter_counter->connected[i])
      return 0;
  }

  // Adapter is ready. Remove it from the counter list.
  LIST_REMOVE(adapter_counter, subsystem_link);
  free(adapter_counter);

  return 1;
}

static void
str_substring(char *destination, int max_size, const char *source, int start, int end)
{
  snprintf(destination, max_size, "%.*s", end - start, source + start);
}

/**
 * If the subsystem is required [eg. frontend3], returns it's subsystem index.
 * Returns -1 otherwise.
 */
static int
get_required_subsystem_index(const char *subsystem)
{
  regmatch_t match[1];

  int i;
  for (i = 0; i < SUBSYSTEM_COUNT; i++)
  {
    if (regexec(&dvb_required_subsystem_regex[i], subsystem, 1, match, 0) == 0)
      return i;
  }
  return -1;
}

static struct dvb_subsystem_counter*
create_subsystem_counter(const char *adapter)
{
  struct dvb_subsystem_counter *counter = calloc(1, sizeof(struct dvb_subsystem_counter));

  strncpy(counter->adapter, adapter, STRING_BUFFER_SIZE - 1);
  /*adapter_counter->adapter[STRING_BUFFER_SIZE - 1] = '\0';

  int i;
  for (i = 0; i < SUBSYSTEM_COUNT; i++)
    adapter_counter->connected[i] = 0;
  */
  return counter;
}

static struct dvb_subsystem_counter*
find_subsystem_counter_by_adapter(const char *adapter)
{
  struct dvb_subsystem_counter *counter;

  LIST_FOREACH(counter, &subsystem_counter_head, subsystem_link)
  {
    if (strcmp(adapter, counter->adapter) == 0)
      return counter;
  }

  return NULL;
}

