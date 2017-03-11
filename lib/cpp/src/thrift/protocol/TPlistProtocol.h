/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _THRIFT_PROTOCOL_TPLISTPROTOCOL_H_
#define _THRIFT_PROTOCOL_TPLISTPROTOCOL_H_ 1

#include "TVirtualProtocol.h"

#include <stack>

namespace apache { namespace thrift { namespace protocol {

// Forward declaration
class TPlistContext;

/**
 * Plist protocol for Thrift.
 *
 * Implements a protocol which uses Plist as the wire-format.
 *
 * Thrift types are represented as described below:
 *
 * 1. Every Thrift integer type is represented as a Plist number.
 *
 * 2. Thrift doubles are represented as Plist numbers. Some special values are
 *    represented as strings:
 *    a. "NaN" for not-a-number values
 *    b. "Infinity" for postive infinity
 *    c. "-Infinity" for negative infinity
 *
 * 3. Thrift string values are emitted as Plist strings, with appropriate
 *    escaping.
 *
 * 4. Thrift binary values are encoded into Base64 and emitted as Plist strings.
 *    The readBinary() method is written such that it will properly skip if
 *    called on a Thrift string (although it will decode garbage data).
 *
 * 5. Thrift structs are represented as Plist objects, with the field ID as the
 *    key, and the field value represented as a Plist object with a single
 *    key-value pair. The key is a short string identifier for that type,
 *    followed by the value. The valid type identifiers are: "tf" for bool,
 *    "i8" for byte, "i16" for 16-bit integer, "i32" for 32-bit integer, "i64"
 *    for 64-bit integer, "dbl" for double-precision loating point, "str" for
 *    string (including binary), "rec" for struct ("records"), "map" for map,
 *    "lst" for list, "set" for set.
 *
 * 6. Thrift lists and sets are represented as Plist arrays, with the first
 *    element of the Plist array being the string identifier for the Thrift
 *    element type and the second element of the Plist array being the count of
 *    the Thrift elements. The Thrift elements then follow.
 *
 * 7. Thrift maps are represented as Plist arrays, each array elements is a
 *    plist object containing the key-value pairs. Note that Plist keys can only
 *    be strings, which means that the key type of the Thrift map should be
 *    restricted to numeric or string types -- in the case of numerics, they
 *    are serialized as strings.
 *
 * More discussion of the double handling is probably warranted. The aim of
 * the current implementation is to match as closely as possible the behavior
 * of Java's Double.toString(), which has no precision loss.  Implementors in
 * other languages should strive to achieve that where possible. I have not
 * yet verified whether boost:lexical_cast, which is doing that work for me in
 * C++, loses any precision, but I am leaving this as a future improvement. I
 * may try to provide a C component for this, so that other languages could
 * bind to the same underlying implementation for maximum consistency.
 *
 */
class TPlistProtocol : public TVirtualProtocol<TPlistProtocol> {
 public:

  TPlistProtocol(boost::shared_ptr<TTransport> ptrans);

  ~TPlistProtocol();

 private:

  void pushContext(boost::shared_ptr<TPlistContext> c);

  void popContext();

  uint32_t writePlistEscapeChar(uint8_t ch);

  uint32_t writePlistChar(uint8_t ch);

  uint32_t writePlistBool(bool b);

  uint32_t writePlistKey(const std::string &str);

  uint32_t writePlistString(const std::string &str);

  uint32_t writePlistBase64(const std::string &str);

  template <typename NumberType>
  uint32_t writePlistInteger(NumberType num);

  uint32_t writePlistDouble(double num);

  uint32_t writePlistObjectStart() ;

  uint32_t writePlistObjectEnd();

  uint32_t writePlistArrayStart();

  uint32_t writePlistArrayEnd();

  uint32_t readPlistSyntaxChar(uint8_t ch);

  uint32_t readPlistSyntaxString(std::string str);

  uint32_t peekPlistSyntaxString(std::string str);

  uint32_t readPlistEscapeChar(uint8_t *out);

  uint32_t readPlistString(std::string &str, bool skipContext = false);

  uint32_t readPlistBinary(std::string &str, bool skipContext = false);

  uint32_t readPlistKey(std::string &str, bool skipContext = false);

  uint32_t readPlistBase64(std::string &str);

  uint32_t readPlistNumericChars(std::string &str);

  template <typename NumberType>
  uint32_t readPlistInteger(NumberType &num);

  uint32_t readPlistBool(bool &b);

  uint32_t readPlistDouble(double &num);

  uint32_t readPlistObjectStart();

  uint32_t readPlistObjectEnd();

  uint32_t readPlistArrayStart();

  uint32_t readPlistArrayEnd();

 public:

  /**
   * Writing functions.
   */
  uint32_t writeMessageBegin(const std::string& name,
                             const TMessageType messageType,
                             const int32_t seqid);

  uint32_t writeMessageEnd();

  uint32_t writeStructBegin(const char* name);

  uint32_t writeStructEnd();

  uint32_t writeFieldBegin(const char* name,
                           const TType fieldType,
                           const int16_t fieldId);

