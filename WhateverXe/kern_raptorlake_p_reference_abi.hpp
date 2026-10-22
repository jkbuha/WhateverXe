// SPDX-License-Identifier: BSD-2-Clause
// Kernel-only, fixed-size bridge to the qualified reference framebuffer owner.
#ifndef KERN_RPLP_REFERENCE_ABI_HPP
#define KERN_RPLP_REFERENCE_ABI_HPP
#include <stdint.h>
namespace RaptorLakeP {namespace ReferenceABI {
constexpr char selector[]="WhateverXeRPLPReferenceABI1";
enum Operation:uint32_t {Read32=1,Write32,Read64,Write64,Time,DSB};
struct Request {
 uint32_t version,size,operation,status;
 void *owner;
 volatile void *base;
 uint64_t offset,value;
};
static_assert(sizeof(Request)==48,"reference bridge ABI");
inline bool valid(const Request &r){return r.version==1 && r.size==sizeof(Request) && r.owner && r.operation>=Read32 && r.operation<=DSB;}
}}
bool wexRplReferenceABI(void *);
#endif
