#include "../WhateverXe/kern_tgl_render_window.hpp"
#include <cassert>
#include <initializer_list>
#include <cstring>
struct Page {uint64_t q[8];uint8_t bytes[4096];};
using Record=TGLRenderWindow::Capture<Page>;
int main(){
 Record c{};unsigned reads=0,checks=0;
 auto owned=[&]{++checks;return true;};
 auto read=[&](Page &p){assert(p.q[0]==c.q[2]+reads*4096ULL);++reads;p.q[2]=4096;p.q[3]=1;};
 TGLRenderWindow::collect(c,0x405cb250,owned,read);
 assert(c.q[6]==0&&reads==32&&checks==2&&c.q[2]==0x405ad000&&c.q[4]==0xffffffff&&c.q[5]==0xffffffff);
 assert(c.pages[30].q[0]==0x405cb000&&c.pages[31].q[0]==0x405cc000);
 for(uint64_t bad:{uint64_t(3),uint64_t(1ULL<<47),uint64_t((1ULL<<47)-4)}){
  c={};reads=checks=0;TGLRenderWindow::collect(c,bad,owned,read);assert(c.q[6]==1&&!reads&&!checks);
 }
 c={};reads=checks=0;TGLRenderWindow::collect(c,0x1000,owned,read);assert(c.q[2]==0&&reads==32);
 c={};reads=0;TGLRenderWindow::collect(c,0x405cb250,[]{return false;},read);assert(c.q[6]==2&&!reads);
 c={};reads=checks=0;TGLRenderWindow::collect(c,0x405cb250,[&]{return ++checks==1;},read);assert(c.q[6]==3&&reads==32);
 c={};reads=checks=0;TGLRenderWindow::collect(c,0x405cb250,owned,[&](Page &p){unsigned n=reads++;p.q[2]=n==0?4095:4096;p.q[3]=n==1?0:1;p.q[4]=n==2?3:0;});
 assert(c.q[4]==0xfffffffe&&c.q[5]==0xfffffff8&&c.q[6]==0);
}
