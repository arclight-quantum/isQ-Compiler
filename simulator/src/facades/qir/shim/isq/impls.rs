extern crate std;
use std::{thread, time, println};

use super::super::super::context::get_current_context as context;
use super::types::*;
use crate::devices::qdevice::QuantumOp::*;
use crate::facades::qir::context::RANK_REF;
pub fn isq_qir_shim_qis_u3(theta: f64, phi: f64, lam: f64, x0: K<QIRQubit>)->() {
    trace!(
        "calling isq_qir_shim_qis_u3(x0: {}, theta: {}, phi: {}, lam: {})",
        P(&x0),
        theta, phi, lam
    );
    let rctx = context();
    let mut ctx = rctx.lock().unwrap();
    let device = ctx.get_device_mut();
    device.controlled_qop(U3, &[], &[&x0.key], &[theta, phi, lam]);
}
pub fn isq_qir_shim_qis_rz_body(x0: f64, x1: K<QIRQubit>)->() {
    trace!(
        "calling isq_qir_shim_qis_rz(x0: {}, theta: {})",
        P(&x1),
        x0,
    );
    let rctx = context();
    let mut ctx = rctx.lock().unwrap();
    let device = ctx.get_device_mut();
    device.controlled_qop(Rz, &[], &[&x1.key], &[x0]);
}
pub fn isq_qir_shim_qis_x2p(x0: K<QIRQubit>)->() {
    trace!(
        "calling isq_qir_shim_qis_x2p(x0: {})",
        P(&x0)
    );
    let rctx = context();
    let mut ctx = rctx.lock().unwrap();
    let device = ctx.get_device_mut();
    device.controlled_qop(X2P, &[], &[&x0.key], &[]);
}
pub fn isq_qir_shim_qis_x2m(x0: K<QIRQubit>)->() {
    trace!(
        "calling isq_qir_shim_qis_x2m(x0: {})",
        P(&x0)
    );
    let rctx = context();
    let mut ctx = rctx.lock().unwrap();
    let device = ctx.get_device_mut();
    device.controlled_qop(X2M, &[], &[&x0.key], &[]);
}
pub fn isq_qir_shim_qis_y2p(x0: K<QIRQubit>)->() {
    trace!(
        "calling isq_qir_shim_qis_y2y(x0: {})",
        P(&x0)
    );
    let rctx = context();
    let mut ctx = rctx.lock().unwrap();
    let device = ctx.get_device_mut();
    device.controlled_qop(Y2P, &[], &[&x0.key], &[]);
}
pub fn isq_qir_shim_qis_y2m(x0: K<QIRQubit>)->() {
    trace!(
        "calling isq_qir_shim_qis_y2m(x0: {})",
        P(&x0)
    );
    let rctx = context();
    let mut ctx = rctx.lock().unwrap();
    let device = ctx.get_device_mut();
    device.controlled_qop(Y2M, &[], &[&x0.key], &[]);
}

pub fn isq_qir_shim_qis_gphase(x0: f64)->() {
    trace!(
        "calling isq_qir_shim_qis_gphase(x0: {})",
        x0
    );
    let rctx = context();
    let mut ctx = rctx.lock().unwrap();
    let device = ctx.get_device_mut();
    device.controlled_qop(GPhase, &[], &[], &[x0]);
}
pub fn isq_qir_shim_qis_cnot(x0: K<QIRQubit>, x1: K<QIRQubit>)->() {
    trace!("calling isq_qir_shim_qis_cnot(x0: {}, x1: {})", P(&x0), P(&x1));
    let rctx = context();
    let mut ctx = rctx.lock().unwrap();
    let device = ctx.get_device_mut();
    device.controlled_qop(CNOT, &[], &[&x0.key, &x1.key], &[]);
}
pub fn isq_qir_shim_qis_cz(x0: K<QIRQubit>, x1: K<QIRQubit>)->() {
    trace!("calling isq_qir_shim_qis_cz(x0: {}, x1: {})", P(&x0), P(&x1));
    let rctx = context();
    let mut ctx = rctx.lock().unwrap();
    let device = ctx.get_device_mut();
    device.controlled_qop(CZ, &[], &[&x0.key, &x1.key], &[]);
}
pub fn isq_qir_shim_qis_measure(x0: K<QIRQubit>)->QIRResult {
    trace!("calling isq_qir_shim_qis_measure(x0: {})", P(&x0));
    let rctx = context();
    let mut ctx = rctx.lock().unwrap();
    let device = ctx.get_device_mut();
    if device.measure(&x0.key){
        QIR_RESULT_ONE
    }else{
        QIR_RESULT_ZERO
    }

}
pub fn isq_qir_shim_qis_reset(x0: K<QIRQubit>)->() {
    trace!("calling isq_qir_shim_qis_reset(x0: {})", P(&x0));
    let rctx = context();
    let mut ctx = rctx.lock().unwrap();
    let device = ctx.get_device_mut();
    device.controlled_qop(Reset, &[], &[&x0.key], &[]);
}


