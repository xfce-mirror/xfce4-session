/*-
 * Copyright (c) 2018      Ali Abdallah <ali@xfce.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */
#ifndef __XFSM_SHUTDOWN_COMMON_H_
#define __XFSM_SHUTDOWN_COMMON_H_

#ifdef POWEROFF_CMD
#undef POWEROFF_CMD
#endif
#ifdef REBOOT_CMD
#undef REBOOT_CMD
#endif
#ifdef UP_BACKEND_SUSPEND_COMMAND
#undef UP_BACKEND_SUSPEND_COMMAND
#endif
#ifdef UP_BACKEND_HIBERNATE_COMMAND
#undef UP_BACKEND_HIBERNATE_COMMAND
#endif

/* On FreeBSD, NetBSD and DragonFly BSD users with write access to
 * /dev/acpi can suspend/hibernate the system */
#define BSD_SLEEP_ACCESS_NODE "/dev/acpi"

/* On OpenBSD this is done via apmd, so we check /var/run/apmdev to see
 * zzz and ZZZ commands can write to it*/
#ifdef BACKEND_TYPE_OPENBSD
#undef BSD_SLEEP_ACCESS_NODE
#define BSD_SLEEP_ACCESS_NODE "/var/run/apmdev"
#endif

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define POWEROFF_CMD "/sbin/shutdown -p now"
#define REBOOT_CMD "/sbin/shutdown -r now"
#elif defined(sun) || defined(__sun)
#define POWEROFF_CMD "/usr/sbin/shutdown -i 5 -g 0 -y"
#define REBOOT_CMD "/usr/sbin/shutdown -i 6 -g 0 -y"
#else
#define POWEROFF_CMD "/sbin/shutdown -h now"
#define REBOOT_CMD "/sbin/shutdown -r now"
#endif

#ifdef BACKEND_TYPE_FREEBSD
#define UP_BACKEND_SUSPEND_COMMAND "/usr/sbin/acpiconf -s 3"
#define UP_BACKEND_HIBERNATE_COMMAND "/usr/sbin/acpiconf -s 4"
#endif

#ifdef BACKEND_TYPE_LINUX
#define UP_BACKEND_SUSPEND_COMMAND "/usr/sbin/pm-suspend"
#define UP_BACKEND_HIBERNATE_COMMAND "/usr/sbin/pm-hibernate"
#endif

#ifdef BACKEND_TYPE_OPENBSD
#define UP_BACKEND_SUSPEND_COMMAND "/usr/sbin/zzz"
#define UP_BACKEND_HIBERNATE_COMMAND "/usr/sbin/ZZZ"
#endif

#ifdef BACKEND_TYPE_SOLARIS
#define UP_BACKEND_SUSPEND_COMMAND "/usr/bin/sys-suspend"
#define UP_BACKEND_HIBERNATE_COMMAND "/usr/bin/false"
#endif

#ifndef UP_BACKEND_SUSPEND_COMMAND
#define UP_BACKEND_SUSPEND_COMMAND "/usr/bin/false"
#endif

#ifndef UP_BACKEND_HIBERNATE_COMMAND
#define UP_BACKEND_HIBERNATE_COMMAND "/usr/bin/false"
#endif

#endif /* __XFSM_SHUTDOWN_COMMON_H_ */
