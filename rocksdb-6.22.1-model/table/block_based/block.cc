//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Decodes the blocks generated by block_builder.cc.

#include "table/block_based/block.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "monitoring/perf_context_imp.h"
#include "port/port.h"
#include "port/stack_trace.h"
#include "rocksdb/comparator.h"
#include "table/block_based/block_prefix_index.h"
#include "table/block_based/data_block_footer.h"
#include "table/format.h"
#include "util/coding.h"

namespace ROCKSDB_NAMESPACE {

// Helper routine: decode the next block entry starting at "p",
// storing the number of shared key bytes, non_shared key bytes,
// and the length of the value in "*shared", "*non_shared", and
// "*value_length", respectively.  Will not derefence past "limit".
//
// If any errors are detected, returns nullptr.  Otherwise, returns a
// pointer to the key delta (just past the three decoded values).
struct DecodeEntry {
  inline const char* operator()(const char* p, const char* limit,
                                uint32_t* shared, uint32_t* non_shared,
                                uint32_t* value_length) {
    // We need 2 bytes for shared and non_shared size. We also need one more
    // byte either for value size or the actual value in case of value delta
    // encoding.
    assert(limit - p >= 3);
    *shared = reinterpret_cast<const unsigned char*>(p)[0];
    *non_shared = reinterpret_cast<const unsigned char*>(p)[1];
    *value_length = reinterpret_cast<const unsigned char*>(p)[2];
    if ((*shared | *non_shared | *value_length) < 128) {
      // Fast path: all three values are encoded in one byte each
      p += 3;
    } else {
      if ((p = GetVarint32Ptr(p, limit, shared)) == nullptr) return nullptr;
      if ((p = GetVarint32Ptr(p, limit, non_shared)) == nullptr) return nullptr;
      if ((p = GetVarint32Ptr(p, limit, value_length)) == nullptr) {
        return nullptr;
      }
    }

    // Using an assert in place of "return null" since we should not pay the
    // cost of checking for corruption on every single key decoding
    assert(!(static_cast<uint32_t>(limit - p) < (*non_shared + *value_length)));
    return p;
  }
};

// Helper routine: similar to DecodeEntry but does not have assertions.
// Instead, returns nullptr so that caller can detect and report failure.
struct CheckAndDecodeEntry {
  inline const char* operator()(const char* p, const char* limit,
                                uint32_t* shared, uint32_t* non_shared,
                                uint32_t* value_length) {
    // We need 2 bytes for shared and non_shared size. We also need one more
    // byte either for value size or the actual value in case of value delta
    // encoding.
    if (limit - p < 3) {
      return nullptr;
    }
    *shared = reinterpret_cast<const unsigned char*>(p)[0];
    *non_shared = reinterpret_cast<const unsigned char*>(p)[1];
    *value_length = reinterpret_cast<const unsigned char*>(p)[2];
    if ((*shared | *non_shared | *value_length) < 128) {
      // Fast path: all three values are encoded in one byte each
      p += 3;
    } else {
      if ((p = GetVarint32Ptr(p, limit, shared)) == nullptr) return nullptr;
      if ((p = GetVarint32Ptr(p, limit, non_shared)) == nullptr) return nullptr;
      if ((p = GetVarint32Ptr(p, limit, value_length)) == nullptr) {
        return nullptr;
      }
    }

    if (static_cast<uint32_t>(limit - p) < (*non_shared + *value_length)) {
      return nullptr;
    }
    return p;
  }
};

struct DecodeKey {
  inline const char* operator()(const char* p, const char* limit,
                                uint32_t* shared, uint32_t* non_shared) {
    uint32_t value_length;
    return DecodeEntry()(p, limit, shared, non_shared, &value_length);
  }
};

// In format_version 4, which is used by index blocks, the value size is not
// encoded before the entry, as the value is known to be the handle with the
// known size.
struct DecodeKeyV4 {
  inline const char* operator()(const char* p, const char* limit,
                                uint32_t* shared, uint32_t* non_shared) {
    // We need 2 bytes for shared and non_shared size. We also need one more
    // byte either for value size or the actual value in case of value delta
    // encoding.
    if (limit - p < 3) return nullptr;
    *shared = reinterpret_cast<const unsigned char*>(p)[0];
    *non_shared = reinterpret_cast<const unsigned char*>(p)[1];
    if ((*shared | *non_shared) < 128) {
      // Fast path: all three values are encoded in one byte each
      p += 2;
    } else {
      if ((p = GetVarint32Ptr(p, limit, shared)) == nullptr) return nullptr;
      if ((p = GetVarint32Ptr(p, limit, non_shared)) == nullptr) return nullptr;
    }
    return p;
  }
};

void DataBlockIter::NextImpl() { ParseNextDataKey<DecodeEntry>(); }

void DataBlockIter::NextOrReportImpl() {
  ParseNextDataKey<CheckAndDecodeEntry>();
}

void IndexBlockIter::NextImpl() { ParseNextIndexKey(); }

void IndexBlockIter::PrevImpl() {
  assert(Valid());
  // Scan backwards to a restart point before current_
  const uint32_t original = current_;
  while (GetRestartPoint(restart_index_) >= original) {
    if (restart_index_ == 0) {
      // No more entries
      current_ = restarts_;
      restart_index_ = num_restarts_;
      return;
    }
    restart_index_--;
  }
  SeekToRestartPoint(restart_index_);
  // Loop until end of current entry hits the start of original entry
  while (ParseNextIndexKey() && NextEntryOffset() < original) {
  }
}

// Similar to IndexBlockIter::PrevImpl but also caches the prev entries
void DataBlockIter::PrevImpl() {
  assert(Valid());

  assert(prev_entries_idx_ == -1 ||
         static_cast<size_t>(prev_entries_idx_) < prev_entries_.size());
  // Check if we can use cached prev_entries_
  if (prev_entries_idx_ > 0 &&
      prev_entries_[prev_entries_idx_].offset == current_) {
    // Read cached CachedPrevEntry
    prev_entries_idx_--;
    const CachedPrevEntry& current_prev_entry =
        prev_entries_[prev_entries_idx_];

    const char* key_ptr = nullptr;
    bool raw_key_cached;
    if (current_prev_entry.key_ptr != nullptr) {
      // The key is not delta encoded and stored in the data block
      key_ptr = current_prev_entry.key_ptr;
      raw_key_cached = false;
    } else {
      // The key is delta encoded and stored in prev_entries_keys_buff_
      key_ptr = prev_entries_keys_buff_.data() + current_prev_entry.key_offset;
      raw_key_cached = true;
    }
    const Slice current_key(key_ptr, current_prev_entry.key_size);

    current_ = current_prev_entry.offset;
    // TODO(ajkr): the copy when `raw_key_cached` is done here for convenience,
    // not necessity. It is convenient since this class treats keys as pinned
    // when `raw_key_` points to an outside buffer. So we cannot allow
    // `raw_key_` point into Prev cache as it is a transient outside buffer
    // (i.e., keys in it are not actually pinned).
    raw_key_.SetKey(current_key, raw_key_cached /* copy */);
    value_ = current_prev_entry.value;

    return;
  }

  // Clear prev entries cache
  prev_entries_idx_ = -1;
  prev_entries_.clear();
  prev_entries_keys_buff_.clear();

  // Scan backwards to a restart point before current_
  const uint32_t original = current_;
  while (GetRestartPoint(restart_index_) >= original) {
    if (restart_index_ == 0) {
      // No more entries
      current_ = restarts_;
      restart_index_ = num_restarts_;
      return;
    }
    restart_index_--;
  }

  SeekToRestartPoint(restart_index_);

  do {
    if (!ParseNextDataKey<DecodeEntry>()) {
      break;
    }
    Slice current_key = raw_key_.GetKey();

    if (raw_key_.IsKeyPinned()) {
      // The key is not delta encoded
      prev_entries_.emplace_back(current_, current_key.data(), 0,
                                 current_key.size(), value());
    } else {
      // The key is delta encoded, cache decoded key in buffer
      size_t new_key_offset = prev_entries_keys_buff_.size();
      prev_entries_keys_buff_.append(current_key.data(), current_key.size());

      prev_entries_.emplace_back(current_, nullptr, new_key_offset,
                                 current_key.size(), value());
    }
    // Loop until end of current entry hits the start of original entry
  } while (NextEntryOffset() < original);
  prev_entries_idx_ = static_cast<int32_t>(prev_entries_.size()) - 1;
}

void DataBlockIter::SeekImpl(const Slice& target, ColumnFamilyData *cfd /*NetBuffer*/) {
  Slice seek_key = target;
  PERF_TIMER_GUARD(block_seek_nanos);
  if (data_ == nullptr) {  // Not init yet
    return;
  }
  uint32_t index = 0;
  bool skip_linear_scan = false;
  bool ok = BinarySeek<DecodeKey>(seek_key, &index, &skip_linear_scan);

  if (!ok) {
    return;
  }
  FindKeyAfterBinarySeek(seek_key, index, skip_linear_scan);
}

// Optimized Seek for point lookup for an internal key `target`
// target = "seek_user_key @ type | seqno".
//
// For any type other than kTypeValue, kTypeDeletion, kTypeSingleDeletion,
// or kTypeBlobIndex, this function behaves identically as Seek().
//
// For any type in kTypeValue, kTypeDeletion, kTypeSingleDeletion,
// or kTypeBlobIndex:
//
// If the return value is FALSE, iter location is undefined, and it means:
// 1) there is no key in this block falling into the range:
//    ["seek_user_key @ type | seqno", "seek_user_key @ kTypeDeletion | 0"],
//    inclusive; AND
// 2) the last key of this block has a greater user_key from seek_user_key
//
// If the return value is TRUE, iter location has two possibilies:
// 1) If iter is valid, it is set to a location as if set by BinarySeek. In
//    this case, it points to the first key with a larger user_key or a matching
//    user_key with a seqno no greater than the seeking seqno.
// 2) If the iter is invalid, it means that either all the user_key is less
//    than the seek_user_key, or the block ends with a matching user_key but
//    with a smaller [ type | seqno ] (i.e. a larger seqno, or the same seqno
//    but larger type).
bool DataBlockIter::SeekForGetImpl(const Slice& target, ColumnFamilyData *cfd /*NetBuffer*/) {
  Slice target_user_key = ExtractUserKey(target);
  uint32_t map_offset = restarts_ + num_restarts_ * sizeof(uint32_t);
  uint8_t entry =
      data_block_hash_index_->Lookup(data_, map_offset, target_user_key);

  if (entry == kCollision) {
    // HashSeek not effective, falling back
    SeekImpl(target, cfd /*NetBuffer*/);
    return true;
  }

  if (entry == kNoEntry) {
    // Even if we cannot find the user_key in this block, the result may
    // exist in the next block. Consider this example:
    //
    // Block N:    [aab@100, ... , app@120]
    // boundary key: axy@50 (we make minimal assumption about a boundary key)
    // Block N+1:  [axy@10, ...   ]
    //
    // If seek_key = axy@60, the search will starts from Block N.
    // Even if the user_key is not found in the hash map, the caller still
    // have to continue searching the next block.
    //
    // In this case, we pretend the key is the the last restart interval.
    // The while-loop below will search the last restart interval for the
    // key. It will stop at the first key that is larger than the seek_key,
    // or to the end of the block if no one is larger.
    entry = static_cast<uint8_t>(num_restarts_ - 1);
  }

  uint32_t restart_index = entry;

  // check if the key is in the restart_interval
  assert(restart_index < num_restarts_);
  SeekToRestartPoint(restart_index);

  const char* limit = nullptr;
  if (restart_index_ + 1 < num_restarts_) {
    limit = data_ + GetRestartPoint(restart_index_ + 1);
  } else {
    limit = data_ + restarts_;
  }

  while (true) {
    // Here we only linear seek the target key inside the restart interval.
    // If a key does not exist inside a restart interval, we avoid
    // further searching the block content accross restart interval boundary.
    //
    // TODO(fwu): check the left and write boundary of the restart interval
    // to avoid linear seek a target key that is out of range.
    if (!ParseNextDataKey<DecodeEntry>(limit) ||
        CompareCurrentKey(target) >= 0) {
      // we stop at the first potential matching user key.
      break;
    }
  }

  if (current_ == restarts_) {
    // Search reaches to the end of the block. There are three possibilites:
    // 1) there is only one user_key match in the block (otherwise collsion).
    //    the matching user_key resides in the last restart interval, and it
    //    is the last key of the restart interval and of the block as well.
    //    ParseNextDataKey() skiped it as its [ type | seqno ] is smaller.
    //
    // 2) The seek_key is not found in the HashIndex Lookup(), i.e. kNoEntry,
    //    AND all existing user_keys in the restart interval are smaller than
    //    seek_user_key.
    //
    // 3) The seek_key is a false positive and happens to be hashed to the
    //    last restart interval, AND all existing user_keys in the restart
    //    interval are smaller than seek_user_key.
    //
    // The result may exist in the next block each case, so we return true.
    return true;
  }

  if (ucmp().Compare(raw_key_.GetUserKey(), target_user_key) != 0) {
    // the key is not in this block and cannot be at the next block either.
    return false;
  }

  // Here we are conservative and only support a limited set of cases
  ValueType value_type = ExtractValueType(raw_key_.GetInternalKey());
  if (value_type != ValueType::kTypeValue &&
      value_type != ValueType::kTypeDeletion &&
      value_type != ValueType::kTypeSingleDeletion &&
      value_type != ValueType::kTypeBlobIndex) {
    SeekImpl(target, cfd /*NetBuffer*/);
    return true;
  }

  // Result found, and the iter is correctly set.
  return true;
}

void IndexBlockIter::SeekImpl(const Slice& target, ColumnFamilyData *cfd /*NetBuffer*/) {
  TEST_SYNC_POINT("IndexBlockIter::Seek:0");
  PERF_TIMER_GUARD(block_seek_nanos);
  if (data_ == nullptr) {  // Not init yet
    return;
  }
  Slice seek_key = target;
  if (raw_key_.IsUserKey()) {
    seek_key = ExtractUserKey(target);
  }
  status_ = Status::OK();
  uint32_t index = 0;
  bool skip_linear_scan = false;
  bool ok = false;
  if (prefix_index_) {
    bool prefix_may_exist = true;
    ok = PrefixSeek(target, &index, &prefix_may_exist);
    if (!prefix_may_exist) {
      // This is to let the caller to distinguish between non-existing prefix,
      // and when key is larger than the last key, which both set Valid() to
      // false.
      current_ = restarts_;
      status_ = Status::NotFound();
    }
    // restart interval must be one when hash search is enabled so the binary
    // search simply lands at the right place.
    skip_linear_scan = true;
  } else if (value_delta_encoded_) {
    ok = BinarySeek<DecodeKeyV4>(seek_key, &index, &skip_linear_scan);
  } else {
    ok = BinarySeek<DecodeKey>(seek_key, &index, &skip_linear_scan);
  }

  if (!ok) {
    return;
  }
  FindKeyAfterBinarySeek(seek_key, index, skip_linear_scan);
}

void DataBlockIter::SeekForPrevImpl(const Slice& target) {
  PERF_TIMER_GUARD(block_seek_nanos);
  Slice seek_key = target;
  if (data_ == nullptr) {  // Not init yet
    return;
  }
  uint32_t index = 0;
  bool skip_linear_scan = false;
  bool ok = BinarySeek<DecodeKey>(seek_key, &index, &skip_linear_scan);

  if (!ok) {
    return;
  }
  FindKeyAfterBinarySeek(seek_key, index, skip_linear_scan);

  if (!Valid()) {
    SeekToLastImpl();
  } else {
    while (Valid() && CompareCurrentKey(seek_key) > 0) {
      PrevImpl();
    }
  }
}

void DataBlockIter::SeekToFirstImpl() {
  if (data_ == nullptr) {  // Not init yet
    return;
  }
  SeekToRestartPoint(0);
  ParseNextDataKey<DecodeEntry>();
}

void DataBlockIter::SeekToFirstOrReportImpl() {
  if (data_ == nullptr) {  // Not init yet
    return;
  }
  SeekToRestartPoint(0);
  ParseNextDataKey<CheckAndDecodeEntry>();
}

void IndexBlockIter::SeekToFirstImpl() {
  if (data_ == nullptr) {  // Not init yet
    return;
  }
  status_ = Status::OK();
  SeekToRestartPoint(0);
  ParseNextIndexKey();
}

void DataBlockIter::SeekToLastImpl() {
  if (data_ == nullptr) {  // Not init yet
    return;
  }
  SeekToRestartPoint(num_restarts_ - 1);
  while (ParseNextDataKey<DecodeEntry>() && NextEntryOffset() < restarts_) {
    // Keep skipping
  }
}

void IndexBlockIter::SeekToLastImpl() {
  if (data_ == nullptr) {  // Not init yet
    return;
  }
  status_ = Status::OK();
  SeekToRestartPoint(num_restarts_ - 1);
  while (ParseNextIndexKey() && NextEntryOffset() < restarts_) {
    // Keep skipping
  }
}

template <class TValue>
void BlockIter<TValue>::CorruptionError() {
  current_ = restarts_;
  restart_index_ = num_restarts_;
  status_ = Status::Corruption("bad entry in block");
  raw_key_.Clear();
  value_.clear();
}

template <typename DecodeEntryFunc>
bool DataBlockIter::ParseNextDataKey(const char* limit) {
  current_ = NextEntryOffset();
  const char* p = data_ + current_;
  if (!limit) {
    limit = data_ + restarts_;  // Restarts come right after data
  }

  if (p >= limit) {
    // No more entries to return.  Mark as invalid.
    current_ = restarts_;
    restart_index_ = num_restarts_;
    return false;
  }

  // Decode next entry
  uint32_t shared, non_shared, value_length;
  p = DecodeEntryFunc()(p, limit, &shared, &non_shared, &value_length);
  if (p == nullptr || raw_key_.Size() < shared) {
    CorruptionError();
    return false;
  } else {
    if (shared == 0) {
      // If this key doesn't share any bytes with prev key then we don't need
      // to decode it and can use its address in the block directly.
      raw_key_.SetKey(Slice(p, non_shared), false /* copy */);
    } else {
      // This key share `shared` bytes with prev key, we need to decode it
      raw_key_.TrimAppend(shared, p, non_shared);
    }

#ifndef NDEBUG
    if (global_seqno_ != kDisableGlobalSequenceNumber) {
      // If we are reading a file with a global sequence number we should
      // expect that all encoded sequence numbers are zeros and any value
      // type is kTypeValue, kTypeMerge, kTypeDeletion, or kTypeRangeDeletion.
      uint64_t packed = ExtractInternalKeyFooter(raw_key_.GetKey());
      SequenceNumber seqno;
      ValueType value_type;
      UnPackSequenceAndType(packed, &seqno, &value_type);
      assert(value_type == ValueType::kTypeValue ||
             value_type == ValueType::kTypeMerge ||
             value_type == ValueType::kTypeDeletion ||
             value_type == ValueType::kTypeRangeDeletion);
      assert(seqno == 0);
    }
#endif  // NDEBUG

    value_ = Slice(p + non_shared, value_length);
    if (shared == 0) {
      while (restart_index_ + 1 < num_restarts_ &&
             GetRestartPoint(restart_index_ + 1) < current_) {
        ++restart_index_;
      }
    }
    // else we are in the middle of a restart interval and the restart_index_
    // thus has not changed
    return true;
  }
}

bool IndexBlockIter::ParseNextIndexKey() {
  current_ = NextEntryOffset();
  const char* p = data_ + current_;
  const char* limit = data_ + restarts_;  // Restarts come right after data
  if (p >= limit) {
    // No more entries to return.  Mark as invalid.
    current_ = restarts_;
    restart_index_ = num_restarts_;
    return false;
  }

  // Decode next entry
  uint32_t shared, non_shared, value_length;
  if (value_delta_encoded_) {
    p = DecodeKeyV4()(p, limit, &shared, &non_shared);
    value_length = 0;
  } else {
    p = DecodeEntry()(p, limit, &shared, &non_shared, &value_length);
  }
  if (p == nullptr || raw_key_.Size() < shared) {
    CorruptionError();
    return false;
  }
  if (shared == 0) {
    // If this key doesn't share any bytes with prev key then we don't need
    // to decode it and can use its address in the block directly.
    raw_key_.SetKey(Slice(p, non_shared), false /* copy */);
  } else {
    // This key share `shared` bytes with prev key, we need to decode it
    raw_key_.TrimAppend(shared, p, non_shared);
  }
  value_ = Slice(p + non_shared, value_length);
  if (shared == 0) {
    while (restart_index_ + 1 < num_restarts_ &&
           GetRestartPoint(restart_index_ + 1) < current_) {
      ++restart_index_;
    }
  }
  // else we are in the middle of a restart interval and the restart_index_
  // thus has not changed
  if (value_delta_encoded_ || global_seqno_state_ != nullptr) {
    DecodeCurrentValue(shared);
  }
  return true;
}

// The format:
// restart_point   0: k, v (off, sz), k, v (delta-sz), ..., k, v (delta-sz)
// restart_point   1: k, v (off, sz), k, v (delta-sz), ..., k, v (delta-sz)
// ...
// restart_point n-1: k, v (off, sz), k, v (delta-sz), ..., k, v (delta-sz)
// where, k is key, v is value, and its encoding is in parenthesis.
// The format of each key is (shared_size, non_shared_size, shared, non_shared)
// The format of each value, i.e., block handle, is (offset, size) whenever the
// shared_size is 0, which included the first entry in each restart point.
// Otherwise the format is delta-size = block handle size - size of last block
// handle.
void IndexBlockIter::DecodeCurrentValue(uint32_t shared) {
  Slice v(value_.data(), data_ + restarts_ - value_.data());
  // Delta encoding is used if `shared` != 0.
  Status decode_s __attribute__((__unused__)) = decoded_value_.DecodeFrom(
      &v, have_first_key_,
      (value_delta_encoded_ && shared) ? &decoded_value_.handle : nullptr);
  assert(decode_s.ok());
  value_ = Slice(value_.data(), v.data() - value_.data());

  if (global_seqno_state_ != nullptr) {
    // Overwrite sequence number the same way as in DataBlockIter.

    IterKey& first_internal_key = global_seqno_state_->first_internal_key;
    first_internal_key.SetInternalKey(decoded_value_.first_internal_key,
                                      /* copy */ true);

    assert(GetInternalKeySeqno(first_internal_key.GetInternalKey()) == 0);

    ValueType value_type = ExtractValueType(first_internal_key.GetKey());
    assert(value_type == ValueType::kTypeValue ||
           value_type == ValueType::kTypeMerge ||
           value_type == ValueType::kTypeDeletion ||
           value_type == ValueType::kTypeRangeDeletion);

    first_internal_key.UpdateInternalKey(global_seqno_state_->global_seqno,
                                         value_type);
    decoded_value_.first_internal_key = first_internal_key.GetKey();
  }
}

template <class TValue>
void BlockIter<TValue>::FindKeyAfterBinarySeek(const Slice& target,
                                               uint32_t index,
                                               bool skip_linear_scan) {
  // SeekToRestartPoint() only does the lookup in the restart block. We need
  // to follow it up with NextImpl() to position the iterator at the restart
  // key.
  SeekToRestartPoint(index);
  NextImpl();

  if (!skip_linear_scan) {
    // Linear search (within restart block) for first key >= target
    uint32_t max_offset;
    if (index + 1 < num_restarts_) {
      // We are in a non-last restart interval. Since `BinarySeek()` guarantees
      // the next restart key is strictly greater than `target`, we can
      // terminate upon reaching it without any additional key comparison.
      max_offset = GetRestartPoint(index + 1);
    } else {
      // We are in the last restart interval. The while-loop will terminate by
      // `Valid()` returning false upon advancing past the block's last key.
      max_offset = port::kMaxUint32;
    }
    while (true) {
      NextImpl();
      if (!Valid()) {
        break;
      }
      if (current_ == max_offset) {
        assert(CompareCurrentKey(target) > 0);
        break;
      } else if (CompareCurrentKey(target) >= 0) {
        break;
      }
    }
  }
}

// Binary searches in restart array to find the starting restart point for the
// linear scan, and stores it in `*index`. Assumes restart array does not
// contain duplicate keys. It is guaranteed that the restart key at `*index + 1`
// is strictly greater than `target` or does not exist (this can be used to
// elide a comparison when linear scan reaches all the way to the next restart
// key). Furthermore, `*skip_linear_scan` is set to indicate whether the
// `*index`th restart key is the final result so that key does not need to be
// compared again later.
template <class TValue>
template <typename DecodeKeyFunc>
bool BlockIter<TValue>::BinarySeek(const Slice& target, uint32_t* index,
                                   bool* skip_linear_scan) {
  if (restarts_ == 0) {
    // SST files dedicated to range tombstones are written with index blocks
    // that have no keys while also having `num_restarts_ == 1`. This would
    // cause a problem for `BinarySeek()` as it'd try to access the first key
    // which does not exist. We identify such blocks by the offset at which
    // their restarts are stored, and return false to prevent any attempted
    // key accesses.
    return false;
  }

  *skip_linear_scan = false;
  // Loop invariants:
  // - Restart key at index `left` is less than or equal to the target key. The
  //   sentinel index `-1` is considered to have a key that is less than all
  //   keys.
  // - Any restart keys after index `right` are strictly greater than the target
  //   key.
  int64_t left = -1, right = num_restarts_ - 1;
  while (left != right) {
    // The `mid` is computed by rounding up so it lands in (`left`, `right`].
    int64_t mid = left + (right - left + 1) / 2;
    uint32_t region_offset = GetRestartPoint(static_cast<uint32_t>(mid));
    uint32_t shared, non_shared;
    const char* key_ptr = DecodeKeyFunc()(
        data_ + region_offset, data_ + restarts_, &shared, &non_shared);
    if (key_ptr == nullptr || (shared != 0)) {
      CorruptionError();
      return false;
    }
    Slice mid_key(key_ptr, non_shared);
    raw_key_.SetKey(mid_key, false /* copy */);
    int cmp = CompareCurrentKey(target);
    if (cmp < 0) {
      // Key at "mid" is smaller than "target". Therefore all
      // blocks before "mid" are uninteresting.
      left = mid;
    } else if (cmp > 0) {
      // Key at "mid" is >= "target". Therefore all blocks at or
      // after "mid" are uninteresting.
      right = mid - 1;
    } else {
      *skip_linear_scan = true;
      left = right = mid;
    }
  }

  if (left == -1) {
    // All keys in the block were strictly greater than `target`. So the very
    // first key in the block is the final seek result.
    *skip_linear_scan = true;
    *index = 0;
  } else {
    *index = static_cast<uint32_t>(left);
  }
  return true;
}

// Compare target key and the block key of the block of `block_index`.
// Return -1 if error.
int IndexBlockIter::CompareBlockKey(uint32_t block_index, const Slice& target) {
  uint32_t region_offset = GetRestartPoint(block_index);
  uint32_t shared, non_shared;
  const char* key_ptr =
      value_delta_encoded_
          ? DecodeKeyV4()(data_ + region_offset, data_ + restarts_, &shared,
                          &non_shared)
          : DecodeKey()(data_ + region_offset, data_ + restarts_, &shared,
                        &non_shared);
  if (key_ptr == nullptr || (shared != 0)) {
    CorruptionError();
    return 1;  // Return target is smaller
  }
  Slice block_key(key_ptr, non_shared);
  raw_key_.SetKey(block_key, false /* copy */);
  return CompareCurrentKey(target);
}

// Binary search in block_ids to find the first block
// with a key >= target
bool IndexBlockIter::BinaryBlockIndexSeek(const Slice& target,
                                          uint32_t* block_ids, uint32_t left,
                                          uint32_t right, uint32_t* index,
                                          bool* prefix_may_exist) {
  assert(left <= right);
  assert(index);
  assert(prefix_may_exist);
  *prefix_may_exist = true;
  uint32_t left_bound = left;

  while (left <= right) {
    uint32_t mid = (right + left) / 2;

    int cmp = CompareBlockKey(block_ids[mid], target);
    if (!status_.ok()) {
      return false;
    }
    if (cmp < 0) {
      // Key at "target" is larger than "mid". Therefore all
      // blocks before or at "mid" are uninteresting.
      left = mid + 1;
    } else {
      // Key at "target" is <= "mid". Therefore all blocks
      // after "mid" are uninteresting.
      // If there is only one block left, we found it.
      if (left == right) break;
      right = mid;
    }
  }

  if (left == right) {
    // In one of the two following cases:
    // (1) left is the first one of block_ids
    // (2) there is a gap of blocks between block of `left` and `left-1`.
    // we can further distinguish the case of key in the block or key not
    // existing, by comparing the target key and the key of the previous
    // block to the left of the block found.
    if (block_ids[left] > 0 &&
        (left == left_bound || block_ids[left - 1] != block_ids[left] - 1) &&
        CompareBlockKey(block_ids[left] - 1, target) > 0) {
      current_ = restarts_;
      *prefix_may_exist = false;
      return false;
    }

    *index = block_ids[left];
    return true;
  } else {
    assert(left > right);

    // If the next block key is larger than seek key, it is possible that
    // no key shares the prefix with `target`, or all keys with the same
    // prefix as `target` are smaller than prefix. In the latter case,
    // we are mandated to set the position the same as the total order.
    // In the latter case, either:
    // (1) `target` falls into the range of the next block. In this case,
    //     we can place the iterator to the next block, or
    // (2) `target` is larger than all block keys. In this case we can
    //     keep the iterator invalidate without setting `prefix_may_exist`
    //     to false.
    // We might sometimes end up with setting the total order position
    // while there is no key sharing the prefix as `target`, but it
    // still follows the contract.
    uint32_t right_index = block_ids[right];
    assert(right_index + 1 <= num_restarts_);
    if (right_index + 1 < num_restarts_) {
      if (CompareBlockKey(right_index + 1, target) >= 0) {
        *index = right_index + 1;
        return true;
      } else {
        // We have to set the flag here because we are not positioning
        // the iterator to the total order position.
        *prefix_may_exist = false;
      }
    }

    // Mark iterator invalid
    current_ = restarts_;
    return false;
  }
}

bool IndexBlockIter::PrefixSeek(const Slice& target, uint32_t* index,
                                bool* prefix_may_exist) {
  assert(index);
  assert(prefix_may_exist);
  assert(prefix_index_);
  *prefix_may_exist = true;
  Slice seek_key = target;
  if (raw_key_.IsUserKey()) {
    seek_key = ExtractUserKey(target);
  }
  uint32_t* block_ids = nullptr;
  uint32_t num_blocks = prefix_index_->GetBlocks(target, &block_ids);

  if (num_blocks == 0) {
    current_ = restarts_;
    *prefix_may_exist = false;
    return false;
  } else {
    assert(block_ids);
    return BinaryBlockIndexSeek(seek_key, block_ids, 0, num_blocks - 1, index,
                                prefix_may_exist);
  }
}

uint32_t Block::NumRestarts() const {
  assert(size_ >= 2 * sizeof(uint32_t));
  uint32_t block_footer = DecodeFixed32(data_ + size_ - sizeof(uint32_t));
  uint32_t num_restarts = block_footer;
  if (size_ > kMaxBlockSizeSupportedByHashIndex) {
    // In BlockBuilder, we have ensured a block with HashIndex is less than
    // kMaxBlockSizeSupportedByHashIndex (64KiB).
    //
    // Therefore, if we encounter a block with a size > 64KiB, the block
    // cannot have HashIndex. So the footer will directly interpreted as
    // num_restarts.
    //
    // Such check is for backward compatibility. We can ensure legacy block
    // with a vary large num_restarts i.e. >= 0x80000000 can be interpreted
    // correctly as no HashIndex even if the MSB of num_restarts is set.
    return num_restarts;
  }
  BlockBasedTableOptions::DataBlockIndexType index_type;
  UnPackIndexTypeAndNumRestarts(block_footer, &index_type, &num_restarts);
  return num_restarts;
}

BlockBasedTableOptions::DataBlockIndexType Block::IndexType() const {
  assert(size_ >= 2 * sizeof(uint32_t));
  if (size_ > kMaxBlockSizeSupportedByHashIndex) {
    // The check is for the same reason as that in NumRestarts()
    return BlockBasedTableOptions::kDataBlockBinarySearch;
  }
  uint32_t block_footer = DecodeFixed32(data_ + size_ - sizeof(uint32_t));
  uint32_t num_restarts = block_footer;
  BlockBasedTableOptions::DataBlockIndexType index_type;
  UnPackIndexTypeAndNumRestarts(block_footer, &index_type, &num_restarts);
  return index_type;
}

Block::~Block() {
  // This sync point can be re-enabled if RocksDB can control the
  // initialization order of any/all static options created by the user.
  // TEST_SYNC_POINT("Block::~Block");
}

Block::Block(BlockContents&& contents, size_t read_amp_bytes_per_bit,
             Statistics* statistics)
    : contents_(std::move(contents)),
      data_(contents_.data.data()),
      size_(contents_.data.size()),
      restart_offset_(0),
      num_restarts_(0) {
  TEST_SYNC_POINT("Block::Block:0");
  if (size_ < sizeof(uint32_t)) {
    size_ = 0;  // Error marker
  } else {
    // Should only decode restart points for uncompressed blocks
    num_restarts_ = NumRestarts();
    switch (IndexType()) {
      case BlockBasedTableOptions::kDataBlockBinarySearch:
        restart_offset_ = static_cast<uint32_t>(size_) -
                          (1 + num_restarts_) * sizeof(uint32_t);
        if (restart_offset_ > size_ - sizeof(uint32_t)) {
          // The size is too small for NumRestarts() and therefore
          // restart_offset_ wrapped around.
          size_ = 0;
        }
        break;
      case BlockBasedTableOptions::kDataBlockBinaryAndHash:
        if (size_ < sizeof(uint32_t) /* block footer */ +
                        sizeof(uint16_t) /* NUM_BUCK */) {
          size_ = 0;
          break;
        }

        uint16_t map_offset;
        data_block_hash_index_.Initialize(
            contents.data.data(),
            static_cast<uint16_t>(contents.data.size() -
                                  sizeof(uint32_t)), /*chop off
                                                 NUM_RESTARTS*/
            &map_offset);

        restart_offset_ = map_offset - num_restarts_ * sizeof(uint32_t);

        if (restart_offset_ > map_offset) {
          // map_offset is too small for NumRestarts() and
          // therefore restart_offset_ wrapped around.
          size_ = 0;
          break;
        }
        break;
      default:
        size_ = 0;  // Error marker
    }
  }
  if (read_amp_bytes_per_bit != 0 && statistics && size_ != 0) {
    read_amp_bitmap_.reset(new BlockReadAmpBitmap(
        restart_offset_, read_amp_bytes_per_bit, statistics));
  }
}

DataBlockIter* Block::NewDataIterator(const Comparator* raw_ucmp,
                                      SequenceNumber global_seqno,
                                      DataBlockIter* iter, Statistics* stats,
                                      bool block_contents_pinned) {
  DataBlockIter* ret_iter;
  if (iter != nullptr) {
    ret_iter = iter;
  } else {
    ret_iter = new DataBlockIter;
  }
  if (size_ < 2 * sizeof(uint32_t)) {
    ret_iter->Invalidate(Status::Corruption("bad block contents"));
    return ret_iter;
  }
  if (num_restarts_ == 0) {
    // Empty block.
    ret_iter->Invalidate(Status::OK());
    return ret_iter;
  } else {
    ret_iter->Initialize(
        raw_ucmp, data_, restart_offset_, num_restarts_, global_seqno,
        read_amp_bitmap_.get(), block_contents_pinned,
        data_block_hash_index_.Valid() ? &data_block_hash_index_ : nullptr);
    if (read_amp_bitmap_) {
      if (read_amp_bitmap_->GetStatistics() != stats) {
        // DB changed the Statistics pointer, we need to notify read_amp_bitmap_
        read_amp_bitmap_->SetStatistics(stats);
      }
    }
  }

  return ret_iter;
}

IndexBlockIter* Block::NewIndexIterator(
    const Comparator* raw_ucmp, SequenceNumber global_seqno,
    IndexBlockIter* iter, Statistics* /*stats*/, bool total_order_seek,
    bool have_first_key, bool key_includes_seq, bool value_is_full,
    bool block_contents_pinned, BlockPrefixIndex* prefix_index) {
  IndexBlockIter* ret_iter;
  if (iter != nullptr) {
    ret_iter = iter;
  } else {
    ret_iter = new IndexBlockIter;
  }
  if (size_ < 2 * sizeof(uint32_t)) {
    ret_iter->Invalidate(Status::Corruption("bad block contents"));
    return ret_iter;
  }
  if (num_restarts_ == 0) {
    // Empty block.
    ret_iter->Invalidate(Status::OK());
    return ret_iter;
  } else {
    BlockPrefixIndex* prefix_index_ptr =
        total_order_seek ? nullptr : prefix_index;
    ret_iter->Initialize(raw_ucmp, data_, restart_offset_, num_restarts_,
                         global_seqno, prefix_index_ptr, have_first_key,
                         key_includes_seq, value_is_full,
                         block_contents_pinned);
  }

  return ret_iter;
}

size_t Block::ApproximateMemoryUsage() const {
  size_t usage = usable_size();
#ifdef ROCKSDB_MALLOC_USABLE_SIZE
  usage += malloc_usable_size((void*)this);
#else
  usage += sizeof(*this);
#endif  // ROCKSDB_MALLOC_USABLE_SIZE
  if (read_amp_bitmap_) {
    usage += read_amp_bitmap_->ApproximateMemoryUsage();
  }
  return usage;
}

}  // namespace ROCKSDB_NAMESPACE
