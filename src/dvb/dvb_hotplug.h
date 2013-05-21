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

#include <libudev.h>

#include "tvhlog.h"

/**
 * Hotplug interface functions
 */
struct dvb_hotplug_interface {
    void (*hotplug_init)(void);
    void (*hotplug_destroy)(void);
    void (*hotplug_poll)(void);
};

/**
 * Hotplug common functions
 */
void dvb_hotplug_init(void);
void dvb_hotplug_destroy(void);
void dvb_hotplug_poll(void);

/**
 * Hotplug system event handlers. 
 * devicepath can be NULL if it's unknown.
 */
void dvb_hotplug_device_connect(const char *devicepath);
void dvb_hotplug_device_disconnect(const char *devicepath);

/**
 * libudev
 */

#endif

