/**
 * Autogenerated by Thrift Compiler (0.9.2)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#include "res_types.h"

#include <algorithm>
#include <ostream>

#include <thrift/TToString.h>

namespace res_pd_rpc {


DevTarget_t::~DevTarget_t() throw() {
}


void DevTarget_t::__set_dev_id(const int32_t val) {
  this->dev_id = val;
}

void DevTarget_t::__set_dev_pipe_id(const int16_t val) {
  this->dev_pipe_id = val;
}

const char* DevTarget_t::ascii_fingerprint = "422C35A5D98C69C9CDE50568C7E3028F";
const uint8_t DevTarget_t::binary_fingerprint[16] = {0x42,0x2C,0x35,0xA5,0xD9,0x8C,0x69,0xC9,0xCD,0xE5,0x05,0x68,0xC7,0xE3,0x02,0x8F};

uint32_t DevTarget_t::read(::apache::thrift::protocol::TProtocol* iprot) {

  uint32_t xfer = 0;
  std::string fname;
  ::apache::thrift::protocol::TType ftype;
  int16_t fid;

  xfer += iprot->readStructBegin(fname);

  using ::apache::thrift::protocol::TProtocolException;

  bool isset_dev_id = false;
  bool isset_dev_pipe_id = false;

  while (true)
  {
    xfer += iprot->readFieldBegin(fname, ftype, fid);
    if (ftype == ::apache::thrift::protocol::T_STOP) {
      break;
    }
    switch (fid)
    {
      case 1:
        if (ftype == ::apache::thrift::protocol::T_I32) {
          xfer += iprot->readI32(this->dev_id);
          isset_dev_id = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      case 2:
        if (ftype == ::apache::thrift::protocol::T_I16) {
          xfer += iprot->readI16(this->dev_pipe_id);
          isset_dev_pipe_id = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      default:
        xfer += iprot->skip(ftype);
        break;
    }
    xfer += iprot->readFieldEnd();
  }

  xfer += iprot->readStructEnd();

  if (!isset_dev_id)
    throw TProtocolException(TProtocolException::INVALID_DATA);
  if (!isset_dev_pipe_id)
    throw TProtocolException(TProtocolException::INVALID_DATA);
  return xfer;
}

uint32_t DevTarget_t::write(::apache::thrift::protocol::TProtocol* oprot) const {
  uint32_t xfer = 0;
  oprot->incrementRecursionDepth();
  xfer += oprot->writeStructBegin("DevTarget_t");

  xfer += oprot->writeFieldBegin("dev_id", ::apache::thrift::protocol::T_I32, 1);
  xfer += oprot->writeI32(this->dev_id);
  xfer += oprot->writeFieldEnd();

  xfer += oprot->writeFieldBegin("dev_pipe_id", ::apache::thrift::protocol::T_I16, 2);
  xfer += oprot->writeI16(this->dev_pipe_id);
  xfer += oprot->writeFieldEnd();

  xfer += oprot->writeFieldStop();
  xfer += oprot->writeStructEnd();
  oprot->decrementRecursionDepth();
  return xfer;
}

void swap(DevTarget_t &a, DevTarget_t &b) {
  using ::std::swap;
  swap(a.dev_id, b.dev_id);
  swap(a.dev_pipe_id, b.dev_pipe_id);
}

DevTarget_t::DevTarget_t(const DevTarget_t& other0) {
  dev_id = other0.dev_id;
  dev_pipe_id = other0.dev_pipe_id;
}
DevTarget_t& DevTarget_t::operator=(const DevTarget_t& other1) {
  dev_id = other1.dev_id;
  dev_pipe_id = other1.dev_pipe_id;
  return *this;
}
std::ostream& operator<<(std::ostream& out, const DevTarget_t& obj) {
  using apache::thrift::to_string;
  out << "DevTarget_t(";
  out << "dev_id=" << to_string(obj.dev_id);
  out << ", " << "dev_pipe_id=" << to_string(obj.dev_pipe_id);
  out << ")";
  return out;
}


DevParserTarget_t::~DevParserTarget_t() throw() {
}


void DevParserTarget_t::__set_dev_id(const int32_t val) {
  this->dev_id = val;
}

void DevParserTarget_t::__set_gress_id(const int8_t val) {
  this->gress_id = val;
}

void DevParserTarget_t::__set_dev_pipe_id(const int16_t val) {
  this->dev_pipe_id = val;
}

void DevParserTarget_t::__set_parser_id(const int8_t val) {
  this->parser_id = val;
}

const char* DevParserTarget_t::ascii_fingerprint = "266A4AF49DEE7BD806959C5F640F90DF";
const uint8_t DevParserTarget_t::binary_fingerprint[16] = {0x26,0x6A,0x4A,0xF4,0x9D,0xEE,0x7B,0xD8,0x06,0x95,0x9C,0x5F,0x64,0x0F,0x90,0xDF};

uint32_t DevParserTarget_t::read(::apache::thrift::protocol::TProtocol* iprot) {

  uint32_t xfer = 0;
  std::string fname;
  ::apache::thrift::protocol::TType ftype;
  int16_t fid;

  xfer += iprot->readStructBegin(fname);

  using ::apache::thrift::protocol::TProtocolException;

  bool isset_dev_id = false;
  bool isset_gress_id = false;
  bool isset_dev_pipe_id = false;
  bool isset_parser_id = false;

  while (true)
  {
    xfer += iprot->readFieldBegin(fname, ftype, fid);
    if (ftype == ::apache::thrift::protocol::T_STOP) {
      break;
    }
    switch (fid)
    {
      case 1:
        if (ftype == ::apache::thrift::protocol::T_I32) {
          xfer += iprot->readI32(this->dev_id);
          isset_dev_id = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      case 2:
        if (ftype == ::apache::thrift::protocol::T_BYTE) {
          xfer += iprot->readByte(this->gress_id);
          isset_gress_id = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      case 3:
        if (ftype == ::apache::thrift::protocol::T_I16) {
          xfer += iprot->readI16(this->dev_pipe_id);
          isset_dev_pipe_id = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      case 4:
        if (ftype == ::apache::thrift::protocol::T_BYTE) {
          xfer += iprot->readByte(this->parser_id);
          isset_parser_id = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      default:
        xfer += iprot->skip(ftype);
        break;
    }
    xfer += iprot->readFieldEnd();
  }

  xfer += iprot->readStructEnd();

  if (!isset_dev_id)
    throw TProtocolException(TProtocolException::INVALID_DATA);
  if (!isset_gress_id)
    throw TProtocolException(TProtocolException::INVALID_DATA);
  if (!isset_dev_pipe_id)
    throw TProtocolException(TProtocolException::INVALID_DATA);
  if (!isset_parser_id)
    throw TProtocolException(TProtocolException::INVALID_DATA);
  return xfer;
}

uint32_t DevParserTarget_t::write(::apache::thrift::protocol::TProtocol* oprot) const {
  uint32_t xfer = 0;
  oprot->incrementRecursionDepth();
  xfer += oprot->writeStructBegin("DevParserTarget_t");

  xfer += oprot->writeFieldBegin("dev_id", ::apache::thrift::protocol::T_I32, 1);
  xfer += oprot->writeI32(this->dev_id);
  xfer += oprot->writeFieldEnd();

  xfer += oprot->writeFieldBegin("gress_id", ::apache::thrift::protocol::T_BYTE, 2);
  xfer += oprot->writeByte(this->gress_id);
  xfer += oprot->writeFieldEnd();

  xfer += oprot->writeFieldBegin("dev_pipe_id", ::apache::thrift::protocol::T_I16, 3);
  xfer += oprot->writeI16(this->dev_pipe_id);
  xfer += oprot->writeFieldEnd();

  xfer += oprot->writeFieldBegin("parser_id", ::apache::thrift::protocol::T_BYTE, 4);
  xfer += oprot->writeByte(this->parser_id);
  xfer += oprot->writeFieldEnd();

  xfer += oprot->writeFieldStop();
  xfer += oprot->writeStructEnd();
  oprot->decrementRecursionDepth();
  return xfer;
}

void swap(DevParserTarget_t &a, DevParserTarget_t &b) {
  using ::std::swap;
  swap(a.dev_id, b.dev_id);
  swap(a.gress_id, b.gress_id);
  swap(a.dev_pipe_id, b.dev_pipe_id);
  swap(a.parser_id, b.parser_id);
}

DevParserTarget_t::DevParserTarget_t(const DevParserTarget_t& other2) {
  dev_id = other2.dev_id;
  gress_id = other2.gress_id;
  dev_pipe_id = other2.dev_pipe_id;
  parser_id = other2.parser_id;
}
DevParserTarget_t& DevParserTarget_t::operator=(const DevParserTarget_t& other3) {
  dev_id = other3.dev_id;
  gress_id = other3.gress_id;
  dev_pipe_id = other3.dev_pipe_id;
  parser_id = other3.parser_id;
  return *this;
}
std::ostream& operator<<(std::ostream& out, const DevParserTarget_t& obj) {
  using apache::thrift::to_string;
  out << "DevParserTarget_t(";
  out << "dev_id=" << to_string(obj.dev_id);
  out << ", " << "gress_id=" << to_string(obj.gress_id);
  out << ", " << "dev_pipe_id=" << to_string(obj.dev_pipe_id);
  out << ", " << "parser_id=" << to_string(obj.parser_id);
  out << ")";
  return out;
}

} // namespace