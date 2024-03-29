// Naive unitary simulator. Using a vector representation and perform matrix onto the vectors.
// May renumber the qubits to simplify qubit freeing.

use crate::qdevice::QDevice;
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use num_complex::Complex64;
use serde::Serialize;
extern crate std;
use std::{println};

#[derive(Serialize)]
pub struct DebugInfo{
    phy_state: Vec<(f64, f64)>,
    phy_qubit_to_virt_qubit: Vec<usize>
}

#[derive(Clone)]
pub struct NaiveSimulator {
    state: Vec<Complex64>,
    qubit_map: BTreeMap<usize, usize>,
    qubit_map_inv: Vec<usize>,
    allocated_qubit_counter: usize,
    measure_res: String,
    reset_flag: bool,
    capacity: usize
}

impl NaiveSimulator {
    pub fn new(capacity: usize) -> Self {
        trace!("Naive simulator created");
        let zero_state = vec![Complex64::new(1.0, 0.0)];
        Self {
            state: zero_state,
            qubit_map: BTreeMap::new(),
            qubit_map_inv: Vec::new(),
            allocated_qubit_counter: 0,
            measure_res: "".into(),
            reset_flag: false,
            capacity: capacity
        }
    }
    fn qubit_to_state_id(&self, q: &<NaiveSimulator as QDevice>::Qubit) -> usize {
        self.qubit_map
            .get(q)
            .expect(&format!("Qubit {} freed", q))
            .clone()
    }
    // Reset all qubits to ket-0.
    pub fn reset_all(&mut self) {
        self.state
            .iter_mut()
            .for_each(|x| *x = Complex64::new(0.0, 0.0));
        self.state[0] = Complex64::new(1.0, 0.0);
    }
    // Move a qubit to most-significant bit.
    // This is used to simplify the qubit freeing.
    // TODO: performance test against version without if-then-else.
    fn swap_bits(
        &mut self,
        q1: &<NaiveSimulator as QDevice>::Qubit,
        q2: &<NaiveSimulator as QDevice>::Qubit,
    ) {
        if q1 == q2 {
            return;
        }
        let i1 = self.qubit_to_state_id(q1);
        let i2 = self.qubit_to_state_id(q2);
        let mask1 = 1 << i1;
        let mask2 = 1 << i2;
        for i in 0..self.state.iter_mut().len() {
            if (i & mask1) == 0 && (i & mask2) != 0 {
                // swap amplitude.
                let i_other = i ^ mask1 ^ mask2;
                let x_other = self.state[i_other];
                let x_this = self.state[i];
                self.state[i] = x_other;
                self.state[i_other] = x_this;
            }
        }
        self.qubit_map_inv[i2] = *q1;
        self.qubit_map_inv[i1] = *q2;
        self.qubit_map.insert(*q1, i2);
        self.qubit_map.insert(*q2, i1);
    }
    fn swap_qubit_to_msb(&mut self, q1: &<NaiveSimulator as QDevice>::Qubit) {
        let msb = *self.qubit_map_inv.last().expect("No qubit allocated");
        self.swap_bits(q1, &msb);
        assert_eq!(self.qubit_map_inv.last().unwrap(), q1);
    }

    // Measure out the most significant qubit and get the result.
    // Returns probability to get two results.
    fn try_remove_msb_qubit(&self) -> (f64, f64) {
        if self.state.len() == 1 {
            panic!("No qubits allocated");
        }
        let mut prob_zero = 0f64;
        for i in 0..self.state.len() / 2 {
            prob_zero += self.state[i].norm_sqr();
        }
        prob_zero = prob_zero.clamp(0.0, 1.0);
        assert_eq!(
            prob_zero >= 0.0 && prob_zero <= 1.0,
            true,
            "prob_zero out of range {}",
            prob_zero
        );
        (prob_zero, 1.0f64 - prob_zero)
    }

