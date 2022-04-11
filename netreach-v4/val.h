#ifndef VAL_H
#define VAL_H

#include <array>
#include <limits>
#include <string>
//#include "rocksdb/slice.h"
#include "helper.h"

class Val {

 public:

  static int32_t MAX_VALLEN; // max # of bytes
  static int32_t SWITCH_MAX_VALLEN; // max # of bytes in switch
  static int32_t get_padding_size(int32_t vallen);

  Val();
  ~Val();
  Val(const uint64_t* buf, int32_t length);
  Val(const Val &other);
  Val(const volatile Val &other);
  Val &operator=(const Val &other);
  Val &operator=(const volatile Val &other);
  // Qualifiers of member functions are used to restrict "this"
  volatile Val &operator=(const Val &other) volatile;
  volatile Val &operator=(const volatile Val &other) volatile;
  bool operator==(const Val& other);

  /*rocksdb::Slice to_slice() const; // NOTE: slice is shallow copy
  void from_slice(rocksdb::Slice& slice);*/

  void from_string(std::string& str);

  // operation on packet buf (1B vallength + valdata)
  uint32_t deserialize(const char *buf, uint32_t buflen);
  uint32_t deserialize_vallen(const char *buf, uint32_t buflen)
  uint32_t deserialize_val(const char *buf, uint32_t buflen)
  uint32_t serialize(char *buf, uint32_t buflen);

  uint32_t get_bytesnum() const;

  std::string to_string() const; // For print

  int32_t val_length; // val_length (# of bytes)
  char *val_data;
} PACKED;

#endif
