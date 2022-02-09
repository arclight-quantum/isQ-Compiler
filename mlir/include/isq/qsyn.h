#ifndef QSYN_H
#define QSYN_H

#include <Eigen/Core>
#include <Eigen/Eigenvalues>
#include <Eigen/SVD>
#include <vector>


namespace qsyn {

    using namespace Eigen;
    using std::vector;
    using std::tuple;
    using std::pair;

    enum GateType {
        NONE, RX, RY, RZ, CNOT, MX, MY, MZ
    };

    typedef vector<int> GateLocation;
    typedef double GatePhase;
    typedef double GateAngle;
    typedef MatrixXcd GateMatrix;
    typedef tuple<GateType, GateLocation, GateMatrix, GatePhase> Gate;
    typedef vector<Gate> GateSequence;
    typedef pair<double, double> ComplexPair;
    typedef vector<ComplexPair> UnitaryVector;
    typedef tuple<GateType, GateLocation, GateAngle, GateAngle, GateAngle> ElementGate;
    typedef vector<ElementGate> DecomposedGates;

    // Single Pauli matrices
    // Matrix2cd Rx(double angle);
    // Matrix2cd Ry(double angle);
    // Matrix2cd Rz(double angle);

    // Cosine-Sine Decomposition
    class CSD {
        public:
            MatrixXcd A1;
            MatrixXcd A2;
            MatrixXcd B1;
            MatrixXcd B2;
            MatrixXcd C;
            MatrixXcd S;
            CSD(MatrixXcd U);
    };

    // Multiplexed-Pauli Decomposition
    // GateSequence MPD(std::vector<double> angles, GateLocation labQ, GateType P);

    // Quantum Shannon Decomposition
    // GateSequence& QSD(GateSequence& decomposed_gates, GateSequence& remain_gates);

    DecomposedGates simplify(DecomposedGates &gates);
    
    bool verify(int n, UnitaryVector& Uvector, DecomposedGates& gates, double phase);

    class qsyn {
        public:
            DecomposedGates gates;
            GatePhase phase;
            double eps;
            qsyn(int n, UnitaryVector uvector, double e=1e-16);
            void QSD();
        private:
            void AddDecomposedGate(Gate gate);
            GateSequence remain_gates;
    };

    //DecomposedGates Universal(int n, UnitaryVector uvector);
}

#endif