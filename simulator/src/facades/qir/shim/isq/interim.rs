use super::impls::*;
use super::types::*;
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__u3(x0: f64, x1: f64, x2: f64, x3: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_u3(t::<_, f64>(x0), t::<_, f64>(x1), t::<_, f64>(x2), t::<_, K<QIRQubit>>(x3)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__rz__body(x0: f64, x1: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_rz_body(t::<_, f64>(x0), t::<_, K<QIRQubit>>(x1)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__rx__body(x0: f64, x1: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_rx_body(t::<_, f64>(x0), t::<_, K<QIRQubit>>(x1)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__ry__body(x0: f64, x1: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_ry_body(t::<_, f64>(x0), t::<_, K<QIRQubit>>(x1)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__rxp__body(x0: *mut i8, x1: i64, x2: i64, x3: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_rxp_body(t::<_, *mut u8>(x0), t::<_, usize>(x1), t::<_, i64>(x2), t::<_, K<QIRQubit>>(x3)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__ryp__body(x0: *mut i8, x1: i64, x2: i64, x3: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_ryp_body(t::<_, *mut u8>(x0), t::<_, usize>(x1), t::<_, i64>(x2), t::<_, K<QIRQubit>>(x3)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__rzp__body(x0: *mut i8, x1: i64, x2: i64, x3: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_rzp_body(t::<_, *mut u8>(x0), t::<_, usize>(x1), t::<_, i64>(x2), t::<_, K<QIRQubit>>(x3)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__x2p(x0: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_x2p(t::<_, K<QIRQubit>>(x0)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__x2m(x0: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_x2m(t::<_, K<QIRQubit>>(x0)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__y2p(x0: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_y2p(t::<_, K<QIRQubit>>(x0)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__y2m(x0: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_y2m(t::<_, K<QIRQubit>>(x0)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__gphase(x0: f64)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_gphase(t::<_, f64>(x0)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__cnot(x0: *mut i8, x1: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_cnot(t::<_, K<QIRQubit>>(x0), t::<_, K<QIRQubit>>(x1)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__cz(x0: *mut i8, x1: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_cz(t::<_, K<QIRQubit>>(x0), t::<_, K<QIRQubit>>(x1)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__measure(x0: *mut i8)->*mut i8 {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_measure(t::<_, K<QIRQubit>>(x0)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__reset(x0: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_reset(t::<_, K<QIRQubit>>(x0)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__qcis__finalize()->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_qcis_finalize())}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__bp(x0: i64)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_bp(t::<_, i64>(x0)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__assert(x0: *mut *mut i8, x1: *mut *mut i8, x2: i64, x3: i64, x4: i64, x5: *mut f64, x6: *mut f64, x7: i64, x8: i64, x9: i64)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_assert(t::<_, *mut *mut i8>(x0), t::<_, *mut *mut i8>(x1), t::<_, i64>(x2), t::<_, i64>(x3), t::<_, i64>(x4), t::<_, *mut f64>(x5), t::<_, *mut f64>(x6), t::<_, i64>(x7), t::<_, i64>(x8), t::<_, i64>(x9)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__isq_print_i64(x0: i64)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_isq_print_i64(t::<_, i64>(x0)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qis__isq_print_f64(x0: f64)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qis_isq_print_f64(t::<_, f64>(x0)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qmpi__csend(receiver: i64, tag: i64, val: bool)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qmpi_csend(t::<_, i64>(receiver), t::<_, i64>(tag), t::<_, bool>(val)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qmpi__crecv(sender: i64, tag: i64)->bool {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qmpi_crecv(t::<_, i64>(sender), t::<_, i64>(tag)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qmpi__create__epr(peer: i64, tag: i64, x1: *mut i8)->() {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qmpi_create_epr(t::<_, i64>(peer), t::<_, i64>(tag), t::<_, K<QIRQubit>>(x1)))}
}
#[no_mangle]
pub extern "C" fn __isq__qir__shim__qmpi__size()->i64 {
    use core::mem::transmute as t;
    unsafe { t(isq_qir_shim_qmpi_comm_size())}
}
