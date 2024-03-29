use core::ops::Rem;
use rand::{distributions::Uniform, prelude::Distribution};

use super::types::*;
pub fn isq_qir_shim_qis_nan_body() -> f64 {
    trace!("calling isq_qir_shim_qis_nan_body()");
    f64::NAN
}
pub fn isq_qir_shim_qis_isnan_body(x0: f64) -> bool {
    trace!("calling isq_qir_shim_qis_isnan_body({})", x0);
    x0.is_nan()
}
pub fn isq_qir_shim_qis_infinity_body() -> f64 {
    trace!("calling isq_qir_shim_qis_infinity_body()");
    f64::INFINITY
}
pub fn isq_qir_shim_qis_isinf_body(x0: f64) -> bool {
    trace!("calling isq_qir_shim_qis_isinf_body({})", x0);
    x0.is_infinite() && x0 > 0.0
}
pub fn isq_qir_shim_qis_isnegativeinfinity_body(x0: f64) -> bool {
    trace!("calling isq_qir_shim_qis_isnegativeinfinity_body({})", x0);
    x0.is_infinite() && x0 < 0.0
}
pub fn isq_qir_shim_qis_sin_body(x0: f64) -> f64 {
    trace!("calling isq_qir_shim_qis_sin_body({})", x0);
    x0.sin()
}
pub fn isq_qir_shim_qis_cos_body(x0: f64) -> f64 {
    trace!("calling isq_qir_shim_qis_cos_body({})", x0);
    x0.cos()
}
pub fn isq_qir_shim_qis_tan_body(x0: f64) -> f64 {
    trace!("calling isq_qir_shim_qis_tan_body({})", x0);
    x0.tan()
}
pub fn isq_qir_shim_qis_arctan2_body(x0: f64, x1: f64) -> f64 {
    trace!("calling isq_qir_shim_qis_arctan2_body({}, {})", x0, x1);
    f64::atan2(x0, x1)
}
pub fn isq_qir_shim_qis_sinh_body(x0: f64) -> f64 {
    trace!("calling isq_qir_shim_qis_sinh_body({})", x0);
    x0.sinh()
}
pub fn isq_qir_shim_qis_cosh_body(x0: f64) -> f64 {
    trace!("calling isq_qir_shim_qis_cosh_body({})", x0);
    x0.cosh()
}
pub fn isq_qir_shim_qis_tanh_body(x0: f64) -> f64 {
    trace!("calling isq_qir_shim_qis_tanh_body({})", x0);
    x0.tanh()
}
pub fn isq_qir_shim_qis_arcsin_body(x0: f64) -> f64 {
    trace!("calling isq_qir_shim_qis_arcsin_body({})", x0);
    x0.asin()
}
pub fn isq_qir_shim_qis_arccos_body(x0: f64) -> f64 {
    trace!("calling isq_qir_shim_qis_arccos_body({})", x0);
    x0.acos()
}
pub fn isq_qir_shim_qis_arctan_body(x0: f64) -> f64 {
    trace!("calling isq_qir_shim_qis_arctan_body({})", x0);
    x0.atan()
}
pub fn isq_qir_shim_qis_sqrt_body(x0: f64) -> f64 {
    trace!("calling isq_qir_shim_qis_sqrt_body({})", x0);
    x0.sqrt()
}
pub fn isq_qir_shim_qis_log_body(x0: f64) -> f64 {
    trace!("calling isq_qir_shim_qis_log_body({})", x0);
    x0.ln()
}
pub fn isq_qir_shim_qis_ieeeremainder_body(x0: f64, x1: f64) -> f64 {
    trace!(
        "calling isq_qir_shim_qis_ieeeremainder_body({}, {})",
        x0,
        x1
    );
    x0.rem(x1)
}
pub fn isq_qir_shim_qis_drawrandomint_body(x0: i64, x1: i64) -> i64 {
    trace!(
        "calling isq_qir_shim_qis_drawrandomint_body({}, {})",
        x0,
        x1
    );
    if x1 < x0 {
        panic!("bad random range");
    }
    let mut rng = rand::thread_rng();
    Uniform::from(x0..=x1).sample(&mut rng)
}
pub fn isq_qir_shim_qis_drawrandomdouble_body(x0: f64, x1: f64) -> f64 {
    trace!(
        "calling isq_qir_shim_qis_drawrandomdouble_body({}, {})",
        x0,
        x1
    );
    if x1 < x0 {
        panic!("bad random range");
    }
    let mut rng = rand::thread_rng();
    Uniform::new_inclusive(x0, x1).sample(&mut rng)
}
// pub fn isq_qir_shim_qis_applyifelseintrinsic_body(
//     x0: QIRResult,
//     x1: K<QIRCallable>,
//     x2: K<QIRCallable>,
// ) -> () {
//     trace!(
//         "calling isq_qir_shim_qis_applyifelseintrinsic_body({:?}, {}, {})",
//         x0,
//         P(&x1),
//         P(&x2)
//     );
//     if x0 == QIR_RESULT_ONE {
//         crate::facades::qir::shim::qir_builtin::impls::isq_qir_shim_rt_callable_invoke(
//             x2, 0 as _, 0 as _,
//         );
//     } else {
//         crate::facades::qir::shim::qir_builtin::impls::isq_qir_shim_rt_callable_invoke(
//             x1, 0 as _, 0 as _,
//         );
//     }
// }
// pub fn isq_qir_shim_qis_applyconditionallyinstrinsic_body(
//     x0: K<QIRArray>,
//     x1: K<QIRArray>,
//     x2: K<QIRCallable>,
//     x3: K<QIRCallable>,
// ) -> () {
//     trace!(
//         "calling isq_qir_shim_qis_applyconditionallyinstrinsic_body({}, {}, {}, {})",
//         P(&x0),
//         P(&x1),
//         P(&x2),
//         P(&x3)
//     );
//     use crate::facades::qir::shim::context;
//     let rctx = context();
//     let mut ctx = rctx.lock().unwrap();
//     let measurement_results = x0
//         .get(&ctx)
//         .get_1d_data_of::<QIRResult>()
//         .iter()
//         .copied()
//         .collect_vec();
//     let expected_results = x1
//         .get(&ctx)
//         .get_1d_data_of::<QIRResult>()
//         .iter()
//         .copied()
//         .collect_vec();
//     drop(ctx);
//     drop(rctx);
//     if measurement_results == expected_results {
//         crate::facades::qir::shim::qir_builtin::impls::isq_qir_shim_rt_callable_invoke(
//             x2, 0 as _, 0 as _,
//         );
//     } else {
//         crate::facades::qir::shim::qir_builtin::impls::isq_qir_shim_rt_callable_invoke(
//             x3, 0 as _, 0 as _,
//         );
//     }
// }
pub fn isq_qir_shim_qis_assertmeasurementprobability_body(
    x0: K<QIRArray>,
    x1: K<QIRArray>,
    x2: QIRResult,
    x3: f64,
    x4: K<QIRString>,
    x5: f64,
) -> () {
    trace!(
        "calling isq_qir_shim_qis_assertmeasurementprobability_body({}, {}, {:?}, {}, {}, {})",
        P(&x0),
        P(&x1),
        x2,
        x3,
        P(&x4),
        x5
    );
    todo!()
}
pub fn isq_qir_shim_qis_assertmeasurementprobability_ctl(
    _x0: K<QIRArray>,
    _x1: *const MeasurementProbabilityArgs,
) -> () {
    todo!()
}