    // Do the real collaposing.
    fn collapse_msb_qubit_into(&mut self, value: bool, prob: f64) {
        let prob_sqrt = prob.sqrt();
        let mid = self.state.len() / 2;
        let (lo, hi) = self.state.split_at_mut(mid);
        if value {
            lo.iter_mut().for_each(|x| *x = Complex64::new(0.0, 0.0));
            hi.iter_mut().for_each(|x| *x /= prob_sqrt);
        } else {
            hi.iter_mut().for_each(|x| *x = Complex64::new(0.0, 0.0));
            lo.iter_mut().for_each(|x| *x /= prob_sqrt);
        }
    }
    fn validate(&self){
        trace!("{:?}", self.state);
        let sum: f64 = self.state.iter().map(|x| x.norm_sqr()).sum();
        if sum>1.001 || sum<0.999{
            panic!("failed as the sum is {}", sum);
        }
    }
    // Remove the most significant qubit. This must be done after assuring that the qubit is in computational state.
    fn trace_out_msb_computational_qubit(&mut self, value: bool) {
        let qubit_id = self.qubit_map_inv.pop().expect("No qubit allocated");
        self.validate();
        if value {
            let mid = self.state.len() / 2;
            let (lo, hi) = self.state.split_at_mut(mid);
            unsafe {
                core::ptr::copy_nonoverlapping(hi.as_mut_ptr(), lo.as_mut_ptr(), hi.len());
            }
        }
        unsafe {
            self.state.set_len(self.state.len() / 2);
        }
        self.validate();
        self.qubit_map.remove(&qubit_id).unwrap();
    }

    fn allocate_qubit_at_msb(&mut self) -> usize {
        if self.qubit_map.len() + 1 > self.capacity {
            panic!("simulation on qcloud can only support {} qubits, please use offline tarball isqc for more qubits", self.capacity);
        }
        self.state
            .resize(self.state.len() * 2, Complex64::new(0.0, 0.0));
        let next_id = self.allocated_qubit_counter;
        self.allocated_qubit_counter += 1;
        self.qubit_map.insert(next_id, self.qubit_map_inv.len());
        self.qubit_map_inv.push(next_id);
        next_id
    }
    fn single_qubit_gate(&mut self, controllers: &[&usize], pos: usize, mat: [[Complex64; 2]; 2]) {
        let pos = self.qubit_to_state_id(&pos);
        'state_arg: for i in 0..self.state.len() {
            for c in controllers.iter().copied().copied() {
                if i & (1 << c) == 0 {
                    continue 'state_arg;
                }
            }
            if (i & (1 << pos)) == 0 {
                let a = self.state[i];
                let b = self.state[i + (1 << pos)];
                self.state[i] = mat[0][0] * a + mat[0][1] * b;
                self.state[i + (1 << pos)] = mat[1][0] * a + mat[1][1] * b;
            }
        }
    }
    fn two_qubit_gate(
        &mut self,
        controllers: &[&usize],
        pos1: usize,
        pos2: usize,
        mat: [[Complex64; 4]; 4],
    ) {
        let pos1 = self.qubit_to_state_id(&pos1);
        let pos2 = self.qubit_to_state_id(&pos2);
        assert_ne!(pos1, pos2);
        'state_arg: for i in 0..self.state.len() {
            for c in controllers.iter().copied().copied() {
                if i & (1 << c) == 0 {
                    continue 'state_arg;
                }
            }
            if (i & (1 << pos1)) == 0 && (i & (1 << pos2)) == 0 {
                let a = self.state[i];
                let b = self.state[i + (1 << pos2)];
                let c = self.state[i + (1 << pos1)];
                let d = self.state[i + (1 << pos1) + (1 << pos2)];
                self.state[i] = mat[0][0] * a + mat[0][1] * b + mat[0][2] * c + mat[0][3] * d;
                self.state[i + (1 << pos2)] =
                    mat[1][0] * a + mat[1][1] * b + mat[1][2] * c + mat[1][3] * d;
                self.state[i + (1 << pos1)] =
                    mat[2][0] * a + mat[2][1] * b + mat[2][2] * c + mat[2][3] * d;
                self.state[i + (1 << pos1) + (1 << pos2)] =
                    mat[3][0] * a + mat[3][1] * b + mat[3][2] * c + mat[3][3] * d;
            }
        }
    }

    // Perfomed when post-measurement msb is 1.
    fn zeroing_msb(&mut self) {
        let mid = self.state.len() / 2;
        let (lo, hi) = self.state.split_at_mut(mid);
        assert_eq!(lo.len(), hi.len());
        unsafe {
            core::ptr::copy_nonoverlapping(hi.as_mut_ptr(), lo.as_mut_ptr(), mid);
            hi.iter_mut().for_each(|x| *x = core::mem::zeroed());
        }
    }
}

fn random_coin(a: f64, b: f64) -> (bool, f64) {
    let r = rand::random::<f64>();
    if r < a {
        (false, a)
    } else {
        (true, b)
    }
}

/*fn print_vec<T: core::fmt::Display>(row: &Vec<T>) {
    print!("[");
    for v in row {
        print!("{}, ", v);
    }
    println!("]");
}

fn print_matrix(mat: &Vec<Vec<Complex64>>) {
    print!("[");
    for row in mat {
        print_vec(row);
    }
    println!("]");
}*/

