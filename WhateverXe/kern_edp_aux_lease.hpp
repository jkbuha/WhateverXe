// Bounded native AUX power reference around one qualified training call.
#ifndef kern_edp_aux_lease_hpp
#define kern_edp_aux_lease_hpp
namespace EdpAuxLease {
template<class Get,class Put,class Native> auto run(bool qualified,Get get,Put put,Native native)->decltype(native()) {
 if(!qualified)return native();
 get();const auto result=native();put();return result;
}
}
#endif
