// Useful quantum ops that can be supported.
use alloc::{vec::Vec, string::String};
#[derive(Copy, Clone, Eq, PartialEq, Debug, PartialOrd, Ord)]
pub enum QuantumOp {
    Reset,
    X,
    Y,
    Z,
    H,
    S,
    SInv,
    T,
    TInv,
    CNOT,
    CZ,
    Swap,
    CCNOT,
    Rx,
    Ry,
    Rz,
    U3,
    GPhase,
    AnySQ,
    X2P, X2M, Y2P, Y2M,
    QcisFinalize,
    EPR,
}

impl QuantumOp {
    pub fn get_qubit_count(&self) -> usize {
        match self {
            QuantumOp::Reset => 1,
            QuantumOp::X => 1,
            QuantumOp::Y => 1,
            QuantumOp::Z => 1,
            QuantumOp::H => 1,
            QuantumOp::S => 1,
            QuantumOp::SInv => 1,
            QuantumOp::T => 1,
            QuantumOp::TInv => 1,
            QuantumOp::CNOT => 2,
            QuantumOp::CZ => 2,
            QuantumOp::Swap => 2,
            QuantumOp::CCNOT => 3,
            QuantumOp::Rx => 1,
            QuantumOp::Ry => 1,
            QuantumOp::Rz => 1,
            QuantumOp::U3 => 1,
            QuantumOp::GPhase => 0,
            QuantumOp::AnySQ => 1,
            QuantumOp::X2P => 1,
            QuantumOp::X2M => 1,
            QuantumOp::Y2P => 1,
            QuantumOp::Y2M => 1,
            QuantumOp::QcisFinalize => 0,
            QuantumOp::EPR => 2,
        }
    }
    pub fn get_parameter_count(&self) -> usize {
        match self {
            QuantumOp::Reset => 0,
            QuantumOp::X => 0,
            QuantumOp::Y => 0,
            QuantumOp::Z => 0,
            QuantumOp::H => 0,
            QuantumOp::S => 0,
            QuantumOp::SInv => 0,
            QuantumOp::T => 0,
            QuantumOp::TInv => 0,
            QuantumOp::CNOT => 0,
            QuantumOp::CZ => 0,
            QuantumOp::Swap => 0,
            QuantumOp::CCNOT => 0,
            QuantumOp::Rx => 1,
            QuantumOp::Ry => 1,
            QuantumOp::Rz => 1,
            QuantumOp::U3 => 3,
            QuantumOp::GPhase => 1,
            QuantumOp::AnySQ => 8,
            QuantumOp::X2P => 0,
            QuantumOp::X2M => 0,
            QuantumOp::Y2P => 0,
            QuantumOp::Y2M => 0,
            QuantumOp::QcisFinalize => 0,
            QuantumOp::EPR => 0,
        }
    }
}
pub trait QDevice {
    type Qubit: Eq;
    fn alloc_qubit(&mut self) -> Self::Qubit;
    fn free_qubit(&mut self, qubit: Self::Qubit);
    fn supported_quantum_ops(&self) -> Vec<QuantumOp>;
    fn qop(&mut self, op_type: QuantumOp, qubits: &[&Self::Qubit], parameters: &[f64]) {
        self.controlled_qop(op_type, &[], qubits, parameters)
    }
    fn controlled_qop(
        &mut self,
        op_type: QuantumOp,
        controllers: &[&Self::Qubit],
        qubits: &[&Self::Qubit],
        parameters: &[f64],
    ) {
        if controllers.len() != 0 {
            panic!("This device does not support accelerated-controlled operations. Do decomposition first.");
        }
        self.qop(op_type, qubits, parameters)
    }
    fn measure(&mut self, qubit: &Self::Qubit) -> bool;
    fn get_measure_res(&mut self) -> String{
        "".into()
    }
    fn print_state(&mut self) {}
    fn assert(&self, _qubits: &[&Self::Qubit], _space: &[f64]) -> bool {true}
    fn param_qop(
        &mut self,
        op_type: QuantumOp,
        qubits: &[&Self::Qubit],
        parameters: String
    ){}
}

