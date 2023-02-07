/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <sys/stat.h>
#include <sys/syslog.h>

#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

#include <string>

#include <boost/filesystem.hpp>

#include "utils/utils.h"
#include "utils/log.h"

static char *civ_config_path = NULL;

const char *GetConfigPath(void) {
    if (civ_config_path) {
        return civ_config_path;
    }

    int ret = 0;
    char *sudo_uid = NULL;
    char *sudo_gid = NULL;
    uid_t ruid = -1, euid = -1, suid = -1;
    gid_t rgid = -1, egid = -1, sgid = -1;

    if (getresuid(&ruid, &euid, &suid) != 0)
        return NULL;
    if (getresgid(&rgid, &egid, &sgid) != 0)
        return NULL;
    suid = geteuid();
    sgid = getegid();

    sudo_uid = std::getenv("SUDO_UID");
    sudo_gid = std::getenv("SUDO_GID");

    if (sudo_gid) {
        egid = atoi(sudo_gid);
        setresgid(rgid, egid, sgid);
    }

    if (sudo_uid) {
        euid = atoi(sudo_gid);
        setresuid(ruid, euid, suid);
    }

    civ_config_path = new char[MAX_PATH];
    memset(civ_config_path, 0, MAX_PATH);

    struct passwd pwd;
    struct passwd *ppwd = &pwd;
    struct passwd *presult = NULL;
    char buffer[10240];
    ret = getpwuid_r(euid, ppwd, buffer, sizeof(buffer), &presult);
    if (ret != 0 || presult == NULL)
        return NULL;

    snprintf(civ_config_path, MAX_PATH, "%s%s", pwd.pw_dir, "/.intel/.civ");
    if (!boost::filesystem::exists(civ_config_path)) {
        if (!boost::filesystem::create_directories(civ_config_path)) {
            delete[] civ_config_path;
            civ_config_path = NULL;
        }
    }

    if (sudo_gid)
        setresgid(rgid, sgid, egid);

    if (sudo_uid)
        setresuid(ruid, suid, euid);

    return civ_config_path;
}

int Daemonize(void) {
    if (pid_t pid = fork()) {
        if (pid > 0) {
            exit(0);
        } else {
            LOG(error) << "First fork failed %m!";
            return -1;
        }
    }

    setsid();
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    chdir("/");

    if (pid_t pid = fork()) {
        if (pid > 0) {
            return pid;
        } else {
            LOG(error) << "Second fork failed!";
            exit(-1);
        }
    }

    umask(0);
    for (int t = sysconf(_SC_OPEN_MAX); t >= 0; t--) {
        close(t);
    }

    /* Close the standard streams. */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int fd = open("/dev/null", O_RDWR);
    if (fd == -1)
        return -1;

    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);

    close(fd);

    return 0;
}