impl QDevice for NaiveSimulator {
    type Qubit = usize;

    fn alloc_qubit(&mut self) -> Self::Qubit {
        let new_qubit = self.allocate_qubit_at_msb();
        trace!("Allocating qubit {}", new_qubit);
        new_qubit
    }

    fn free_qubit(&mut self, qubit: Self::Qubit) {
        self.swap_qubit_to_msb(&qubit);
        let (prob_zero, prob_one) = self.try_remove_msb_qubit();
        if prob_zero >= 1e-4 && prob_one >= 1e-4 {
            warn!(
                "Qubit {} is not in computational state! prob = ({}, {})",
                qubit, prob_zero, prob_one
            );
            warn!("This may be treated as an error in Q# environment. However we perform trace-out here.");
        }
        let (result, prob) = random_coin(prob_zero, prob_one);
        self.collapse_msb_qubit_into(result, prob);
        assert_eq!(self.qubit_map_inv.last().unwrap(), &qubit);
        self.trace_out_msb_computational_qubit(result);
        trace!("Freeing qubit {}", qubit);
    }

    fn supported_quantum_ops(&self) -> Vec<crate::qdevice::QuantumOp> {
        use crate::qdevice::QuantumOp::*;
        vec![Reset, CNOT, CZ, Swap, AnySQ, GPhase, EPR]
    }

    fn controlled_qop(
        &mut self,
        op_type: crate::qdevice::QuantumOp,
        controllers: &[&Self::Qubit],
        qubits: &[&Self::Qubit],
        parameters: &[f64],
    ) {
        trace!(
            "Perform {:?}{:?} on {:?}-{:?}",
            op_type,
            parameters,
            controllers,
            qubits
        );
        use crate::qdevice::QuantumOp::*;
        match op_type {
            Reset => {
                if controllers.len() != 0 {
                    panic!("Reset is not allowed to have control qubits");
                }
                self.reset_flag = true;
                let ret = self.measure(qubits[0]);
                self.validate();
                if ret {
                    self.zeroing_msb();
                }
                self.validate();
                self.reset_flag = false;
            }
            Swap => {
                self.two_qubit_gate(
                    controllers,
                    *qubits[0],
                    *qubits[1],
                    [
                        [1.0.into(), 0.0.into(), 0.0.into(), 0.0.into()],
                        [0.0.into(), 0.0.into(), 1.0.into(), 0.0.into()],
                        [0.0.into(), 1.0.into(), 0.0.into(), 0.0.into()],
                        [0.0.into(), 0.0.into(), 0.0.into(), 1.0.into()],
                    ],
                );
            }
            CNOT => {
                self.two_qubit_gate(
                    controllers,
                    *qubits[0],
                    *qubits[1],
                    [
                        [1.0.into(), 0.0.into(), 0.0.into(), 0.0.into()],
                        [0.0.into(), 1.0.into(), 0.0.into(), 0.0.into()],
                        [0.0.into(), 0.0.into(), 0.0.into(), 1.0.into()],
                        [0.0.into(), 0.0.into(), 1.0.into(), 0.0.into()],
                    ],
                );
            }
            CZ => {
                self.two_qubit_gate(
                    controllers,
                    *qubits[0],
                    *qubits[1],
                    [
                        [1.0.into(), 0.0.into(), 0.0.into(), 0.0.into()],
                        [0.0.into(), 1.0.into(), 0.0.into(), 0.0.into()],
                        [0.0.into(), 0.0.into(), 1.0.into(), 0.0.into()],
                        [0.0.into(), 0.0.into(), 0.0.into(), (-1.0).into()],
                    ],
                );
            }
            AnySQ => {
                let p = parameters;
                let mat: [[Complex64; 2]; 2] = [
                    [Complex64::new(p[0], p[1]), Complex64::new(p[2], p[3])],
                    [Complex64::new(p[4], p[5]), Complex64::new(p[6], p[7])]
                ];
                self.single_qubit_gate(controllers, *qubits[0], mat)
            }
            EPR => {
                assert_ne!(qubits[0], qubits[1]);
                self.measure(qubits[0]);
                self.measure(qubits[1]);
                let pos1 = self.qubit_to_state_id(qubits[0]);
                let pos2 = self.qubit_to_state_id(qubits[1]);
                for i in 0..self.state.len() {
                    if (i & (1 << pos1)) == 0 && (i & (1 << pos2)) == 0 {
                        let a = self.state[i];
                        let b = self.state[i + (1 << pos2)];
                        let c = self.state[i + (1 << pos1)];
                        let d = self.state[i + (1 << pos1) + (1 << pos2)];
                        let m = a + b + c + d;
                        self.state[i] = m * (0.5f64).sqrt();
                        self.state[i + (1 << pos1) + (1 << pos2)] = self.state[i];
                    }
                }
            }
            _ => {
                panic!("Unsupported quantum operation: {:?}", op_type);
            }
        }
    }

