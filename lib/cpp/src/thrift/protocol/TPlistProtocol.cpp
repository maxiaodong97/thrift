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

#include "TPlistProtocol.h"

#include <math.h>
#include <boost/lexical_cast.hpp>
#include "TBase64Utils.h"
#include <transport/TTransportException.h>

using namespace apache::thrift::transport;

namespace apache { namespace thrift { namespace protocol {


// Static data

static const std::string kPlistObjectStart("<dict>");
static const std::string kPlistObjectEnd("</dict>");
static const std::string kPlistArrayStart("<array>");
static const std::string kPlistArrayEnd("</array>");
static const std::string kPlistKeyStart("<key>");
static const std::string kPlistKeyEnd("</key>");
static const std::string kPlistStringStart("<string>");
static const std::string kPlistStringEnd("</string>");
static const std::string kPlistBinaryStart("<data>");
static const std::string kPlistBinaryEnd("</data>");
static const std::string kPlistIntegerStart("<integer>");
static const std::string kPlistIntegerEnd("</integer>");
static const std::string kPlistRealStart("<real>");
static const std::string kPlistRealEnd("</real>");
static const std::string kPlistPlistStart("<plist version=\"1.0\">");
static const std::string kPlistPlistEnd("</plist>");
static const std::string kPlistHeader("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
static const uint8_t kPlistNewline = '\n';
static const uint8_t kPlistSpace = ' ';
static const uint8_t kPlistOpenTag = '<';
static const uint8_t kPlistCloseTag = '>';
static const uint8_t kPlistPairSeparator = ' ';
static const uint8_t kPlistElemSeparator = ' ';
static const uint8_t kPlistBackslash = '\\';
static const uint8_t kPlistStringDelimiter = '"';
static const uint8_t kPlistZeroChar = '0';
static const uint8_t kPlistEscapeChar = 'u';
static const uint8_t kPlistTrueChar = 't';
static const uint8_t kPlistFalseChar = 'f';
static const std::string kPlistStringTrue("<true/>");
static const std::string kPlistStringFalse("<false/>");
static const std::string kPlistEscapePrefix("\\u00");

static const uint32_t kThriftVersion1 = 1;

static const std::string kThriftNan("NaN");
static const std::string kThriftInfinity("Infinity");
static const std::string kThriftNegativeInfinity("-Infinity");

static const std::string kTypeNameBool("tf");
static const std::string kTypeNameByte("i8");
static const std::string kTypeNameI16("i16");
static const std::string kTypeNameI32("i32");
static const std::string kTypeNameI64("i64");
static const std::string kTypeNameDouble("dbl");
static const std::string kTypeNameStruct("rec");
static const std::string kTypeNameString("str");
static const std::string kTypeNameMap("map");
static const std::string kTypeNameList("lst");
static const std::string kTypeNameSet("set");

// This string's characters must match up with the elements in kEscapeCharVals.
// I don't have '/' on this list even though it appears on www.json.org --
// it is not in the RFC
const static std::string kEscapeChars("\"\'><&");

// Static helper functions

// Read 1 character from the transport trans and verify that it is the
// expected character ch.
// Throw a protocol exception if it is not.
static uint32_t readSyntaxChar(TPlistProtocol::LookaheadReader &reader,
                               uint8_t ch) {
  uint8_t ch2 = reader.peek();
  uint32_t n = 0;
  while (ch2 == kPlistSpace || ch2 == kPlistNewline) {
    reader.read(); // skip the space or new line.
    n++;
    ch2 = reader.peek();
  }
  if (ch2 != ch) {
    throw TProtocolException(TProtocolException::INVALID_DATA,
                             "Expected \'" + std::string((char *)&ch, 1) +
                             "\'; got \'" + std::string((char *)&ch2, 1) +
                             "\'.");
  }
  reader.read();
  n++;
  return n;
}

// Static helper functions

// Read string from the transport trans and verify that it is the
// expected string.
// Throw a protocol exception if it is not.
static uint32_t readSyntaxString(TPlistProtocol::LookaheadReader &reader,
                               const std::string &str)
{
  std::string str2="";
  uint32_t n = str.length();
  uint32_t result = 0;
  uint8_t ch2 = reader.peek();
  while (ch2 == kPlistSpace || ch2 == kPlistNewline) {
    reader.read(); // skip the space or new line.
    result++;
    ch2 = reader.peek();
  }
  str2 = reader.peek(n);
  if (str2 != str) {
    throw TProtocolException(TProtocolException::INVALID_DATA,
                             "Expected \'" + str + "\'; got \'" +
                             str2 + "\'.");
  }
  else {
    result += n;
    reader.consume(n);
  }
  return result;
}

// Return the integer value of a hex character ch.
// Throw a protocol exception if the character is not [0-9a-f].
static uint8_t hexVal(uint8_t ch) {
  if ((ch >= '0') && (ch <= '9')) {
    return ch - '0';
  }
  else if ((ch >= 'a') && (ch <= 'f')) {
    return ch - 'a' + 10;
  }
  else {
    throw TProtocolException(TProtocolException::INVALID_DATA,
                             "Expected hex val ([0-9a-f]); got \'"
                               + std::string((char *)&ch, 1) + "\'.");
  }
}

// Return the hex character representing the integer val. The value is masked
// to make sure it is in the correct range.
static uint8_t hexChar(uint8_t val) {
  val &= 0x0F;
  if (val < 10) {
    return val + '0';
  }
  else {
    return val - 10 + 'a';
  }
}

// Return true if the character ch is in [-+0-9.Ee]; false otherwise
static bool isPlistNumeric(uint8_t ch) {
  switch (ch) {
  case '+':
  case '-':
  case '.':
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
  case 'E':
  case 'e':
    return true;
  }
  return false;
}


/**
 * Class to serve as base Plist context and as base class for other context
 * implementations
 */
class TPlistContext {

 public:

  TPlistContext() {};

  virtual ~TPlistContext() {};

  /**
   * Write context data to the transport. Default is to do nothing.
   */
  virtual uint32_t write(TTransport &trans) {
    (void) trans;
    return 0;
  };

  /**
   * Read context data from the transport. Default is to do nothing.
   */
  virtual uint32_t read(TPlistProtocol::LookaheadReader &reader) {
    (void) reader;
    return 0;
  };

  /**
   * Return true if numbers need to be escaped as strings in this context.
   * Default behavior is to return false.
   */
  virtual bool escapeNum() {
    return false;
  }
};

// Context class for object member key-value pairs
class PlistPairContext : public TPlistContext {

public:

  PlistPairContext() :
    first_(true),
    colon_(true) {
  }

  uint32_t write(TTransport &trans) {
    if (first_) {
      first_ = false;
      colon_ = true;
      return 0;
    }
    else {
      trans.write(colon_ ? &kPlistPairSeparator : &kPlistElemSeparator, 1);
      colon_ = !colon_;
      return 1;
    }
  }

  uint32_t read(TPlistProtocol::LookaheadReader &reader) {
    if (first_) {
      first_ = false;
      colon_ = true;
      return 0;
    }
    else {
      uint8_t ch = (colon_ ? kPlistPairSeparator : kPlistElemSeparator);
      colon_ = !colon_;
      return 0;
      //readSyntaxChar(reader, ch);
    }
  }

  // Numbers must be turned into strings if they are the key part of a pair
  virtual bool escapeNum() {
    return colon_;
  }

  private:

    bool first_;
    bool colon_;
};

// Context class for lists
class PlistListContext : public TPlistContext {

public:

  PlistListContext() :
    first_(true) {
  }

  uint32_t write(TTransport &trans) {
    if (first_) {
      first_ = false;
      return 0;
    }
    else {
      trans.write(&kPlistElemSeparator, 1);
      return 1;
    }
  }

  uint32_t read(TPlistProtocol::LookaheadReader &reader) {
    if (first_) {
      first_ = false;
      return 0;
    }
    else {
      return 0;
      //readSyntaxChar(reader, kPlistElemSeparator);
    }
  }

  private:
    bool first_;
};


TPlistProtocol::TPlistProtocol(boost::shared_ptr<TTransport> ptrans) :
  TVirtualProtocol<TPlistProtocol>(ptrans),
  trans_(ptrans.get()),
  context_(new TPlistContext()),
  reader_(*ptrans) {
}

TPlistProtocol::~TPlistProtocol() {}

void TPlistProtocol::pushContext(boost::shared_ptr<TPlistContext> c) {
  contexts_.push(context_);
  context_ = c;
}

void TPlistProtocol::popContext() {
  context_ = contexts_.top();
  contexts_.pop();
}

// Write the character ch as a Plist escape sequence ("\u00xx")
uint32_t TPlistProtocol::writePlistEscapeChar(uint8_t ch) {
  switch(ch) {
  case '\'':
    trans_->write((const uint8_t*)"&apos;", 6);
    return 6;
  case '\"':
    trans_->write((const uint8_t*)"&quot;", 6);
    return 6;
  case '>':
    trans_->write((const uint8_t*)"&gt;", 4);
    return 4;
  case '<':
    trans_->write((const uint8_t*)"&lt;", 4);
    return 4;
  case '&':
    trans_->write((const uint8_t*)"&amp;", 5);
    return 5;
  }
  return 0;
}

// Write the character ch as part of a Plist string, escaping as appropriate.
uint32_t TPlistProtocol::writePlistChar(uint8_t ch) {
  if (kEscapeChars.find(ch) == std::string::npos) {
    trans_->write(&ch, 1);
    return 1;
  }
  return writePlistEscapeChar(ch);
}

uint32_t TPlistProtocol::writePlistKey(const std::string &str) {
  uint32_t result = context_->write(*trans_);
  result += kPlistKeyStart.length() + kPlistKeyEnd.length();
  trans_->write((uint8_t*)kPlistKeyStart.c_str(), kPlistKeyStart.length());
  std::string::const_iterator iter(str.begin());
  std::string::const_iterator end(str.end());
  while (iter != end) {
    if (*iter == '_') {
      result += writePlistChar('-');
      iter++;
    }
    else {
      result += writePlistChar(*iter++);
    }
  }
  trans_->write((uint8_t*)kPlistKeyEnd.c_str(), kPlistKeyEnd.length());
  return result;
}

// Write out the contents of the string str as a Plist string, escaping
// characters as appropriate.
uint32_t TPlistProtocol::writePlistString(const std::string &str) {
  uint32_t result = context_->write(*trans_);
  result += kPlistStringStart.length() + kPlistStringEnd.length();
  trans_->write((uint8_t*)kPlistStringStart.c_str(), kPlistStringStart.length());
  std::string::const_iterator iter(str.begin());
  std::string::const_iterator end(str.end());
  while (iter != end) {
    result += writePlistChar(*iter++);
  }
  trans_->write((uint8_t*)kPlistStringEnd.c_str(), kPlistStringEnd.length());
  return result;
}

uint32_t TPlistProtocol::writePlistBool(bool b) {
  uint32_t result = context_->write(*trans_);
  if (b) {
    trans_->write((const uint8_t *)kPlistStringTrue.c_str(),
            kPlistStringTrue.length());
    result += kPlistStringTrue.length();
  }
  else {
    trans_->write((const uint8_t *)kPlistStringFalse.c_str(),
            kPlistStringFalse.length());
    result += kPlistStringFalse.length();
  }
  return result;
}

// Write out the contents of the string as Plist string, base64-encoding
// the string's contents, and escaping as appropriate
uint32_t TPlistProtocol::writePlistBase64(const std::string &str) {
  uint32_t result = context_->write(*trans_);
  result += kPlistBinaryStart.length() + kPlistBinaryEnd.length();
  trans_->write((uint8_t*)kPlistBinaryStart.c_str(), kPlistBinaryStart.length());
  uint8_t b[4];
  const uint8_t *bytes = (const uint8_t *)str.c_str();
  uint32_t len = str.length();
  while (len >= 3) {
    // Encode 3 bytes at a time
    base64_encode(bytes, 3, b);
    trans_->write(b, 4);
    result += 4;
    bytes += 3;
    len -=3;
  }
  if (len) { // Handle remainder
    base64_encode(bytes, len, b);
    trans_->write(b, len + 1);
    result += len + 1;
  }
  trans_->write((uint8_t*)kPlistBinaryEnd.c_str(), kPlistBinaryEnd.length());
  return result;
}

// Convert the given integer type to a Plist number, or a string
// if the context requires it (eg: key in a map pair).
template <typename NumberType>
uint32_t TPlistProtocol::writePlistInteger(NumberType num) {
  uint32_t result = context_->write(*trans_);
  std::string val(boost::lexical_cast<std::string>(num));
  trans_->write((uint8_t*)kPlistIntegerStart.c_str(), kPlistIntegerStart.length());
  result += kPlistIntegerStart.length();
  trans_->write((const uint8_t *)val.c_str(), val.length());
  result += val.length();
  trans_->write((uint8_t*)kPlistIntegerEnd.c_str(), kPlistIntegerEnd.length());
  result += kPlistIntegerEnd.length();
  return result;
}

// Convert the given double to a Plist string, which is either the number,
// "NaN" or "Infinity" or "-Infinity".
uint32_t TPlistProtocol::writePlistDouble(double num) {
  uint32_t result = context_->write(*trans_);
  std::string val(boost::lexical_cast<std::string>(num));

  // Normalize output of boost::lexical_cast for NaNs and Infinities
  switch (val[0]) {
  case 'N':
  case 'n':
    val = kThriftNan;
    break;
  case 'I':
  case 'i':
    val = kThriftInfinity;
    break;
  case '-':
    if ((val[1] == 'I') || (val[1] == 'i')) {
      val = kThriftNegativeInfinity;
    }
    break;
  }

  trans_->write((uint8_t*)kPlistRealStart.c_str(), kPlistRealStart.length());
  result += kPlistRealStart.length();
  trans_->write((const uint8_t *)val.c_str(), val.length());
  result += val.length();
  trans_->write((uint8_t*)kPlistRealEnd.c_str(), kPlistRealEnd.length());
  result += kPlistRealEnd.length();
  return result;
}

uint32_t TPlistProtocol::writePlistObjectStart() {
  uint32_t result = context_->write(*trans_);
  trans_->write((uint8_t*)kPlistObjectStart.c_str(), kPlistObjectStart.length());
  pushContext(boost::shared_ptr<TPlistContext>(new PlistPairContext()));
  return result + kPlistObjectStart.length();
}

uint32_t TPlistProtocol::writePlistObjectEnd() {
  uint32_t result = context_->write(*trans_);
  popContext();
  trans_->write((uint8_t*)kPlistObjectEnd.c_str(), kPlistObjectEnd.length());
  result += kPlistObjectEnd.length();
  if (contexts_.empty()) {
    trans_->write((uint8_t*)kPlistPlistEnd.c_str(), kPlistPlistEnd.length());
    result += kPlistPlistEnd.length();
  }
  return result;
}

uint32_t TPlistProtocol::writePlistArrayStart() {
  uint32_t result = context_->write(*trans_);
  trans_->write((uint8_t*)kPlistArrayStart.c_str(), kPlistArrayStart.length());
  result += kPlistArrayStart.length();
  pushContext(boost::shared_ptr<TPlistContext>(new PlistListContext()));
  return result;
}

uint32_t TPlistProtocol::writePlistArrayEnd() {
  popContext();
  trans_->write((uint8_t*)kPlistArrayEnd.c_str(), kPlistArrayEnd.length());
  return kPlistArrayEnd.length();
}

uint32_t TPlistProtocol::writeMessageBegin(const std::string& name,
                                          const TMessageType messageType,
                                          const int32_t seqid) {
  uint32_t result = writePlistArrayStart();
  result += writePlistInteger(kThriftVersion1);
  result += writePlistString(name);
  result += writePlistInteger(messageType);
  result += writePlistInteger(seqid);
  return result;
}

uint32_t TPlistProtocol::writeMessageEnd() {
  return writePlistArrayEnd();
}

uint32_t TPlistProtocol::writeStructBegin(const char* name) {
  (void) name;
  uint32_t result = 0;
  if (contexts_.empty()) {
    result = context_->write(*trans_);
    trans_->write((uint8_t*)kPlistHeader.c_str(), kPlistHeader.length());
    result += kPlistHeader.length();
    trans_->write((uint8_t*)kPlistPlistStart.c_str(), kPlistPlistStart.length());
    result += kPlistPlistStart.length();
  }
  result += writePlistObjectStart();
  return result;
}

uint32_t TPlistProtocol::writeStructEnd() {
  return writePlistObjectEnd();
}

uint32_t TPlistProtocol::writeFieldBegin(const char* name,
                                        const TType fieldType,
                                        const int16_t fieldId) {
  //(void) name;
  //uint32_t result = writePlistInteger(fieldId);
  //result += writePlistObjectStart();
  uint32_t result = writePlistKey(name);
  //result += writePlistString(getTypeNameForTypeID(fieldType));
  return result;
}

uint32_t TPlistProtocol::writeFieldEnd() {
//  return writePlistObjectEnd();
    return 0;
}

uint32_t TPlistProtocol::writeFieldStop() {
  return 0;
}

uint32_t TPlistProtocol::writeMapBegin(const TType keyType,
                                      const TType valType,
                                      const uint32_t size) {
  uint32_t result = writePlistArrayStart();
  //result += writePlistString(getTypeNameForTypeID(keyType));
  //result += writePlistString(getTypeNameForTypeID(valType));
  //result += writePlistInteger((int64_t)size);
  result += writePlistObjectStart();
  return result;
}

uint32_t TPlistProtocol::writeMapEnd() {
  return writePlistObjectEnd() + writePlistArrayEnd();
}

uint32_t TPlistProtocol::writeListBegin(const TType elemType,
                                       const uint32_t size) {
  uint32_t result = writePlistArrayStart();
  //result += writePlistString(getTypeNameForTypeID(elemType));
  //result += writePlistInteger((int64_t)size);
  return result;
}

uint32_t TPlistProtocol::writeListEnd() {
  return writePlistArrayEnd();
}

uint32_t TPlistProtocol::writeSetBegin(const TType elemType,
                                      const uint32_t size) {
  uint32_t result = writePlistArrayStart();
  //result += writePlistString(getTypeNameForTypeID(elemType));
  //result += writePlistInteger((int64_t)size);
  return result;
}

uint32_t TPlistProtocol::writeSetEnd() {
  return writePlistArrayEnd();
}

uint32_t TPlistProtocol::writeBool(const bool value) {
  return writePlistBool(value);
}

uint32_t TPlistProtocol::writeByte(const int8_t byte) {
  // writeByte() must be handled specially becuase boost::lexical cast sees
  // int8_t as a text type instead of an integer type
  return writePlistInteger((int16_t)byte);
}

uint32_t TPlistProtocol::writeI16(const int16_t i16) {
  return writePlistInteger(i16);
}

uint32_t TPlistProtocol::writeI32(const int32_t i32) {
  return writePlistInteger(i32);
}

uint32_t TPlistProtocol::writeI64(const int64_t i64) {
  return writePlistInteger(i64);
}

uint32_t TPlistProtocol::writeDouble(const double dub) {
  return writePlistDouble(dub);
}

uint32_t TPlistProtocol::writeString(const std::string& str) {
  return writePlistString(str);
}

uint32_t TPlistProtocol::writeBinary(const std::string& str) {
  return writePlistBase64(str);
}

  /**
   * Reading functions
   */

// Reads 1 byte and verifies that it matches ch.
uint32_t TPlistProtocol::readPlistSyntaxChar(uint8_t ch) {
  return readSyntaxChar(reader_, ch);
}

// Reads string and verifies that it matches str.
uint32_t TPlistProtocol::readPlistSyntaxString(std::string str) {
  return readSyntaxString(reader_, str);
}


uint32_t TPlistProtocol::readPlistEscapeChar(uint8_t *out) {
  *out = reader_.read();
  return 1;
}

// Decodes a Plist string, including unescaping, and returns the string via str
uint32_t TPlistProtocol::readPlistBinary(std::string &str, bool skipContext) {
  uint32_t result = (skipContext ? 0 : context_->read(reader_));
  result += readPlistSyntaxString(kPlistBinaryStart);
  uint8_t ch;
  str.clear();
  while (true) {
    ch = reader_.peek();
    if (ch == kPlistOpenTag) {
        break;
    }
    str += ch;
    ++result;
    reader_.read();
  }
  result += readPlistSyntaxString(kPlistBinaryEnd);
  return result;
}

// Decodes a Plist string, including unescaping, and returns the string via str
uint32_t TPlistProtocol::readPlistString(std::string &str, bool skipContext) {
  uint32_t result = (skipContext ? 0 : context_->read(reader_));
  result += readPlistSyntaxString(kPlistStringStart);
  uint8_t ch;
  str.clear();
  while (true) {
    ch = reader_.peek();
    if (ch == kPlistOpenTag) {
      break;
    }
    if (ch == '&') {
      uint8_t escape = 1;
      std::string s4 = reader_.peek(4);
      std::string s5 = reader_.peek(5);
      std::string s6 = reader_.peek(6);
      if (s4 == "&lt;") {
        str += "<";
        escape = 4;
      }
      else if (s4 == "&gt;") {
        str += ">";
        escape = 4;
      }
      else if (s5 == "&amp;"){
        str += "&";
        escape = 5;
      }
      else if (s6 == "&apos;") {
        str += "\'";
        escape = 6;
      }
      else if (s6 == "&quot;") {
        str += "\"";
        escape = 6;
      }
      while(escape--) {
        ++result;
        reader_.read();
      }
    }
    else {
      str += ch;
      ++result;
      reader_.read();
    }
  }
  result += readPlistSyntaxString(kPlistStringEnd);
  return result;
}

// Decodes a Plist key, including unescaping, and returns the string via str
uint32_t TPlistProtocol::readPlistKey(std::string &str, bool skipContext) {
  uint32_t result = (skipContext ? 0 : context_->read(reader_));
  result += readPlistSyntaxString(kPlistKeyStart);
  uint8_t ch;
  str.clear();
  while (true) {
    ch = reader_.peek();
    if (ch == kPlistOpenTag) {
        break;
    }
    str += ch == '-'?'_':ch;
    ++result;
    reader_.read();
  }
  result += readPlistSyntaxString(kPlistKeyEnd);
  return result;
}

uint32_t TPlistProtocol::readPlistBool(bool &b) {
  uint32_t result = context_->read(reader_);
  std::string str = "";
  uint8_t ch;
  while (true) {
    ch = reader_.read();
    ++result;
    if (ch == kPlistSpace || ch == kPlistNewline) {
      continue;
    }
    str += ch;
    if (ch == kPlistCloseTag){
      break;
    }
  }

  if (str == kPlistStringTrue) {
    b = true;
  }
  else if (str == kPlistStringFalse) {
    b = false;
  }
  else {
    throw TProtocolException(TProtocolException::INVALID_DATA,
            "Expected '</true>' or '</false>' got '" + str + "'.");
  }
  return result;
}


// Reads a block of base64 characters, decoding it, and returns via str
uint32_t TPlistProtocol::readPlistBase64(std::string &str) {
  std::string tmp;
  uint32_t result = readPlistBinary(tmp);
  uint8_t *b = (uint8_t *)tmp.c_str();
  uint32_t len = tmp.length();
  str.clear();
  while (len >= 4) {
    base64_decode(b, 4);
    str.append((const char *)b, 3);
    b += 4;
    len -= 4;
  }
  // Don't decode if we hit the end or got a single leftover byte (invalid
  // base64 but legal for skip of regular string type)
  if (len > 1) {
    base64_decode(b, len);
    str.append((const char *)b, len - 1);
  }
  return result;
}

// Reads a sequence of characters, stopping at the first one that is not
// a valid Plist numeric character.
uint32_t TPlistProtocol::readPlistNumericChars(std::string &str) {
  uint32_t result = 0;
  str.clear();
  while (true) {
    uint8_t ch = reader_.peek();
    if (!isPlistNumeric(ch)) {
      break;
    }
    reader_.read();
    str += ch;
    ++result;
  }
  return result;
}

// Reads a sequence of characters and assembles them into a number,
// returning them via num
template <typename NumberType>
uint32_t TPlistProtocol::readPlistInteger(NumberType &num) {
  uint32_t result = context_->read(reader_);
  result += readPlistSyntaxString(kPlistIntegerStart);
  std::string str;
  result += readPlistNumericChars(str);
  try {
    num = boost::lexical_cast<NumberType>(str);
  }
  catch (boost::bad_lexical_cast e) {
    throw new TProtocolException(TProtocolException::INVALID_DATA,
                                 "Expected numeric value; got \"" + str +
                                  "\"");
  }
  result += readPlistSyntaxString(kPlistIntegerEnd);
  return result;
}

// Reads a Plist number or string and interprets it as a double.
uint32_t TPlistProtocol::readPlistDouble(double &num) {
  uint32_t result = context_->read(reader_);
  result += readPlistSyntaxString(kPlistRealStart);
  std::string str;
  while (true) {
    uint8_t ch = reader_.peek();
    if (ch == kPlistOpenTag){
      break;
    }
    reader_.read();
    str += ch;
    ++result;
  }
  // Check for NaN, Infinity and -Infinity
  if (str == kThriftNan) {
    num = HUGE_VAL/HUGE_VAL; // generates NaN
  }
  else if (str == kThriftInfinity) {
    num = HUGE_VAL;
  }
  else if (str == kThriftNegativeInfinity) {
    num = -HUGE_VAL;
  }
  else {
    try {
      num = boost::lexical_cast<double>(str);
    }
    catch (boost::bad_lexical_cast e) {
      throw new TProtocolException(TProtocolException::INVALID_DATA,
                                   "Expected numeric value; got \"" + str +
                                   "\"");
    }
  }
  result += readPlistSyntaxString(kPlistRealEnd);
  return result;
}

uint32_t TPlistProtocol::readPlistObjectStart() {
  uint32_t result = context_->read(reader_);
  result += readPlistSyntaxString(kPlistObjectStart);
  pushContext(boost::shared_ptr<TPlistContext>(new PlistPairContext()));
  return result;
}

uint32_t TPlistProtocol::readPlistObjectEnd() {
  uint32_t result = readPlistSyntaxString(kPlistObjectEnd);
  popContext();
  if (contexts_.empty()) {
    result += readPlistSyntaxString(kPlistPlistEnd);
  }
  return result;
}

uint32_t TPlistProtocol::readPlistArrayStart() {
  uint32_t result = context_->read(reader_);
  result += readPlistSyntaxString(kPlistArrayStart);
  pushContext(boost::shared_ptr<TPlistContext>(new PlistListContext()));
  return result;
}

uint32_t TPlistProtocol::readPlistArrayEnd() {
  uint32_t result = readPlistSyntaxString(kPlistArrayEnd);
  popContext();
  return result;
}

uint32_t TPlistProtocol::readMessageBegin(std::string& name,
                                         TMessageType& messageType,
                                         int32_t& seqid) {
  uint32_t result = readPlistArrayStart();
  uint64_t tmpVal = 0;
  result += readPlistInteger(tmpVal);
  if (tmpVal != kThriftVersion1) {
    throw TProtocolException(TProtocolException::BAD_VERSION,
                             "Message contained bad version.");
  }
  result += readPlistString(name);
  result += readPlistInteger(tmpVal);
  messageType = (TMessageType)tmpVal;
  result += readPlistInteger(tmpVal);
  seqid = tmpVal;
  return result;
}

uint32_t TPlistProtocol::readMessageEnd() {
  return readPlistArrayEnd();
}

uint32_t TPlistProtocol::readStructBegin(std::string& name) {
  (void) name;
  uint32_t result = 0;
  if (contexts_.empty()) {
    // skip first three tags of <?xml>, <!DOC> and <plist>
    uint8_t ch;
    while((ch = reader_.read()) != kPlistCloseTag) result++;
    while((ch = reader_.read()) != kPlistCloseTag) result++;
    while((ch = reader_.read()) != kPlistCloseTag) result++;
  }
  result += readPlistObjectStart();
  return result;
}

uint32_t TPlistProtocol::readStructEnd() {
  return readPlistObjectEnd();
}

uint32_t TPlistProtocol::readFieldBegin(std::string& name,
                                       TType& fieldType,
                                       int16_t& fieldId) {
  //(void) name;
  uint32_t result = 0;
  // Check if we hit the end of the list
  fieldType = T_VOID;
  uint8_t ch = reader_.peek();
  while (true) {
    if (ch == kPlistSpace || ch == kPlistNewline) {
      reader_.read();
      ++result;
      ch = reader_.peek();
      continue;
    }
    break;
  }
  std::string str = reader_.peek(kPlistObjectEnd.length());
  if (str == kPlistObjectEnd) {
    fieldType = apache::thrift::protocol::T_STOP;
  }
  else {
    //uint64_t tmpVal = 0;
    //std::string tmpStr;
    //result += readPlistInteger(tmpVal);
    //fieldId = tmpVal;
    //result += readPlistObjectStart();
    //result += readPlistString(tmpStr);
    result += readPlistKey(name);
    //fieldType = getTypeIDForTypeName(tmpStr);
    fieldId = (int16_t)-1;
  }
  return result;
}

uint32_t TPlistProtocol::readFieldEnd() {
  //return readPlistObjectEnd();
    return 0;
}

uint32_t TPlistProtocol::readMapBegin(TType& keyType,
                                     TType& valType,
                                     uint32_t& size) {
  //uint64_t tmpVal = 0;
  //std::string tmpStr;
  uint32_t result = readPlistArrayStart();
  //result += readPlistString(tmpStr);
  //keyType = getTypeIDForTypeName(tmpStr);
  //result += readPlistString(tmpStr);
  //valType = getTypeIDForTypeName(tmpStr);
  //result += readPlistInteger(tmpVal);
  //size = tmpVal;
  result += readPlistObjectStart();
  size = (uint32_t)-1;
  return result;
}

uint32_t TPlistProtocol::readMapEnd() {
  return readPlistObjectEnd() + readPlistArrayEnd();
}

uint32_t TPlistProtocol::readListBegin(TType& elemType,
                                      uint32_t& size) {
  (void) elemType;
  //uint64_t tmpVal = 0;
  //std::string tmpStr;
  uint32_t result = readPlistArrayStart();
  //result += readPlistString(tmpStr);
  //elemType = getTypeIDForTypeName(tmpStr);
  //result += readPlistInteger(tmpVal);
  //size = tmpVal;
  size = (uint32_t)-1;
  return result;
}

uint32_t TPlistProtocol::readListEnd() {
  return readPlistArrayEnd();
}

uint32_t TPlistProtocol::readSetBegin(TType& elemType,
                                     uint32_t& size) {
  (void) elemType;
  //uint64_t tmpVal = 0;
  //std::string tmpStr;
  uint32_t result = readPlistArrayStart();
  //result += readPlistString(tmpStr);
  //elemType = getTypeIDForTypeName(tmpStr);
  //result += readPlistInteger(tmpVal);
  //size = tmpVal;
  size = (uint32_t)-1;
  return result;
}

uint32_t TPlistProtocol::readSetEnd() {
  return readPlistArrayEnd();
}

uint32_t TPlistProtocol::readBool(bool& value) {
  return readPlistBool(value);
}

// readByte() must be handled properly becuase boost::lexical cast sees int8_t
// as a text type instead of an integer type
uint32_t TPlistProtocol::readByte(int8_t& byte) {
  int16_t tmp = (int16_t) byte;
  uint32_t result =  readPlistInteger(tmp);
  assert(tmp < 256);
  byte = (int8_t)tmp;
  return result;
}

uint32_t TPlistProtocol::readI16(int16_t& i16) {
  return readPlistInteger(i16);
}

uint32_t TPlistProtocol::readI32(int32_t& i32) {
  return readPlistInteger(i32);
}

uint32_t TPlistProtocol::readI64(int64_t& i64) {
  return readPlistInteger(i64);
}

uint32_t TPlistProtocol::readDouble(double& dub) {
  return readPlistDouble(dub);
}

uint32_t TPlistProtocol::readString(std::string &str) {
  return readPlistString(str);
}

uint32_t TPlistProtocol::readBinary(std::string &str) {
  return readPlistBase64(str);
}

}}} // apache::thrift::protocol
