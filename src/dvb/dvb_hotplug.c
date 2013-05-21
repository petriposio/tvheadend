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

#include "dvb_hotplug.h"
#include "config.h"

static dvb_hotplug_interface
hotplug_systems[] = {
  NULL
};

void
dvb_hotplug_init()
{
  dvb_hotplug_interface *system = hotplug_systems;

  while (system != NULL)
  {
    system->hotplug_init();
    system++;
  }
}

void
dvb_hotplug_destroy()
{
  dvb_hotplug_interface *system = hotplug_systems;

  while (system != NULL)
  {
    system->hotplug_destroy();
    system++;
  }
}

void
dvb_hotplug_poll(void)
{
  dvb_hotplug_interface *system = hotplug_systems;

  while (system != NULL)
  {
    system->hotplug_poll();
    system++;
  }
}

void
dvb_hotplug_device_connect(const char *devicepath)
{

}

void
dvb_hotplug_device_disconnect(const char *devicepath)
{

}

