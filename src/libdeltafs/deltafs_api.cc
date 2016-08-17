/*
 * Copyright (c) 2014-2016 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#include "deltafs/deltafs_api.h"

#include <errno.h>
#include <fcntl.h>

#include "deltafs_client.h"
#include "pdlfs-common/dbfiles.h"
#include "pdlfs-common/logging.h"
#include "pdlfs-common/port.h"
#include "pdlfs-common/status.h"

#ifdef __cplusplus
extern "C" {
#endif

static pdlfs::port::OnceType once = PDLFS_ONCE_INIT;
// global Deltafs client instance shared by all threads within a process
static pdlfs::Client* client;

namespace pdlfs {
typedef Client::FileInfo FileInfo;
static void InitClient() {
  Status s = Client::Open(&client);
  if (!s.ok()) {
    Error(__LOG_ARGS__, "Fail to open client :%s", s.ToString().c_str());
    client = NULL;
  }
}
static void SetErrno(const Status& s) {
  if (s.IsNotFound()) {
    errno = ENOENT;
  } else if (s.IsAlreadyExists()) {
    errno = EEXIST;
  } else if (s.IsFileExpected()) {
    errno = EISDIR;
  } else if (s.IsDirExpected()) {
    errno = ENOTDIR;
  } else if (s.IsInvalidFileDescriptor()) {
    errno = EBADF;
  } else if (s.IsTooManyOpens()) {
    errno = EMFILE;
  } else if (s.IsAccessDenied()) {
    errno = EACCES;
  } else if (s.IsReadOnly()) {
    errno = EROFS;
  } else if (!s.ok()) {
    errno = EIO;  // TODO: map more error types
  }
}
}  // namespace pdlfs

int deltafs_open(const char* __path, int __oflags, mode_t __mode,
                 struct stat* __buf) {
  pdlfs::port::InitOnce(&once, pdlfs::InitClient);
  if (client == NULL) {
    errno = ENODEV;
    return -1;
  } else {
    pdlfs::FileInfo info;
    pdlfs::Status s;
    s = client->Fopen(__path, __oflags, __mode, &info);
    if (s.ok()) {
      __buf->st_mode = info.stat.FileMode();
      __buf->st_size = info.stat.FileSize();
      __buf->st_mtime = info.stat.ModifyTime() / 1000000;
      __buf->st_gid = info.stat.GroupId();
      __buf->st_uid = info.stat.UserId();
      return info.fd;
    } else {
      pdlfs::SetErrno(s);
      return -1;
    }
  }
}

ssize_t deltafs_pread(int __fd, void* __buf, size_t __sz, off_t __off) {
  pdlfs::port::InitOnce(&once, pdlfs::InitClient);
  if (client == NULL) {
    errno = ENODEV;
    return -1;
  } else {
    pdlfs::Slice result;
    pdlfs::Status s =
        client->Pread(__fd, &result, __off, __sz, static_cast<char*>(__buf));
    if (s.ok()) {
      return result.size();
    } else {
      pdlfs::SetErrno(s);
      return -1;
    }
  }
}

ssize_t deltafs_read(int __fd, void* __buf, size_t __sz) {
  pdlfs::port::InitOnce(&once, pdlfs::InitClient);
  if (client == NULL) {
    errno = ENODEV;
    return -1;
  } else {
    pdlfs::Slice result;
    pdlfs::Status s =
        client->Read(__fd, &result, __sz, static_cast<char*>(__buf));
    if (s.ok()) {
      return result.size();
    } else {
      pdlfs::SetErrno(s);
      return -1;
    }
  }
}

ssize_t deltafs_pwrite(int __fd, const void* __buf, size_t __sz, off_t __off) {
  pdlfs::port::InitOnce(&once, pdlfs::InitClient);
  if (client == NULL) {
    errno = ENODEV;
    return -1;
  } else {
    pdlfs::Status s = client->Pwrite(
        __fd, pdlfs::Slice(static_cast<const char*>(__buf), __sz), __off);
    if (s.ok()) {
      return __sz;
    } else {
      pdlfs::SetErrno(s);
      return -1;
    }
  }
}

ssize_t deltafs_write(int __fd, const void* __buf, size_t __sz) {
  pdlfs::port::InitOnce(&once, pdlfs::InitClient);
  if (client == NULL) {
    errno = ENODEV;
    return -1;
  } else {
    pdlfs::Status s = client->Write(
        __fd, pdlfs::Slice(static_cast<const char*>(__buf), __sz));
    if (s.ok()) {
      return __sz;
    } else {
      pdlfs::SetErrno(s);
      return -1;
    }
  }
}

int deltafs_fdatasync(int __fd) {
  pdlfs::port::InitOnce(&once, pdlfs::InitClient);
  if (client == NULL) {
    errno = ENODEV;
    return -1;
  } else {
    pdlfs::Status s = client->Fdatasync(__fd);
    if (s.ok()) {
      return 0;
    } else {
      pdlfs::SetErrno(s);
      return -1;
    }
  }
}

int deltafs_close(int __fd) {
  pdlfs::port::InitOnce(&once, pdlfs::InitClient);
  if (client == NULL) {
    errno = ENODEV;
    return -1;
  } else {
    client->Close(__fd);
    return 0;
  }
}

int deltafs_stat(const char* __path, struct stat* __buf) {
  pdlfs::port::InitOnce(&once, pdlfs::InitClient);
  if (client == NULL) {
    errno = ENODEV;
    return -1;
  } else {
    pdlfs::Stat stat;
    pdlfs::Status s = client->Getattr(__path, &stat);
    if (s.ok()) {
      __buf->st_mode = stat.FileMode();
      __buf->st_size = stat.FileSize();
      __buf->st_mtime = stat.ModifyTime() / 1000000;
      __buf->st_gid = stat.GroupId();
      __buf->st_uid = stat.UserId();
      return 0;
    } else {
      pdlfs::SetErrno(s);
      return -1;
    }
  }
}

int deltafs_mkfile(const char* __path, mode_t __mode) {
  pdlfs::port::InitOnce(&once, pdlfs::InitClient);
  if (client == NULL) {
    errno = ENODEV;
    return -1;
  } else {
    pdlfs::Status s = client->Mkfile(__path, __mode);
    if (s.ok()) {
      return 0;
    } else {
      pdlfs::SetErrno(s);
      return -1;
    }
  }
}

int deltafs_mkdir(const char* __path, mode_t __mode) {
  pdlfs::port::InitOnce(&once, pdlfs::InitClient);
  if (client == NULL) {
    errno = ENODEV;
    return -1;
  } else {
    pdlfs::Status s = client->Mkdir(__path, __mode);
    if (s.ok()) {
      return 0;
    } else {
      pdlfs::SetErrno(s);
      return -1;
    }
  }
}

#ifdef __cplusplus
}
#endif
