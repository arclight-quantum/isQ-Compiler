<script type="text/javascript" src="http://cdn.mathjax.org/mathjax/latest/MathJax.js?config=TeX-AMS-MML_HTMLorMML"></script>
<script type="text/x-mathjax-config">
    MathJax.Hub.Config({ tex2jax: {inlineMath: [['$', '$']]}, messageStyle: "none" });
</script>

isQ Programming Language
============================

This section mainly introduces isQ's grammar which is divided into the following parts:

* [Package](#package)
* [Import](#import)
* [Types](#type)
* [Classical Operation](#classical)
* [Quantum Operation](#operation)
* [Procedure](#procedure)
* [Control Flow](#controlflow)
* [Modifier](#modifier)
* [Oracle](#oracle)
* [Parameter](#parameter)

<br/>

<h2 id = "package"></h2>

Package
---------------------------
Each isQ file belongs to a package. The root of the package is specified with keyword `package` at the begining of the file. For example, the first lines of `/some/path/aaa/bbb/aaa/ccc.isq` are
```Java
// /some/path/aaa/bbb/aaa/ccc.isq
package aaa;
```
It means that the root of the package is `/some/path/aaa/bbb/aaa`. The root is matched with file or folder name in the **abosolute** path of the isQ file, from lower to higher hierachies. In this example, there are two folders of the name `aaa`. The root is the one closer to the file. If the specified package name cannot match any file or folder name, the compiler will report an error. If no package is specified, the default package name is the file name.

The global variables and procedures can be referred to with *qualified names* from another isQ file, after being imported. Continue with the previous example,
```Java
// /some/path/aaa/bbb/aaa/ccc.isq
package aaa;
int g;
```
The global variable `g` can be referred to as `aaa.ccc.g`. This qulified name is constructed with the relative path from the package root (replacing `/` with `.`) plus the variable name.

<br/>

<h2 id = "import"></h2>

Import
---------------------------
An isQ file can use the variables and procedures defined in another isQ file by importing it. The syntax is like this example:
```Java
// /some/path/aaa/bbb/ddd/eee.isq
package ddd;
import aaa.ccc; // /some/path/aaa/bbb/aaa/ccc.isq
import std;     // ${ISQ_ROOT}/lib/std.isq
```
The root of this file is `/some/path/aaa/bbb/ddd` which is placed in the **same** folder (i.e., `/some/path/aaa/bbb/`) as the example in the [Package](#package) part. It specifies the imported file with the **relative** path from the **parent** folder of the root, replacing `/` with `.` and omiting the `.isq` suffix. Therefore, the above line imports `/some/path/aaa/bbb/aaa/ccc.isq`. The user can also uses the relative path from the *isQ library*, which is specified by environmental variable `${ISQ_ROOT}/lib`. A standard library file, `std.isq`, is placed in this folder. It contains the defination of basic gates such as __*H*__ and __*X*__. Therefore, it should be imported by nearly all the isQ files.

Once a file is imported, its global variables and procedures can be used directly. For example, the `eee.isq` mentioned above can use `g` direclty in its program. However, if multiple variables that are defined in different imported files share the same name, they have to be referred to using qualified names, e.g., `aaa.ccc.g` and `some.other.g`.

<br/>

<h2 id = "type"></h2>

Types
---------------------------

isQ supports four primitive types:

* __*int*__: a 64-bit signed integer, e.g. __-1__, __1__;
* __*bool*__: a boolean value that can be either __true__ or __false__;
* __*double*__: a double precision floating point number, e.g. __-0.1__, __3.1415926__;
* __*qbit*__: an opaque type that represents a quantum bit; 

Each type supports one-dimensional array form with __*[N]*__ after the variable where N is a specific integer. Users can use these type to define variables anywhere. All qubits are set to default value __*|0>*__. For example, we could define variables like:

```C++
// global variable
int a, b; // int variable
double d = 3.14159; // double variable with init value 3.14159
qbit q, p[3]; // qbit and qbit array variable

procedure main(){
    
    // local variable with init value 3 
    int c = 3;
    ...
}
```

<br/>

<h2 id = "classical"></h2>

Classical Operation
---------------------------

isQ supports the following operators for classical types:

- Arithmetic operators: __*+*__, __*-*__, __*\**__, __*/*__, __*\*\**__ (power), __*%*__
- Comparison operators: __*==*__, __*!=*__, __*>*__, __*<*__, __*>=*__, __*<=*__
- Logical operators: __*&&*__, __*and*__, __*||*__, __*or*__, __*!*__, __*not*__
- Bitwise operators: __*&*__, __*|*__, __*^*__(XOR)
- Shift operators: __*>>*__, __*<<*__

Their semantics, operand types and precedence are consistent with common programming languages such as C and Python.

isQ has built-in automatic type conversion, which can convert __*bool*__ to __*int*__ and __*int*__ to __*double*__. Specifically, __*true*__ is converted to 1 and __*false*__ to 0. Moreover, isQ provide __*print*__ function to facilitate users to print intermediate results. Users can do arithmetic and print like this:

```C++
procedure main(){

    int a = 2*(3+4) % 3;
    double d = 3.14 * a;

    print a;
    print d;
}
```

<br/>

<h2 id = "operation"></h2>

Quantum Operation
---------------------------

The quantum operation in isQ is simple, users can apply a gate or do measurement on qubits in a way similar to function call. All quantum operations are inside procedures and all qubits used are defined beforehand. When doing measurement, the result must be stored in an int variable.

### basic operation

isQ supports some basic gate: __*X*__, __*Y*__, __*Z*__, __*H*__, __*S*__, __*T*__, __*Rx*__, __*Ry*__, __*Rz*__, __*CNOT*__, __*TOFFOLI*__, __*U3*__(the definition is the same as openqasm3.0), and two non-unitary operation: __*M*__(measure), __*|0>*__(set qubit to |0>). Users can directly use these gate like this:

```C++
qbit q[2];
procedure main(){
    H(q[0]);
    CNOT(q[0], q[1]);
    int x = M(q[0]);
    int y = M(q[1]);
}
```

### defgate
isQ allows users to define gate by using keyword __*defgate*__. gate element can be an int/double/complex value or an arithmetic expression. complex value can be written in a way similar to python. There are three points to note.

* The user-define-gate should be unitary and its size should be a power of 2. 
* users should define gate outside the procedures and then use it inside. 
* user-define-gate's names can not conflict with the names of built-in gates

For example, we can define a 2-qubit gate and use it like this:

```C++
// define gate Rs
defgate Rs = [
    0.5+0.8660254j,0,0,0;
	0,1,0,0;
	0,0,1,0;
    0,0,0,1
];

qbit q[2];

procedure main(){
    // apply Rs on q;
    Rs(q[0], q[1]);
}

```

<br/>

<h2 id = "procedure"></h2>

Procedure
---------------------------

The main body of isQ program is composed of procedures, which is similar to functions in C. The entry procedure is named __*main*__, which has no paramters and output.

Users can define their own procedures, and procedures may have no output or return with classical type. When a procedure has no output, it uses keyword __*procedure*__ at the beginning of definition, otherwise, it uses [classical type](#type) mentioned above. User-procedure can has parameters with [any type](#type) and can be called by others or by itself.

```C++
// proceduer with two qbit as paramters and has no return
// paramtesr can also be written like (a: qbit, b: qbit)
procedure swap(qbit a, qbit b){
    ...
}

// proceduer with two classical paramters and return with a double
double compute(int a, b: double []){
    double s = 0.0;
    ...
    return s;
}

// entry procedure
procedure main(){

    qbit q[2];
    // call swap
    swap(q[0], q[1]);

    double b[2];
    // call compute
    double c = compute(1, b);
}

```


### deriving

User-procedure can be purely quantum. Such kind of procedures works as a gate, and we can do some operations on these procedures like on ordinary quantum gates, like [modifiers](#modifier).

isQ provides a keyword __*deriving gate*__, converting a procedure to quantum gate, and so that, user can add [modifiers](#modifier) when calling procedure. For example, if we now need a control-swap, we can write codes like:

```C++

// pure quantum procedure that do swap on two qbit
procedure swap(qbit a, qbit b){
    CNOT(b, a);
    CNOT(a, b);
    CNOT(b, a);
} deriving gate

procedure main(){
    qbit q[3];
    // set q -> 110
    X(q[0]);
    X(q[1]);
    // ctrl swap, set q -> 101 
    ctrl swap(q[0], q[1], q[2])
}

```

<br/>

<h2 id = "controlflow"></h2>

Control Flow
---------------------------

isQ provides three kinds of classical control flow:

* __*if-statement*__
* __*for-loop*__
* __*while-loop*__

### if-else

If-else statement in isQ is similar to that in C. The differences are as follows:

* Unlike C, the braces {...} cannot be omitted, even if there is single statement following the condition.
* isQ only supports single-condition guard yet.
* isQ does not have else if 


```C++

procedure main(){
    int a = 1;
    int b;
    if (a > 0){
        b = 1;
    }else{
        b = -1;
    }

    bool c = true;
    if (c){
        b = 2;
    }
}

```


### for-loop

For-loop in isQ is similar to that in python. User can uses a non-defined variable as the iterating variable, and use two integer value a, b as loop range [a, b), where a must be less than b. The step size is optional and defaulted to 1. Like for-loop in other languages, keywords __*break*__ and  __*continue*__ are supported.

```C++
procedure main(){
    int a = 0;

    for i in 0:10{
        a = a + 1;
        if (a > 5){
            break;   
        }
    }

    for j in 0:10:3{
        a = a - 1;
    }
}
```

### while-loop

While-loop in isQ is similar to that in C. The differences are as follows:

* Unlike C, the braces {...} cannot be omitted, even if there is single statement following the condition.
* only support single condition yet.

```C++
procedure main(){

    int a = 1;
    qbit q;
    while (a != 1){
        H(q);
        a = M(q);
    }
}
```


<br/>

<h2 id = "modifier"></h2>

Modifiers
---------------------------

Controlled gates are common in quantum circuit. As we introduced before, users can [define gate](#defgate), so theoretically, any controlled gate can be defined. But it requires users to calculate the whole matrix. To simplify this work, isQ provide gate modifiers like some other quantum programming languages.

There are three gate modifiers in isQ:

* __*ctrl*__: modify gate to controlled-gate, can has a paramter representing the number of control qubits(defualt is 1).
* __*nctrl*__: modify gate to neg-controlled-gate, can has a paramter representing the number of control qubits(defualt is 1).
* __*inv*__: modify gate to conjugate transpose.


Controlled gates can be easily defined by __*ctrl*__ and __*nctrl*__. And of course, you can compound these modifiers. For example:

```C++
// define gate Rs
defgate Rs = [
    0.5+0.8660254j,0,0,0;
	0,1,0,0;
	0,0,1,0;
    0,0,0,1
];

qbit q[3];

procedure main(){
    // apply C1(Rs) on q[1]
    ctrl Rs(q[0], q[1]);
    // apply C2(Rs) on q[2]
    ctrl<2> Rs(q[0], q[1], q[2]);
    // apply Rs+ on q[1];
    inv Rs(q[1])
    // apply C1(Rs)+ on q[1];
    inv ctrl Rs(q[0], q[1]);
}

```


<h2 id = "oracle"></h2>

Oracle
---------------------------

### value table

isQ supports simple quantum oracle definitions. For those simple oracles whose function like $f: \\{0,1\\}^n \rightarrow \\{0,1\\}^m$, keyword __*oracle*__ can be used like this:

```C++
oracle g(2, 1) = [0, 1, 0, 0];

qbit q[3];

procedure main(){
    ...
    g(q[0], q[1], q[2]);
    ...
}

```

In the above example, __*g*__ is the oracle name, and there are two integer parameters __*n, m*__, where __*n*__ represents the number of work qubits and __*m*__ represents the number of ancilla qubits. The value list is $\{f(i)\} {\ } for {\ } i {\ } in [0, 2^n) $, and every $f(i)$ must be in $[0, 2^m)$. Usually, $m$ is 1, and the list is a truth table like above.

### oracle function

Alternatively, you can define an oracle using a function. This function will be used to generate the value table shown above. For example, we can rewrite the oracle __*g*__  as follows:

```C++
oracle g(2, 1): x
{
    return x == 1;
}
```

The keyword, oracle name, input and output qubit numbers are specified in the same way. The __*x*__  defined here represents the input value of the function, which is an __*int*__ of 2 bits. The body of the function describes how the output value is calculate based on the input __*x*__. In this case, it is a judgement that whether the input value is 1. The judgement result, a __*bool*__, will be converted into an __*int*__. The compiler will assign __*x*__ to all the values of 2-bit long (i.e., 0, 1, 2, and 3), and evaluates corresponding results. The resulted values form a table like the one shown in the previous section.

Temporary variables can be defined in oracle functions. Control flow structures such as __*if*__, __*while*__ and __*for*__ are all supported. You can also use all the boolean and arithmatic operators. However, __*qbit*__ types cannot be used as an oracle function defines a **classical** function. Procedure calls are also fobidded because they might involve quantum operations or be too complex for oracle decomposition.



<h2 id = "parameter"></h2>

Parameter
---------------------------

isQ supports compiling with parameters and passing values at runtime. If you want to do this, you can define two arrays in the parameter list of the `procedure main`, and you can use them in the function body, like this:

```C++
procedure main(int i_par[], double d_par[]){
	...
	Rx(d_par[0], q);
	if (i_par[1] == 2){...}
	...
}
```

When simulating, you can pass values by `-i` or `-d`. isQ will gather  values and generate __*int*__ and __*double*__ arrays (can be empty). For the above example, you can pass values like this:

```bash
isqc simulate -i 1 -i 2 -d 1.3 xxx.isq
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