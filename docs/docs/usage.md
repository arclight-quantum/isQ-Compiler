Usage
=======

[TOC]

## Compile

The isQ compiler receives the isQ source file as input, and the instruction format is as follows:

    isqc compile [options] <Input>

options:

* `--emit <FORMAT>` : output content format, format value can be `mlir, mlirqir, llvm, mlir-optimized, binary, out` [default: `out`]
* `-o, --output <OUTFILE>` : output file
* `-O, --opt-level <OPT_LEVEL>` : llvm opt-level such as `-O1, -O2, -O3` etc.
* `--target <TARGET_IR>` : target ir, now support `qir, open-qasm3, qcis` [default: `qir`]
* `--qcis-config <MAPPING_CONFIG_FILE>` : qcis mapping config file
* `-I, --inc-path <INC_PATH>`: library path of isQ used in the source code, a default path is `$ISQ_ROOT/lib`
* `-i <INT>`: int type paramter, which is used when compiled to qcis
* `-d <DOUBLE>` double type paramter, which is used when compiled to qcis

#### compile to qir

[QIR](https://github.com/qir-alliance/qir-spec) is a new Microsoft-developed intermediate representation for quantum programs. Users can compile source file  to qir using the following command

```bash
# compile to qir, default output file is source.so
isqc compile source.isq
```

or may compile to intermediate results, such as mlir

```bash
# compile to mlir, default output file is source.mlir
isqc compile source.isq --emit mlir
```

#### compile to qcis

[QCIS](https://quantumcomputer.ac.cn/UserBook.html)  is specially tailored for the superconducting quantum hardware at the University of Science and Technology of China. Users can compile source file  to qcis using the following command

```bash
# compile to qcis, default output file is source.qcis
isqc compile --target qcis source.isq
```

Qcis instructions can run on real superconducting hardware, so isQ compiler provides qubit-mapping function. If want to use this feature, users should feed a configuration file `mapping_config_file`. The file is usually in JSON format, and needs to include the following fields:

```json
{
// required
    "qbit_num": 12, // qubit number on hardware
    "topo": [[1,2],[2,3],[3,4],[4,5],[5,6],[6,7],[7,8],[8,9],[9,10],[10,11],[11,12]], // topology of hardware
// option 
    "init_map": "simulated_annealing" // the way to get init mapping
}
```
and the configuration file needs to be specified through `--qcis config`
```bash
isqc compile --target qcis --qcis config mapping_config_file source.isq
```

QCIS hardware does not support feedback control, so there are some restrictions on the usage of isQ

* can not use reset
* can not print
* can not use measurement results as right value, like assignment, condition

```c++
procedure main(){
	// These operations will report errors
	qbit q;
	int a = M(q);
	print a;
}
```

isQ supports compiling with parameters. When compiling to QCIS, users need pass parameter values in order to generate a definite circuit.

```c++
qbit q;

procedure main(int x[], double d[]){
	if (x[0] == 1){
		X(q);
	}else{
		Rx(d[0], q);
	}
	M(q);
}
```
compile using the following command
```bash
# pass values when compiling
isqc compile --target qcis -i 0 -d 1.23 source.isq
```


## Simulate

The simulator of isQ provides the simulation of `qir` or `qcis`. The instruction format is as follows

	isqc simulate [options] <Input>

options:

* `--shots <NUM>`: simulate times, default is 1
* `--debug`: use debug mod and get the `print` result
* `--cuda <NUM>`: use gpu, `NUM` is the number of qubits to be simulated
* `-i <INT>`: int type paramter
* `-d <DOUBLE>` double type paramter
* `--qcis` simulate qcis


#### simulate qir

The input must be a .so file generated by compiler, and can use as follows:

```bash
# simulate in cpu
isqc simulate ./source.so
# simulate in gpu
isqc simulate --cuda 10 ./source.so
# run 10 times and use debug mod
isqc simulate --shots 10 --debug ./source.so
```

isQ supports compiling with parameters and passing values at runtime. The following program is compiled as `qir` with parameters, and users can pass values during simulation through `-i, -d`. isQ will gather values and generate int and double arrays (can be empty).

```c++
qbit q;

procedure main(int x[], double d[]){
	if (x[0] == 1){
		X(q);
	}else{
		Rx(d[0], q);
	}
	M(q);
}
```
simulate using the following command

```bash
# pass values when simulating
isqc simulate -i 0 -d 1.23 source.so
```


#### simulate qcis

isQ simulator can also simulate `qcis` through  `--qcis`.  The input must be a .qcis file and isQ will check the format of the QCIS instructions. 

```bash
# simulate qcis, 1000 times
isqc simulate --qcis --shots 1000 ./source.qcis
```

#### result
The output is the statistical result of measurement in source.isq, like `{"00": 4, "11": 6}`. The results of all measurement operations will be added to the result string, even if it is an assignment operation like `int a = M(q);`

```c++
import std;

procedure main(){
    qbit q[2];
    H(q[0]);
    ctrl X(q[0],q[1]);
    int a = M(q[0]);
    int b = M(q[1]);
    print a - b;
}
```

The result of above isQ program can be `{"11": 47, "00": 53}`. In this program, there is a print operation, the result of this operation will not output until using debug mode through `--debug`. In debug mode, print result of each simulation will output to stderr, the format like `{ith simulate print: 0}`. When the shot_num is large, there will be many print result. It is suitable that redirect the stderr to a file. Taking the above program as an example:

```bash
# redirect print result to a file
isqc simulate --shots 100 --debug ./bell.so 2>res.txt
```

output
```bash
{"11": 47, "00": 53}
```
res.txt
```bash
0th simulation print:
0
1th simulation print:
0
...
99th simulation print:
0
```


## Run

Users can use isQ’s `run` command to compile and simulate quantum programs (compile to `qir`) 
```bash
	isqc run [options] <Input>
```

options

* `--shots <NUM>`: simulate times, default is 1
* `--debug`: use debug mod and get the `print` result

isQ will compile source file to `qir` first, and then simulate. 

```bash
# simulate in cpu
isqc run --shots 100 ./source.isq
```

<script type="text/javascript" id="MathJax-script" async
  src="https://cdnjs.cloudflare.com/ajax/libs/mathjax/3.0.0/es5/tex-mml-chtml.js">
</script>
<script>
MathJax = {
  tex: {
    inlineMath: [['$', '$'], ['\\(', '\\)']]
  }
};
</script>
<script id="MathJax-script" async
  src="https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-chtml.js">
</script>