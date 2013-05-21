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
#include <sys/time.h>
#include <sys/select.h>
#include <libudev.h>

struct udev *udev;
struct udev_monitor *udev_mon;

void
dvb_hotplug_udev_init()
{
  udev = udev_new();
  if(!udev)
    return; // TODO: error message

  udev_mon = udev_monitor_new_from_netlink(udev, "udev");
  udev_monitor_enable_receiving(udev_mon);
}

void
dvb_hotplug_udev_destroy()
{
  if (udev)
  {
    udev_monitor_unref(udev_mon);
    udev_unref(udev);
  }
}

void
dvb_hotplug_udev_poll()
{
  if (!udev)
    return;

  struct udev_device *dev;

  int fd;
  fd_set fds;
  struct timeval tv;
  int retval;

  fd = udev_monitor_get_fd(udev_mon);

  while (1)
  {
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    retval = select(fd+1, &fds, NULL, NULL, &tv);

    if (retval > 0 && FD_ISSET(fd, &fds))
    {
      dev = udev_monitor_receive_device(udev_mon);
      if (dev)
      {
        printf("Got Device\n");
        printf("   Node: %s\n", udev_device_get_devnode(dev));
        printf("   Subsystem: %s\n", udev_device_get_subsystem(dev));
        printf("   Devtype: %s\n", udev_device_get_devtype(dev));

        printf("   Action: %s\n",udev_device_get_action(dev));
        udev_device_unref(dev);
      }
    }
    else
    {
      if (retval == -1)
        ; // TODO: error message?

      break;
    }
  }
}

int
main(int argc, char **argv)
{
  dvb_hotplug_udev_init();

  while (1)
    dvb_hotplug_udev_poll();

  dvb_hotplug_udev_destroy();
  return 0;
}
