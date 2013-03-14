/**
 * Sked: Task scheduling library for Arduino.
 *
 * Copyright (c) 2013, Christopher Myers.  All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

__extension__ typedef int __guard __attribute__((mode (__DI__))); // NOLINT

extern "C" int __cxa_guard_acquire(__guard *); // NOLINT
extern "C" void __cxa_guard_release (__guard *); // NOLINT
extern "C" void __cxa_guard_abort (__guard *); // NOLINT

int __cxa_guard_acquire(__guard *g) {return !*(char *)(g);}; //NOLINT
void __cxa_guard_release (__guard *g) {*(char *)g = 1;}; // NOLINT
void __cxa_guard_abort (__guard *) {}; // NOLINT

extern "C" void __cxa_pure_virtual(void); // NOLINT
void __cxa_pure_virtual(void) {}; // NOLINT