    fn measure(&mut self, qubit: &Self::Qubit) -> bool {
        self.swap_qubit_to_msb(&qubit);
        let (prob_zero, prob_one) = self.try_remove_msb_qubit();
        let (result, prob) = random_coin(prob_zero, prob_one);
        self.collapse_msb_qubit_into(result, prob);
        trace!(
            "Measuring qubit {} (prob = ({}, {})), yielding {}",
            qubit,
            prob_zero,
            prob_one,
            result
        );
        if !self.reset_flag{
            match result{
                true => self.measure_res += "1",
                false => self.measure_res += "0"
            }
        }
        result
    }

    fn get_measure_res(&mut self) -> String {
        return self.measure_res.clone();
    }
    fn print_state(&mut self) {
        println!("Qubit states:");
        let debug_info = DebugInfo{
            phy_state: self.state.iter().map(|x| (x.re, x.im)).collect(),
            phy_qubit_to_virt_qubit: self.qubit_map_inv.clone()
        };
        println!("{}", serde_json::to_string(&debug_info).unwrap());
        /*
        println!("Qubit states:");
        for (count, v) in self.state.iter().enumerate() {
            println!("{}: {}", count, v);
        }
        */
    }
    fn assert(&self, qubits: &[&Self::Qubit], space: &[f64]) -> bool {
        // Get the real index of the qubits
        let mut ids: Vec<usize> = Vec::new();
        for q in qubits {
            ids.push(self.qubit_to_state_id(q));
        }

        let nq = qubits.len();
        let npow = 1 << nq;
        assert!(npow * npow * 2 == space.len());

        // Get the projector
        let mut projector: Vec<Vec<Complex64>> = Vec::new();
        let mut rho: Vec<Vec<Complex64>> = Vec::new();
        for i in 0..npow {
            let mut row: Vec<Complex64> = Vec::new();
            for j in 0..npow {
                row.push(Complex64::new(space[2 * (i * npow + j)], space[2 * (i * npow + j) + 1]));
            }
            projector.push(row);
            rho.push(vec![Complex64::new(0.0, 0.0); npow]);
        }

        // Calculate \rho based on tr_B(|index><dual|) = |row><col|
        let all = self.state.len();
        for index in 0..all {
            let mut row = 0;
            let mut zeroed = index;
            for i in 0..nq {
                let id = ids[i];
                let bit = (index >> id) & 1;
                row += bit << i;
                zeroed = zeroed - (bit << id);
            }
            for col in 0..npow {
                let mut dual = zeroed;
                for k in 0..nq {
                    let bit = (col >> k) & 1;
                    dual = dual + (bit << ids[k]);
                }
                rho[row][col] += self.state[index] * self.state[dual].conj();
            }
        }

        // for debugging
        //print_matrix(&projector);
        //print_matrix(&rho);

        // p(rho in projector) = tr(projector * rho)
        let mut p = Complex64::new(0.0, 0.0);
        for i in 0..npow {
            for j in 0..npow {
                p += projector[i][j] * rho[i][j];
            }
        }

        let delta = 1e-4;
        assert!(p.im < delta);
        p.re > 1.0 - delta && p.re < 1.0 + delta
    }
}

#[cfg(test)]
mod test {
    
    #[test]
    pub fn naive_bell() {
        use super::*;
        use crate::devices::checked::*;
        use crate::qdevice::QuantumOp::*;
        use crate::devices::sq2u3::SQ2U3Device;

        env_logger::init();
        let mut device = CheckedDevice::new(SQ2U3Device::new(NaiveSimulator::new(22)));

        let (mut r_00, mut r_01, mut r_10, mut r_11) = (0, 0, 0, 0);
        for _i in 0..1000 {
            let a = device.alloc_qubit();
            let b = device.alloc_qubit();
            device.qop(H, &[&a], &[]);
            device.qop(CNOT, &[&a, &b], &[]);
            let ra = device.measure(&a);
            let rb = device.measure(&b);
            if ra && rb {
                r_11 += 1;
            } else if ra && !rb {
                r_10 += 1;
            } else if !ra && rb {
                r_01 += 1;
            } else {
                r_00 += 1;
            }
            device.free_qubit(a);
            device.free_qubit(b);
        }
        println!("Result: {}, {}, {}, {}", r_00, r_01, r_10, r_11);
    }
}
