#ifndef KERN_RAPTORLAKE_P_CONTROL_HPP
#define KERN_RAPTORLAKE_P_CONTROL_HPP
class IOPCIDevice;
namespace RaptorLakeP {namespace Control {
bool create(IOPCIDevice *);
void update(bool,bool);
}}
#endif
