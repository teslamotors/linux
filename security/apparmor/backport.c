/*
 * AppArmor security module
 *
 * This file contains AppArmor file mediation function definitions.
 *
 * Copyright 2014 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 *
 * This is a file of helper fns backported from newer kernels to support
 * backporting of apparmor to older kernels. Fns prefixed with code they
 * are copied of modified from
 */

#include "include/backport.h"
