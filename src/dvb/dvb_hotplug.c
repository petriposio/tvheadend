/*
 *  Hotplug device monitoring
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

#include "dvb_hotplug.h"
#include "tvheadend.h"

//#include "dvb.h" fails and didn't bother including "everything" so declaring these here
void dvb_adapter_device_connect(const char* devicepath);
void dvb_adapter_device_disconnect(const char* devicepath);

/**
 * Hotplug interface functions
 */
struct dvb_hotplug_interface {
    void (*hotplug_init)(void);
    void (*hotplug_destroy)(void);
    void (*hotplug_poll)(void);
};

static const struct dvb_hotplug_interface const
hotplug_systems[] = {
#if ENABLE_UDEV
  (struct dvb_hotplug_interface)
  {
    .hotplug_init = &dvb_hotplug_udev_init,
    .hotplug_destroy = &dvb_hotplug_udev_destroy,
    .hotplug_poll = &dvb_hotplug_udev_poll
  },
#endif
};

void
dvb_hotplug_init()
{
  const struct dvb_hotplug_interface *system = hotplug_systems;
  int i;
  for (i = 0; i < sizeof(hotplug_systems) / sizeof(struct dvb_hotplug_interface); i++)
  {
    system[i].hotplug_init();
  }
}

void
dvb_hotplug_destroy()
{
  const struct dvb_hotplug_interface *system = hotplug_systems;
  int i;
  for (i = 0; i < sizeof(hotplug_systems) / sizeof(struct dvb_hotplug_interface); i++)
  {
    system[i].hotplug_destroy();
  }
}

void
dvb_hotplug_poll(void)
{
  const struct dvb_hotplug_interface *system = hotplug_systems;
  int i;
  for (i = 0; i < sizeof(hotplug_systems) / sizeof(struct dvb_hotplug_interface); i++)
  {
    system[i].hotplug_poll();
  }
}

void
dvb_hotplug_device_connect(const char *devicepath)
{
  pthread_mutex_lock(&global_lock);
  dvb_adapter_device_connect(devicepath);
  pthread_mutex_unlock(&global_lock);
}

void
dvb_hotplug_device_disconnect(const char *devicepath)
{
  pthread_mutex_lock(&global_lock);
  dvb_adapter_device_disconnect(devicepath);
  pthread_mutex_unlock(&global_lock);
}

