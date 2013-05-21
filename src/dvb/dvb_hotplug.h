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

#ifndef DVB_HOTPLUG_H_
#define DVB_HOTPLUG_H_

#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <unistd.h>

/**
 * Hotplug common functions
 */
void dvb_hotplug_init(void);
void dvb_hotplug_destroy(void);
void dvb_hotplug_poll(void);

/**
 * Hotplug subsystems call these to notify about device hotplug.
 * devicepath: NULL means the exact device path is unknown.
 */
void dvb_hotplug_device_connect(const char *devicepath);
void dvb_hotplug_device_disconnect(const char *devicepath);

/**
 * libudev
 */
void dvb_hotplug_udev_init(void);
void dvb_hotplug_udev_destroy(void);
void dvb_hotplug_udev_poll(void);

#endif

