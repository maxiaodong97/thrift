Cpp TSimpleJSONProtocl and TPlistProtocol Support for Apache Thrift
===============================================================

This fork includes following fixes and enhancement for CPP and C

1. Support TSimpleJSONProtocol
SimpleJSON is more commonly used by outside of Apache Thrift.
Java already have this support, here I added for CPP.

2. TPlistProtocol
Plist is Apple's configuration file format, eg: .mobileconfig.
This feature can be useful for MDM vendors to generate profiles for iOS, MacOS, etc.

3. Fix issue where c glib binary protocol may goes to infinite loop in data fuzzy testing.


Examples
============

1. Follow build instruction in README.md.orig
2. Build exampe/
   cd example
   make
3. Run example
   ./example.exe

Here is example output:
```
----- TestSimpleJSONWrite() -----
{"commonField1":"Field1-value","commonField2":["Field2-value1","Field2-value2"],"commonField3":"aGVsbG8sd29ybGQ","commonField4":true,"commonField6":1.732,"commonField7":1732}
----- TestSimpleJSONRead() -----
{
        "commonField1":"Field1",
        "commonField2":["Field2-value1","Field2-value2"],
        "commonField3":"aGVsbG8sd29ybGQ",
        "commonField4":true,
        "commonField6":1.732,
        "commonField7":1732
}

Field1
Field2-value1
Field2-value2
hello,world
1
1.732
1732
----- TestPlistWrite() -----
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict><key>commonField1</key> <string>Field1-&quot;&apos;&lt;&gt;&amp;</string> <key>commonField2</key> <array><string>Field2-value1</string> <string>Field2-value2</string></array> <key>commonField3</key> <data>aGVsbG8sd29ybGQ</data> <key>commonField4</key> <true/> <key>commonField6</key> <real>1.732</real> <key>commonField7</key> <integer>1732</integer> <key>common-field8</key> <integer>32</integer> </dict></plist>
----- TestPlistRead() -----
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
<key>commonField1</key>
<string>Field1-&quot;&apos;&lt;&gt;&amp;</string>
<key>commonField2</key>
<array>
<string>Field2-value1</string>
<string>Field2-value2</string>
</array>
<key>commonField3</key>
<data>aGVsbG8sd29ybGQ</data>
<key>commonField4</key>
<true/>
<key>commonField6</key>
<real>1.732</real>
<key>commonField7</key>
<integer>1732</integer>
<key>common-field8</key>
<integer>32</integer>
</dict>
</plist>

Field1-"'<>&
Field2-value1
Field2-value2
hello,world
1
1.732
1732
32
```
