// Privileged one-shot start gate for recoverable, unattended RPL-P diagnostics.
#include "kern_raptorlake_p_control.hpp"
#include "kern_raptorlake_p_native.hpp"
#include "kern_raptorlake_p_reference.hpp"
#include "kern_raptorlake_p_reference_abi.hpp"
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSBoolean.h>
#include <kern/task.h>
class WhateverXeRPLPControl : public IOService {
    OSDeclareDefaultStructors(WhateverXeRPLPControl)
public:
    IOReturn setProperties(OSObject *) override;
    IOReturn callPlatformFunction(const OSSymbol *,bool,void *,void *,void *,void *) override;
};
OSDefineMetaClassAndStructors(WhateverXeRPLPControl,IOService)
IOReturn WhateverXeRPLPControl::callPlatformFunction(const OSSymbol *name,bool wait,void *p1,void *p2,void *p3,void *p4){
    if(name && name->isEqualTo(RaptorLakeP::ReferenceABI::selector)){
        if(wait || p2 || p3 || p4)return kIOReturnUnsupported;
        return wexRplReferenceABI(p1)?kIOReturnSuccess:kIOReturnUnsupported;
    }
    return IOService::callPlatformFunction(name,wait,p1,p2,p3,p4);
}
IOReturn WhateverXeRPLPControl::setProperties(OSObject *properties) {
    auto dict=OSDynamicCast(OSDictionary,properties);
    if(!dict || dict->getCount()!=1)return kIOReturnUnsupported;
    const bool modules=dict->getObject("LoadReferenceModules")==kOSBooleanTrue;
    const bool capture=dict->getObject("CaptureReferenceState")==kOSBooleanTrue;
    if(!modules && !capture && dict->getObject("BeginDiagnostic")!=kOSBooleanTrue)return kIOReturnUnsupported;
    if(IOUserClient::clientHasPrivilege(current_task(),kIOClientPrivilegeAdministrator)!=kIOReturnSuccess)return kIOReturnNotPrivileged;
    if(modules)return RaptorLakeP::Reference::loadModules() ? kIOReturnSuccess : kIOReturnNotReady;
    if(capture)return RaptorLakeP::Reference::captureState() ? kIOReturnSuccess : kIOReturnNotReady;
    const bool accepted=RaptorLakeP::Reference::enabled() ? RaptorLakeP::Reference::arm() : RaptorLakeP::Native::requestRemoteArm();
    return accepted ? kIOReturnSuccess : kIOReturnNotReady;
}
namespace RaptorLakeP {namespace Control {
static WhateverXeRPLPControl *service;
bool create(IOPCIDevice *pci) {
    if(service)return true;
    if(!pci)return false;
    auto object=new WhateverXeRPLPControl;
    if(!object)return false;
    if(!object->init()){object->release();return false;}
    if(!object->attach(pci)){object->release();return false;}
    if(!object->start(pci)){object->detach(pci);object->release();return false;}
    object->setName("WhateverXeRPLPControl");
    object->setProperty("ControlVersion",1,32);
    object->setProperty("DiagnosticReady",false);object->setProperty("DiagnosticArmed",false);
    service=object;object->registerService();return true;
}
void update(bool ready,bool armed) {
    if(service){service->setProperty("DiagnosticReady",ready);service->setProperty("DiagnosticArmed",armed);}
}
}}
