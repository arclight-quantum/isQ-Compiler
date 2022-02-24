%Range = type { i64, i64, i64 }
%Tuple = type opaque
%Qubit = type opaque
%Result = type opaque
%Array = type opaque
%Callable = type opaque
%BigInt = type opaque
%Pauli = type i2
%String = type opaque
define void @__quantum__qis__u3 (%Qubit* %x0, double %x1, double %x2, double %x3) alwaysinline {
entry:
    %x4 = bitcast %Qubit* %x0 to i8*
    %x5 = bitcast double %x1 to double
    %x6 = bitcast double %x2 to double
    %x7 = bitcast double %x3 to double
    call void @__isq__qir__shim__qis__u3(i8* %x4, double %x5, double %x6, double %x7)
    ret void
}
declare dllimport void @__isq__qir__shim__qis__u3(i8*, double, double, double)
define void @__quantum__qis__gphase (double %x0) alwaysinline {
entry:
    %x1 = bitcast double %x0 to double
    call void @__isq__qir__shim__qis__gphase(double %x1)
    ret void
}
declare dllimport void @__isq__qir__shim__qis__gphase(double)
define void @__quantum__qis__cnot (%Qubit* %x0, %Qubit* %x1) alwaysinline {
entry:
    %x2 = bitcast %Qubit* %x0 to i8*
    %x3 = bitcast %Qubit* %x1 to i8*
    call void @__isq__qir__shim__qis__cnot(i8* %x2, i8* %x3)
    ret void
}
declare dllimport void @__isq__qir__shim__qis__cnot(i8*, i8*)
define %Result* @__quantum__qis__measure (%Qubit* %x0) alwaysinline {
entry:
    %x1 = bitcast %Qubit* %x0 to i8*
    %x2 = call i8* @__isq__qir__shim__qis__measure(i8* %x1)
    %x3 = bitcast i8* %x2 to %Result*
    ret %Result* %x3
}
declare dllimport i8* @__isq__qir__shim__qis__measure(i8*)