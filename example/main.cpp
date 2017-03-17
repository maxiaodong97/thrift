#include <iostream>
#include <fstream>
#include <string>
#include <streambuf>
#include <boost/shared_ptr.hpp>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TSimpleJSONProtocol.h>
#include <thrift/protocol/TPlistProtocol.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include "example_types.h"

using namespace std;
using namespace boost;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

int testSimpleJSONWrite(int argc, char **argv)
{
    cout<<"----- TestSimpleJSONWrite() -----"<<endl;
    shared_ptr<SimpleStruct> example(new SimpleStruct());
    shared_ptr<TMemoryBuffer> omemory(new TMemoryBuffer(1024));
    shared_ptr<TProtocol> oprotocol(new TSimpleJSONProtocol(omemory));
    example->__set_commonField1("Field1-value");
    example->commonField2.push_back("Field2-value1");
    example->commonField2.push_back("Field2-value2");
    example->__isset.commonField2 = true;
    example->__set_commonField3("hello,world");
    example->__set_commonField4(true);
    example->__set_commonField6(1.732);
    example->__set_commonField7(1732);
    example->write(oprotocol.get());
    string body = omemory->getBufferAsString();
    cout<<body<<endl;
    return 0;
}

int testSimpleJSONRead(int argc, char **argv)
{
    cout<<"----- TestSimpleJSONRead() -----"<<endl;
    uint8_t *body = (uint8_t*)
        "{\n"
        "       \"commonField1\":\"Field1\",\n"
        "       \"commonField2\":[\"Field2-value1\",\"Field2-value2\"],\n"
        "       \"commonField3\":\"aGVsbG8sd29ybGQ\",\n"
        "       \"commonField4\":true,\n"
        "       \"commonField6\":1.732,\n"
        "       \"commonField7\":1732\n"
        "}\n";
    cout<<body<<endl;
    shared_ptr<SimpleStruct> example(new SimpleStruct());
    shared_ptr<TMemoryBuffer> imemory(new TMemoryBuffer(body, 1024));
    shared_ptr<TProtocol> iprotocol(new TSimpleJSONProtocol(imemory));
    example->read(iprotocol.get());
    cout<<example->commonField1<<endl;
    cout<<example->commonField2[0]<<endl;
    cout<<example->commonField2[1]<<endl;
    cout<<example->commonField3<<endl;
    cout<<example->commonField4<<endl;
    cout<<example->commonField6<<endl;
    cout<<example->commonField7<<endl;
    return 0;
}

void testPlistWrite(int argc, char **argv)
{
    cout<<"----- TestPlistWrite() -----"<<endl;
    shared_ptr<SimpleStruct> example(new SimpleStruct());
    shared_ptr<TMemoryBuffer> omemory(new TMemoryBuffer(1024));
    shared_ptr<TProtocol> oprotocol(new TPlistProtocol(omemory));
    example->__set_commonField1("Field1-\"\'<>&");
    example->commonField2.push_back("Field2-value1");
    example->commonField2.push_back("Field2-value2");
    example->__isset.commonField2 = true;
    example->__set_commonField3("hello,world");
    example->__set_commonField4(true);
    example->__set_commonField6(1.732);
    example->__set_commonField7(1732);
    example->__set_common_field8(32);
    example->write(oprotocol.get());
    string body = omemory->getBufferAsString();
    cout<<body<<endl;
}

void testPlistRead(int argc, char **argv)
{
    cout<<"----- TestPlistRead() -----"<<endl;
    uint8_t *body = (uint8_t *)
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>             \n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">    \n"
        "<plist version=\"1.0\">                                \n"
        "<dict>                                                 \n"
        "  <key>commonField1</key>                     \n"
        "  <string>Field1-&quot;&apos;&lt;&gt;&amp;</string>\n"
        "  <key>commonField2</key>                     \n"
        "  <array>                                     \n"
        "      <string>Field2-value1</string>          \n"
        "      <string>Field2-value2</string>          \n"
        "  </array>                                    \n"
        "  <key>commonField3</key>                     \n"
        "  <data>aGVsbG8sd29ybGQ</data>                \n"
        "  <key>commonField4</key>                     \n"
        "  <true/>                                     \n"
        "  <key>commonField6</key>                     \n"
        "  <real>1.732</real>                          \n"
        "  <key>commonField7</key>                     \n"
        "  <integer>1732</integer>                     \n"
        "  <key>common-field8</key>                    \n"
        "  <integer>32</integer>                       \n"
        "</dict>                                                \n"
        "</plist>                                               \n";
    cout << body << endl;
    shared_ptr<SimpleStruct> example(new SimpleStruct());
    shared_ptr<TMemoryBuffer> imemory(new TMemoryBuffer(body, 1024 * 40));
    shared_ptr<TProtocol> iprotocol(new TPlistProtocol(imemory));
    example->read(iprotocol.get());
    cout<<example->commonField1<<endl;
    cout<<example->commonField2[0]<<endl;
    cout<<example->commonField2[1]<<endl;
    cout<<example->commonField3<<endl;
    cout<<example->commonField4<<endl;
    cout<<example->commonField6<<endl;
    cout<<example->commonField7<<endl;
    cout<<(int)example->common_field8<<endl;
}

int main(int argc, char **argv)
{
    testSimpleJSONWrite(argc, argv);
    testSimpleJSONRead(argc, argv);
    testPlistWrite(argc, argv);
    testPlistRead(argc, argv);
    return 0;
}
