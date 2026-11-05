// Kernel imports of kern_xe.cpp's -wex-edp-keep code for the extracted
// Display13 AUX/SST wrapper fixtures. Boot arguments are test-controlled and
// registry properties keep their bytes.
#ifndef DISPLAY13_EDP_KEEP_STUBS_HPP
#define DISPLAY13_EDP_KEEP_STUBS_HPP
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "kern_display13_edp_keep.hpp"
inline std::set<std::string> &testKernelArguments() {static std::set<std::string> a;return a;}
inline bool checkKernelArgument(const char *name) {return testKernelArguments().count(name)!=0;}
struct TestRegistryRoot {
 unsigned published=0;
 std::map<std::string,std::vector<uint8_t>> data;
 template<class... T> bool setProperty(const char *,T...) {++published;return true;}
 template<class P,class N> bool setProperty(const char *key,P *bytes,N length) {
  ++published;auto b=reinterpret_cast<const uint8_t *>(bytes);data[key].assign(b,b+length);return true;
 }
};
struct IORegistryEntry {static TestRegistryRoot *getRegistryRoot() {static TestRegistryRoot root;return &root;}};
inline std::vector<std::string> &testSyslog() {static std::vector<std::string> l;return l;}
#ifndef SYSLOG
#define SYSLOG(module,fmt,...) testSyslog().push_back(fmt)
#endif
template<class T,size_t N> constexpr size_t arrsize(const T (&)[N]) {return N;}
#endif
