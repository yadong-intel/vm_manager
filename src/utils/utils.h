/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __UTILS_H__
#define __UTILS_H__

#ifndef MAX_PATH
#define MAX_PATH 2048U
#endif

#define CIV_GUEST_QMP_SUFFIX     ".qmp.unix.socket"

const char *GetConfigPath(void);
int Daemonize(void);

#endif  //__UTILS_H__
