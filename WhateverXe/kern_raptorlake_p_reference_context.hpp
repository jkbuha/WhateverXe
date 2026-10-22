// Weak cache protected through destruction; readers borrow only live objects.
#pragma once
namespace RaptorLakeP {namespace ReferenceContext {
struct Cache {
 void *pointer{};
 template<class IO>void store(IO &io,void *object){io.lock();pointer=object;io.unlock();}
 template<class IO>bool forget(IO &io,void *object){io.lock();bool matched=pointer==object;if(matched)pointer=nullptr;io.unlock();return matched;}
 template<class IO>void *acquire(IO &io){
  io.lock();auto object=pointer;
  if(object && !io.tryRetain(object)){pointer=nullptr;object=nullptr;}
  io.unlock();return object;
 }
};
}}