pub fn isq_qir_shim_qis_isq_print_i64(x0: i64)->() {
    extern crate std;
    debug!("{}", x0);
}
pub fn isq_qir_shim_qis_isq_print_f64(x0: f64)->() {
    extern crate std;
    debug!("{}", x0);
}
pub fn isq_qir_shim_qis_qcis_finalize()->() {
    trace!(
        "calling isq_qir_shim_qis_qcis_finalize()",
    );
    let rctx = context();
    let mut ctx = rctx.lock().unwrap();
    let device = ctx.get_device_mut();
    device.controlled_qop(QcisFinalize, &[], &[], &[]);
}
pub fn isq_qir_shim_qis_bp(x0: i64)->() {
    trace!(
        "calling isq_qir_shim_qis_bp()",
    );
    let rctx = context();
    let mut ctx = rctx.lock().unwrap();
    if ctx.contains_bp(x0) {
        return;
    }
    extern crate std;
    use std::string::String;
    println!("Reaching a break point");
    let device = ctx.get_device();
    device.print_state();
    let mut buffer = String::new();
    std::io::stdin().read_line(&mut buffer).ok().expect("Failed to read line");
    if buffer.trim() == "d" {
        ctx.disable_bp_index(x0);
    }
}
static WAIT_TIME: time::Duration = time::Duration::from_millis(10);

pub fn isq_qir_shim_qmpi_comm_rank()->i64 {
    trace!("calling isq_qir_shim_qmpi_comm_rank()");
    RANK_REF.with(|r|r.borrow().clone())
}
pub fn isq_qir_shim_qmpi_comm_size()->i64 {
    trace!("calling isq_qir_shim_qmpi_comm_size()");
    let rctx = context();
    let ctx = rctx.lock().unwrap();
    *ctx.get_np()
}
pub fn isq_qir_shim_qmpi_csend(receiver: i64, tag: i64, val: bool)->() {
    trace!("calling isq_qir_shim_qmpi_csend(receiver: {}, tag: {}, val: {})", receiver, tag, val);
    let sender = isq_qir_shim_qmpi_comm_rank();
    loop {
        let rctx = context();
        let mut ctx = rctx.lock().unwrap();
        let mailbox = ctx.get_mailbox_mut();
        let tuple = (sender, receiver, tag);
        let mailbox_op = mailbox.get(&tuple);
        if mailbox_op.is_none() {
            mailbox.insert(tuple, val);
            return
        }
        drop(ctx);
        thread::sleep(WAIT_TIME);
    }
}
pub fn isq_qir_shim_qmpi_crecv(sender: i64, tag: i64)->bool {
    trace!("calling isq_qir_shim_qmpi_csend(sender: {}, tag: {})", sender, tag);
    let receiver = isq_qir_shim_qmpi_comm_rank();
    loop {
        let rctx = context();
        let mut ctx = rctx.lock().unwrap();
        let mailbox = ctx.get_mailbox_mut();
        let tuple = (sender, receiver, tag);
        let mailbox_op = mailbox.get(&tuple);
        if mailbox_op.is_some() {
            let val = mailbox_op.unwrap().clone();
            mailbox.remove(&tuple);
            return val
        }
        drop(ctx);
        thread::sleep(WAIT_TIME);
    }
}
pub fn isq_qir_shim_qmpi_create_epr(peer: i64, tag: i64, x0: K<QIRQubit>)->() {
    trace!("calling isq_qir_shim_qmpi_create_epr(peer: {}, tag: {}, x0: {})", peer, tag, P(&x0));
    let rank = isq_qir_shim_qmpi_comm_rank();
    assert_ne!(rank, peer);
    if rank < peer { // send
        // Add tuple to the waiting list
        let rctx = context();
        let mut ctx = rctx.lock().unwrap();
        let epr_pair = ctx.get_epr_pair_mut();
        let tuple = (rank, peer, tag);
        epr_pair.insert(tuple, x0.key);
        drop(ctx);
        thread::sleep(WAIT_TIME);

        // Check whether the tuple has been removed by the peer
        loop {
            let rctx = context();
            let mut ctx = rctx.lock().unwrap();
            if ctx.get_epr_pair_mut().get(&tuple).is_none() {
                return
            }
            drop(ctx);
            thread::sleep(WAIT_TIME);
        }
    } else { // recv
        loop {
            let rctx = context();
            let mut ctx = rctx.lock().unwrap();
            let epr_pair = ctx.get_epr_pair_mut();
            let tuple = (peer, rank, tag);
            let val_op = epr_pair.get(&tuple);
            if val_op.is_some() {
                let k1 = val_op.unwrap().clone();
                epr_pair.remove(&tuple);
                drop(epr_pair);
                let device = ctx.get_device_mut();
                device.controlled_qop(EPR, &[], &[&x0.key, &k1], &[]);
                return
            }
            drop(ctx);
            thread::sleep(WAIT_TIME);
        }
    }
}