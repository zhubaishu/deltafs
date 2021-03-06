/*
 * Copyright (c) 2015-2017 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#pragma once

#include "pdlfs-common/env.h"
#include "pdlfs-common/leveldb/db/options.h"
#include "pdlfs-common/port.h"

#include <stddef.h>
#include <stdint.h>

namespace pdlfs {
namespace plfsio {

class BatchCursor;
class EventListener;

struct IoStats {
  IoStats();

  // Total bytes accessed as indexes
  uint64_t index_bytes;
  // Total number of I/O operations for reading or writing indexes
  uint64_t index_ops;
  // Total bytes accessed as data
  uint64_t data_bytes;
  // Total number of I/O operations for reading or writing data
  uint64_t data_ops;
};

// Directory semantics
enum DirMode {
  // Each duplicated key insertion within an epoch are considered separate
  // records
  kMultiMap = 0x00,
  // Each duplicated key insertion within an epoch overrides the previous
  // insertion in the same epoch
  kUniqueOverride = 0x01,
  // Duplicated key insertions are silently discarded
  kUniqueDrop = 0x02,
  // No duplicated keys
  kUnique = 0x03
};

struct DirOptions {
  DirOptions();

  // Total memory reserved for write buffering.
  // This includes both the buffer space for creating and sorting memtables and
  // the memory space for constructing the data blocks and filter blocks of each
  // table. This, however, does *not* include the buffer space for accumulating
  // block writes to ensure an optimized write size.
  // Default: 4MB
  size_t total_memtable_budget;

  // Flush memtable as soon as it reaches the specified utilization target.
  // Default: 1 (100%)
  double memtable_util;

  // Skip sorting memtables.
  // This is useful when the input data is known to be pre-sorted.
  // Default: false
  bool skip_sort;

  // Estimated average key size.
  // Default: 8 bytes
  size_t key_size;

  // Estimated average value size.
  // Default: 32 bytes
  size_t value_size;

  // Bloom filter bits per key.
  // Set to zero to disable the use of bloom filters.
  // Default: 8 bits
  size_t bf_bits_per_key;

  // Approximate size of user data packed per data block.
  // Note that block is used both as the packaging format and as the logical I/O
  // unit for reading and writing the underlying data log objects.
  // The size of all index and filter blocks are *not* affected
  // by this option.
  // Default: 32K
  size_t block_size;

  // Start zero padding if current estimated block size reaches the
  // specified utilization target. Only applies to data blocks.
  // Default: 0.996 (99.6%)
  double block_util;

  // Set to false to disable the zero padding of data blocks.
  // Only relevant to data blocks.
  // Default: true
  bool block_padding;

  // Number of data blocks to accumulate before flush out to the data log in a
  // single atomic batch. Aggregating data block writes can reduce the I/O
  // contention among multiple concurrent compaction threads that compete
  // for access to a shared data log.
  // Default: 2MB
  size_t block_batch_size;

  // Write buffer size for each physical data log.
  // Set to zero to disable buffering and each data block flush
  // becomes an actual physical write to the underlying storage.
  // Default: 4MB
  size_t data_buffer;

  // Minimum write size for each physical data log.
  // Default: 4MB
  size_t min_data_buffer;

  // Write buffer size for each physical index log.
  // Set to zero to disable buffering and each index block flush
  // becomes an actual physical write to the underlying storage.
  // Default: 4MB
  size_t index_buffer;

  // Minimum write size for each physical index log.
  // Default: 4MB
  size_t min_index_buffer;

  // Add necessary padding to the end of each log object to ensure the
  // final object size is always some multiple of the write size.
  // Required by some underlying object stores.
  // Default: false
  bool tail_padding;

  // Thread pool used to run concurrent background compaction jobs.
  // If set to NULL, Env::Default() may be used to schedule jobs if permitted.
  // Otherwise, the caller's thread context will be used directly to serve
  // compactions.
  // Default: NULL
  ThreadPool* compaction_pool;

  // Thread pool used to run concurrent background reads.
  // If set to NULL, Env::Default() may be used to schedule reads if permitted.
  // Otherwise, the caller's thread context will be used directly.
  // Consider setting parallel_reads to true to take full advantage
  // of this thread pool.
  // Default: NULL
  ThreadPool* reader_pool;

  // Number of bytes to read when loading the indexes.
  // Default: 8MB
  size_t read_size;

  // Set to true to enable parallel reading across different epochs.
  // Otherwise, reads progress serially over all epochs.
  // Default: false
  bool parallel_reads;

  // True if write operations should be performed in a non-blocking manner,
  // in which case a special status is returned instead of blocking the
  // writer to wait for buffer space.
  // Default: false
  bool non_blocking;

  // Number of microseconds to slowdown if a writer cannot make progress
  // because the system has run out of its buffer space.
  // Default: 0
  uint64_t slowdown_micros;

  // If true, the implementation will do aggressive checking of the
  // data it is processing and will stop early if it detects any
  // errors.
  // Default: false
  bool paranoid_checks;

  // Ignore all filters during reads.
  // Default: false
  bool ignore_filters;

  // Compression type to be applied to index blocks.
  // Data blocks are never compressed.
  // Default: kNoCompression
  CompressionType compression;

  // True if compressed data is written out even if compression rate is low.
  // Default: false
  bool force_compression;

  // True if all data read from underlying storage will be verified
  // against the corresponding checksums stored.
  // Default: false
  bool verify_checksums;

  // True if checksum calculation and verification are completely skipped.
  // Default: false
  bool skip_checksums;

  // True if read I/O should be measured.
  // Default: true
  bool measure_reads;

  // True if write I/O should be measured.
  // Default: true
  bool measure_writes;

  // Number of partitions to divide the data. Specified in logarithmic
  // number so each x will give 2**x partitions.
  // REQUIRES: 0 <= lg_parts <= 8
  // Default: 0
  int lg_parts;

  // User callback for handling background events.
  // Default: NULL
  EventListener* listener;

  // Dir mode
  // Default: kUnique
  DirMode mode;

  // Env instance used to access objects or files stored in the underlying
  // storage system. If NULL, Env::Default() will be used.
  // Default: NULL
  Env* env;

  // If the env context may be used to run background jobs.
  // Default: false
  bool allow_env_threads;

  // True if the underlying storage is implemented as a parallel
  // file system rather than an object storage.
  // Default: true
  bool is_env_pfs;

  // Rank of the process in the directory.
  // Default: 0
  int rank;
};

// Parse a given configuration string to structured options.
extern DirOptions ParseDirOptions(const char* conf);

// Abstraction for a thread-unsafe and possibly-buffered
// append-only log stream.
class LogSink {
 public:
  LogSink(const std::string& filename, WritableFile* f, port::Mutex* mu = NULL)
      : mu_(mu), filename_(filename), file_(f), offset_(0), refs_(0) {}

  uint64_t Ltell() const {
    if (mu_ != NULL) mu_->AssertHeld();
    return offset_;
  }

  void Lock() {
    if (mu_ != NULL) {
      mu_->Lock();
    }
  }

  Status Lwrite(const Slice& data) {
    if (file_ == NULL) {
      return Status::AssertionFailed("File already closed", filename_);
    } else {
      if (mu_ != NULL) {
        mu_->AssertHeld();
      }
      Status result = file_->Append(data);
      if (result.ok()) {
        result = file_->Flush();
        if (result.ok()) {
          offset_ += data.size();
        }
      }
      return result;
    }
  }

  Status Lsync() {
    Status status;
    if (file_ != NULL) {
      if (mu_ != NULL) mu_->AssertHeld();
      status = file_->Sync();
    }
    return status;
  }

  void Unlock() {
    if (mu_ != NULL) {
      mu_->Unlock();
    }
  }

  Status Lclose(bool sync = false);
  void Ref() { refs_++; }
  void Unref();

 private:
  ~LogSink();
  // No copying allowed
  void operator=(const LogSink&);
  LogSink(const LogSink&);
  Status Close();

  port::Mutex* mu_;  // Constant after construction
  const std::string filename_;
  WritableFile* file_;  // State protected by mu_
  uint64_t offset_;
  uint32_t refs_;
};

// Abstraction for a thread-unsafe and possibly-buffered
// random access log file.
class LogSource {
 public:
  LogSource(RandomAccessFile* f, uint64_t s) : file_(f), size_(s), refs_(0) {}

  Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) {
    return file_->Read(offset, n, result, scratch);
  }

  uint64_t Size() const { return size_; }

  void Ref() { refs_++; }
  void Unref();

 private:
  ~LogSource();
  // No copying allowed
  void operator=(const LogSource&);
  LogSource(const LogSource&);

  RandomAccessFile* file_;
  uint64_t size_;
  uint32_t refs_;
};

// Destroy the contents of the specified directory.
// Be very careful using this method.
extern Status DestroyDir(const std::string& dirname, const DirOptions& options);

// Deltafs Plfs Dir Writer
class DirWriter {
 public:
  DirWriter() {}
  virtual ~DirWriter();

  // Report the I/O stats for logging the data and the indexes.
  virtual IoStats GetIoStats() const = 0;

  // Return the estimated size of each table.
  // The actual size of each generated may differ.
  virtual uint64_t TEST_estimated_sstable_size() const = 0;

  // Return the max size of each filter.
  // The actual size of each generated filter may be smaller.
  virtual uint64_t TEST_max_filter_size() const = 0;

  // Return the total number of keys inserted so far.
  virtual uint32_t TEST_num_keys() const = 0;

  // Return the total number of keys rejected so far.
  virtual uint32_t TEST_num_dropped_keys() const = 0;

  // Return the total number of data blocks generated so far.
  virtual uint32_t TEST_num_data_blocks() const = 0;

  // Return the total number of SSTable generated so far.
  virtual uint32_t TEST_num_sstables() const = 0;

  // Return the aggregated size of all index blocks.
  // Before compression and excluding any padding or checksum bytes.
  virtual uint64_t TEST_raw_index_contents() const = 0;

  // Return the aggregated size of all filter blocks.
  // Before compression and excluding any padding or checksum bytes.
  virtual uint64_t TEST_raw_filter_contents() const = 0;

  // Return the aggregated size of all data blocks.
  // Excluding any padding or checksum bytes.
  virtual uint64_t TEST_raw_data_contents() const = 0;

  // Return the aggregated size of all inserted keys.
  virtual uint64_t TEST_key_bytes() const = 0;

  // Return the aggregated size of all inserted values.
  virtual uint64_t TEST_value_bytes() const = 0;

  // Return the total amount of memory reserved by this directory.
  virtual uint64_t TEST_total_memory_usage() const = 0;

  // Open an I/O writer against a specified plfs-style directory.
  // Return OK on success, or a non-OK status on errors.
  static Status Open(const DirOptions& options, const std::string& dirname,
                     DirWriter** result);

  // Perform a batch of file appends in a single operation.
  // REQUIRES: Finish() has not been called.
  virtual Status Write(BatchCursor* cursor, int epoch = -1) = 0;

  // Append a piece of data to a specific file under the directory.
  // Set epoch to -1 to disable epoch validation.
  // REQUIRES: Finish() has not been called.
  virtual Status Append(const Slice& fid, const Slice& data,
                        int epoch = -1) = 0;

  // Force a memtable compaction.
  // Set epoch to -1 to disable epoch validation.
  // REQUIRES: Finish() has not been called.
  virtual Status Flush(int epoch = -1) = 0;

  // Force a memtable compaction and start a new epoch.
  // Set epoch to -1 to disable epoch validation.
  // REQUIRES: Finish() has not been called.
  virtual Status EpochFlush(int epoch = -1) = 0;

  // Wait for one background compaction to finish if there is any.
  // Return OK on success, or a non-OK status on errors.
  virtual Status WaitForOne() = 0;

  // Wait for all on-going background compactions to finish.
  // Return OK on success, or a non-OK status on errors.
  virtual Status Wait() = 0;

  // Force a compaction and finalize all log files.
  // No further write operation is allowed after this call.
  virtual Status Finish() = 0;

 private:
  // No copying allowed
  void operator=(const DirWriter&);
  DirWriter(const DirWriter&);
};

// Deltafs Plfs Dir Reader
class DirReader {
 public:
  DirReader() {}
  virtual ~DirReader();

  // Open an I/O reader against a specific plfs-style directory.
  // Return OK on success, or a non-OK status on errors.
  static Status Open(const DirOptions& options, const std::string& name,
                     DirReader** result);

  // Fetch the entire data from a specific file under a given plfs directory.
  virtual Status ReadAll(const Slice& fid, std::string* dst, char* tmp = NULL,
                         size_t tmp_length = 0, size_t* table_seeks = NULL,
                         size_t* seeks = NULL) = 0;

  // Return the aggregated I/O stats accumulated so far.
  virtual IoStats GetIoStats() const = 0;

 private:
  // No copying allowed
  void operator=(const DirReader&);
  DirReader(const DirReader&);
};

}  // namespace plfsio
}  // namespace pdlfs