  uint32_t writeFieldEnd();

  uint32_t writeFieldStop();

  uint32_t writeMapBegin(const TType keyType,
                         const TType valType,
                         const uint32_t size);

  uint32_t writeMapEnd();

  uint32_t writeListBegin(const TType elemType,
                          const uint32_t size);

  uint32_t writeListEnd();

  uint32_t writeSetBegin(const TType elemType,
                         const uint32_t size);

  uint32_t writeSetEnd();

  uint32_t writeBool(const bool value);

  uint32_t writeByte(const int8_t byte);

  uint32_t writeI16(const int16_t i16);

  uint32_t writeI32(const int32_t i32);

  uint32_t writeI64(const int64_t i64);

  uint32_t writeDouble(const double dub);

  uint32_t writeString(const std::string& str);

  uint32_t writeBinary(const std::string& str);

  /**
   * Reading functions
   */

  uint32_t readMessageBegin(std::string& name,
                            TMessageType& messageType,
                            int32_t& seqid);

  uint32_t readMessageEnd();

  uint32_t readStructBegin(std::string& name);

  uint32_t readStructEnd();

  uint32_t readFieldBegin(std::string& name,
                          TType& fieldType,
                          int16_t& fieldId);

  uint32_t readFieldEnd();

  uint32_t readMapBegin(TType& keyType,
                        TType& valType,
                        uint32_t& size);

  uint32_t readMapEnd();

  uint32_t readListBegin(TType& elemType,
                         uint32_t& size);

  uint32_t readListEnd();

  uint32_t readSetBegin(TType& elemType,
                        uint32_t& size);

  uint32_t readSetEnd();

  uint32_t readBool(bool& value);

  // Provide the default readBool() implementation for std::vector<bool>
  using TVirtualProtocol<TPlistProtocol>::readBool;

  uint32_t readByte(int8_t& byte);

  uint32_t readI16(int16_t& i16);

  uint32_t readI32(int32_t& i32);

  uint32_t readI64(int64_t& i64);

  uint32_t readDouble(double& dub);

  uint32_t readString(std::string& str);

  uint32_t readBinary(std::string& str);

  class LookaheadReader {

   public:

    LookaheadReader(TTransport &trans) :
      trans_(&trans) {
      buffer_.clear();
    }

    uint8_t read() {
      uint8_t data;
      if (buffer_.length() > 0) {
        data = buffer_[0];
        buffer_.erase(0, 1);
      }
      else {
        trans_->readAll(&data, 1);
      }
      return data;
    }

    uint8_t peek() {
      uint8_t data;
      if (buffer_.length() > 0) {
        data = buffer_[0];
      }
      else {
        trans_->readAll(&data, 1);
        buffer_.push_back(data);
      }
      return data;
    }

    std::string peek(uint32_t size) {
      if (buffer_.length() >= size) {
        return buffer_.substr(0, size);
      }
      else {
        uint32_t toread = size - buffer_.length();
        uint8_t *data = (uint8_t *)(new char(toread));
        try {
            trans_->readAll(data, toread);
        }
        catch(...) {
            return std::string("");
        }
        buffer_.append((const char*)data, toread);
        delete data;
        return buffer_;
      }
    }

    void consume(uint32_t size) {
      if (buffer_.length() >= size) {
        buffer_.erase(0, size);
      }
      else {
        uint32_t toread = size - buffer_.length();
        uint8_t *data = (uint8_t *)(new char(toread));
        buffer_.clear();
        trans_->readAll(data, toread);
        delete data;
      }
    }

   private:
    TTransport *trans_;
    std::string buffer_;
  };

 private:
  TTransport* trans_;

  std::stack<boost::shared_ptr<TPlistContext> > contexts_;
  boost::shared_ptr<TPlistContext> context_;
  LookaheadReader reader_;
};

/**
 * Constructs input and output protocol objects given transports.
 */
class TPlistProtocolFactory : public TProtocolFactory {
 public:
  TPlistProtocolFactory() {}

  virtual ~TPlistProtocolFactory() {}

  boost::shared_ptr<TProtocol> getProtocol(boost::shared_ptr<TTransport> trans) {
    return boost::shared_ptr<TProtocol>(new TPlistProtocol(trans));
  }
};

}}} // apache::thrift::protocol


// TODO(dreiss): Move part of ThriftPlistString into a .cpp file and remove this.
#include <transport/TBufferTransports.h>

namespace apache { namespace thrift {

template<typename ThriftStruct>
  std::string ThriftPlistString(const ThriftStruct& ts) {
  using namespace apache::thrift::transport;
  using namespace apache::thrift::protocol;
  TMemoryBuffer* buffer = new TMemoryBuffer;
  boost::shared_ptr<TTransport> trans(buffer);
  TPlistProtocol protocol(trans);

  ts.write(&protocol);

  uint8_t* buf;
  uint32_t size;
  buffer->getBuffer(&buf, &size);
  return std::string((char*)buf, (unsigned int)size);
}

}} // apache::thrift

#endif // #define _THRIFT_PROTOCOL_TPLISTPROTOCOL_H_ 1
