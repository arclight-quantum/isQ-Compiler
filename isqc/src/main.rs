extern crate clap;

mod exec;
mod frontend;
mod error;
mod mlir;
use std::{fs::File, io::Write, path::{Path, PathBuf}, ffi::OsStr, collections::VecDeque};

use clap::*;
use error::*;
use isq_version::ISQVersion;

use crate::frontend::resolve_isqc1_output;
use crate::mlir::resolve_mlir_output;

#[derive(Parser)]
#[clap(name = "isQ Compiler", version = ISQVersion::build_semver(),
long_version = Arguments::long_version(), 
about = "isQ Compiler")]
pub struct Arguments {
    #[clap(subcommand)]
    command: Commands,
}
impl Arguments{
    fn long_version()->String{
        format!("{}\nGit revision: {}", ISQVersion::build_semver(),
        ISQVersion::build_rev())
    }
}

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, ValueEnum)]
pub enum EmitMode {
    MLIR,
    MLIRQIR,
    LLVM,
    MLIROptimized,
    Binary,
    Out
}
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, ValueEnum)]
pub enum CompileTarget{
    QIR,
    OpenQASM3,
    QCIS,
    EQASM
}

#[derive(Subcommand)]
pub enum Commands{
    Compile{
        input: String,
        #[clap(long, short)]
        output: Option<String>,
        #[arg(long, short='O', value_parser = value_parser!(u64).range(0..=3))]
        opt_level: Option<u64>,
        #[clap(long, value_enum, default_value = "out")]
        emit: EmitMode,
        #[clap(long, value_enum, default_value = "qir")]
        target: CompileTarget,
        #[clap(long)]
        qcis_config: Option<String>,
        #[clap(long, short='I', action=ArgAction::Append)]
        inc_path: Option<Vec<String>>,
        #[clap(long, short, action=ArgAction::Append, allow_negative_numbers(true))]
        int_par: Option<Vec<i64>>,
        #[clap(long, short, action=ArgAction::Append, allow_negative_numbers(true))]
        double_par: Option<Vec<f64>>
    },
    #[clap(group(
        ArgGroup::new("simulator_type")
            .required(false)
            .args(&["cuda", "qcis"]),
    ))]
    Simulate{
        #[clap(required(true))]
        qir_object: String,
        #[clap(long)]
        cuda: Option<usize>,
        #[clap(long)]
        qcis: bool,
        #[clap(long)]
        shots: Option<i64>,
        #[clap(long)]
        debug: bool,
        #[clap(long, short, action=ArgAction::Append, allow_negative_numbers(true))]
        int_par: Option<Vec<i64>>,
        #[clap(long, short, action=ArgAction::Append, allow_negative_numbers(true))]
        double_par: Option<Vec<f64>>,
        #[clap(long, short, default_value = "1")]
        np: i64,
        #[clap(long, default_value = "25")]
        qn: usize
    },
    Exec{
        #[clap(action=ArgAction::Append, required(true))]
        exec_command: Vec<String>
    },
    Run{
        input: String,
        #[clap(long)]
        shots: Option<i64>,
        #[clap(long)]
        debug: bool,
        #[clap(long, short, default_value = "1")]
        np: i64,
        #[clap(long, default_value = "25")]
        qn: usize
    }
}

struct MayDropFile<'a>(&'a Path, Option<File>);

impl<'a> MayDropFile<'a>{
    pub fn new(p: &'a Path)->miette::Result<Self>{
        Ok(Self(p, Some(File::create(p).map_err(IoError)?)))
    }
    pub fn finalize(mut self){
        let f = self.1.take();
        drop(f);
    }
    pub fn get_file_mut(&mut self)->&mut File{
        self.1.as_mut().unwrap()
    }
}

impl<'a> Drop for MayDropFile<'a>{
    fn drop(&mut self) {
        let f = self.1.take();
        if let Some(x) = f{
            drop(x);
            std::fs::remove_file(self.0).unwrap();
        }
        
    }
}

fn resolve_input_path<'a>(input: &'a str, extension: &str)->miette::Result<(&'a Path, PathBuf)>{
    let input_path = Path::new(input);
            
    if input_path.extension()!=Some(OsStr::new("isq")){
        return Err(BadExtensionError)?;
    }
    
    let output = input_path.with_extension(extension);
    return Ok((input_path, output));
}

fn main()->miette::Result<()> {
    let cli = Arguments::parse();
    let root = std::env::var("ISQ_ROOT").map_err(|_| NoISQv2RootError)?;
    let llvm_root = std::env::var("LLVM_ROOT");
    let llvm_tool = |s: &str|{
        if let Ok(root) = &llvm_root{
            format!("{}/bin/{}", root, s)
        }else{
            s.to_owned()
        }
        
    };
    let mut queue = VecDeque::from([cli.command]);
    while !queue.is_empty() {
        let cmd = queue.pop_front().unwrap();
        match cmd{
            Commands::Run{input, shots, debug, np, qn}=>{
                let (input_path, default_output_path) = resolve_input_path(&input, "so")?;
                let so_path = default_output_path.to_string_lossy().to_string();
                queue.push_back(Commands::Compile { 
                    input: input_path.to_string_lossy().to_string(), output: Some(so_path.clone()),
                    opt_level: None, emit: EmitMode::Out, target: CompileTarget::QIR, qcis_config: None,
                    inc_path: None, int_par: None, double_par: None 
                });
                queue.push_back(Commands::Simulate { 
                    qir_object: so_path, cuda: None, qcis: false, shots: shots, debug: debug, int_par: None, 
                    double_par: None, np: np, qn: qn 
                })
            }
            Commands::Compile{input, output, opt_level, emit, target, qcis_config, inc_path, int_par, double_par}=>'command:{
                let (input_path, default_output_path) = resolve_input_path(&input, match emit{
                    EmitMode::Binary=>"so",
                    EmitMode::Out=> "so",
                    EmitMode::LLVM=>"ll",
                    EmitMode::MLIR=>"mlir",
                    EmitMode::MLIRQIR=>"ll.mlir",
                    EmitMode::MLIROptimized=>"opt.mlir"
                })?;
                
                let output = output.map_or_else(||{
                    default_output_path
                }, |x| {PathBuf::from(x)});
                // Finally, write obj into output.
                let mut fout = MayDropFile::new(&output)?;
                //let mut fout = File::create(output).map_err(IoError)?;
                let _f = File::open(input_path).map_err(IoError)?;
                //let mut buf = String::new();
                //f.read_to_string(&mut buf).unwrap();
                let fin = input_path.canonicalize().unwrap();

                let incpath = match inc_path {
                    Some(s) => s.join(":"),
                    None => "".to_string(),
                };

                let full_flags = ["-i", fin.as_os_str().to_str().unwrap(), "-I", &incpath, "--qcis"];
                let frontend_flags = if let CompileTarget::QCIS = target {&full_flags} else {&full_flags[..4]};
                let mlir = exec::exec_command_text::<&str>(&root, "isqc1", frontend_flags, "").map_err(io_error_when("Calling isqc1"))?;
                let resolved_mlir = resolve_isqc1_output(&mlir)?;
                if let EmitMode::MLIR = emit{
                    writeln!(fout.get_file_mut(), "{}", resolved_mlir).map_err(IoError)?;
                    fout.finalize();
                    break 'command;
                }
                
                let qcis_flags = "-pass-pipeline=builtin.module(cse,logic-lower-to-isq,isq-state-preparation,isq-remove-reset,func.func(affine-loop-unroll),isq-canonicalize,canonicalize,isq-oracle-decompose,isq-lower-switch,isq-recognize-famous-gates,isq-eliminate-neg-ctrl,isq-convert-famous-rot,canonicalize,cse,isq-pure-gate-detection,canonicalize,isq-fold-decorated-gates,canonicalize,isq-decompose-ctrl-u3,isq-convert-famous-rot,isq-decompose-known-gates-qsd,isq-remove-trivial-sq-gates,isq-target-qcis,isq-expand-decomposition,canonicalize,cse,canonicalize,cse)";
                let normal_flags = "-pass-pipeline=builtin.module(cse,logic-lower-to-isq,isq-state-preparation,isq-oracle-decompose,isq-lower-switch,isq-recognize-famous-gates,isq-eliminate-neg-ctrl,isq-convert-famous-rot,canonicalize,cse,isq-pure-gate-detection,canonicalize,isq-fold-decorated-gates,canonicalize,isq-decompose-ctrl-u3,isq-convert-famous-rot,isq-decompose-known-gates-qsd,isq-remove-trivial-sq-gates,isq-expand-decomposition,canonicalize,cse)";
                let qasm_flags = "-pass-pipeline=builtin.module(cse,logic-lower-to-isq,isq-state-preparation,isq-oracle-decompose,isq-lower-switch,isq-recognize-famous-gates,isq-eliminate-neg-ctrl,canonicalize,cse,isq-pure-gate-detection,canonicalize,isq-fold-decorated-gates,canonicalize,isq-decompose-ctrl-u3,isq-decompose-known-gates-qsd,isq-remove-trivial-sq-gates,isq-expand-decomposition,isq-cancel-redundant,canonicalize,cse)";

                let flags = match target {
                    CompileTarget::QCIS => qcis_flags,
                    CompileTarget::OpenQASM3 => qasm_flags,
                    CompileTarget::EQASM => qasm_flags,
                    _ => normal_flags,
                };
                //let flags = if let CompileTarget::QCIS = target {qcis_flags} else {normal_flags};
                let optimized_mlir = exec::exec_command_text(&root, "isq-opt", &[
                    flags,
                    "--mlir-print-debuginfo", "--format-out"
                ], &resolved_mlir).map_err(io_error_when("Calling isq-opt"))?;
                let resolved_mlir_opt = resolve_mlir_output(&optimized_mlir, "mlir optimization failed.".into())?;
                /* 
                if optimized_mlir.trim().is_empty(){
                    return Err(InternalCompilerError("Optimization failed".to_owned()))?;
                }*/
                if let EmitMode::MLIROptimized = emit{
                    writeln!(fout.get_file_mut(), "{}", resolved_mlir_opt).map_err(IoError)?;
                    fout.finalize();
                    break 'command;
                }

                if let CompileTarget::EQASM = target{
                    let eqasm_mlir = exec::exec_command_text(&root, "isq-opt", &["--target=eqasm", "--format-out"], &resolved_mlir_opt).map_err(io_error_when("Calling isq-codegen"))?;
                    let eqasm_ir = resolve_mlir_output(&eqasm_mlir, "eqasm generate error.".into())?;
                    let (_, eqasm_output_path) = resolve_input_path(&input, "eqasm")?;
                    let mut eqasm_out = MayDropFile::new(&eqasm_output_path)?;
                    writeln!(eqasm_out.get_file_mut(), "{}", eqasm_ir).map_err(IoError)?;
                    eqasm_out.finalize(); 
                    break 'command;
                }

                if let CompileTarget::OpenQASM3 = target{
                    let qasm_mlir = exec::exec_command_text(&root, "isq-opt", &["--target=openqasm3", "--format-out"], &resolved_mlir_opt).map_err(io_error_when("Calling isq-codegen"))?;
                    let qasm_ir = resolve_mlir_output(&qasm_mlir, "openqasm3 generate error.".into())?;
                    let (_, qasm_output_path) = resolve_input_path(&input, "qasm3")?;
                    let mut qasm_out = MayDropFile::new(&qasm_output_path)?;
                    writeln!(qasm_out.get_file_mut(), "{}", qasm_ir).map_err(IoError)?;
                    qasm_out.finalize(); 
                    break 'command;
                }


                let llvm_mlir = exec::exec_command_text(&root, "isq-opt", &[
                    // Todo: add symbol-dce pass back
                    //"-pass-pipeline=symbol-dce,cse,isq-remove-gphase,lower-affine,isq-lower-to-qir-rep,cse,canonicalize,builtin.func(convert-math-to-llvm),isq-lower-qir-rep-to-llvm,canonicalize,cse,symbol-dce,llvm-legalize-for-export",
                    "-pass-pipeline=builtin.module(cse,isq-remove-gphase,lower-affine,isq-lower-to-qir-rep,cse,canonicalize,func.func(convert-math-to-llvm),arith-expand,expand-strided-metadata,memref-expand,convert-math-to-funcs,isq-lower-qir-rep-to-llvm,canonicalize,cse,symbol-dce,llvm-legalize-for-export,global-thread-local)",
                    "--mlir-print-debuginfo",
                    "--format-out"
                ], &resolved_mlir_opt).map_err(io_error_when("Calling isq-opt"))?;
                
                let resolved_llvm = resolve_mlir_output(&llvm_mlir, "lower to llvm failed.".into())?;
                /*
                if llvm_mlir.trim().is_empty(){
                    return Err(InternalCompilerError("Generate LLVM IR failed".to_owned()))?;
                }*/

                if let EmitMode::MLIRQIR = emit{
                    writeln!(fout.get_file_mut(), "{}", resolved_llvm).map_err(IoError)?;
                    fout.finalize();
                    break 'command;
                }
                let llvm = exec::exec_command_text("", &llvm_tool("mlir-translate"), &["--mlir-to-llvmir"], &resolved_llvm).map_err(io_error_when("Calling mlir-translate"))?;
                if let EmitMode::LLVM = emit{
                    writeln!(fout.get_file_mut(), "{}", llvm).map_err(IoError)?;
                    fout.finalize();
                    break 'command;
                }
                // linking with stub. This step we use byte output.
                let linked_llvm = exec::exec_command("", &llvm_tool("llvm-link"), &[
                    format!("-"),
                    format!("{}/share/isq-simulator/isq-simulator.bc", &root)
                ], llvm.as_bytes()).map_err(io_error_when("Calling llvm-link"))?;
                let mut opt_args: Vec<String> = Vec::new();
                if let Some(o) = opt_level{
                    opt_args.push(format!("-O{}", o));
                }
                let optimized_llvm = exec::exec_command("", &llvm_tool("opt"), &opt_args, &linked_llvm).map_err(io_error_when("Calling opt"))?;
                let compiled_obj = exec::exec_command("", &llvm_tool("llc"), &["-filetype=obj", "--relocation-model=pic"], &optimized_llvm).map_err(io_error_when("Calling llc"))?;
                // create obj file.
                let mut tmpfile = tempfile::NamedTempFile::new().map_err(io_error_when("Creating tempfile"))?;
                tmpfile.write_all(&compiled_obj).map_err(IoError)?;
                tmpfile.flush().map_err(IoError)?;
                // link obj file.
                let linked_obj = exec::exec_command("", &llvm_tool("lld"), &["-flavor", "gnu", "-shared", tmpfile.path().as_os_str().to_str().unwrap(), "-o", "-"], &[]).map_err(io_error_when("Calling ld.lld"))?;
                drop(tmpfile);
                
                fout.get_file_mut().write_all(&linked_obj).map_err(IoError)?;
                fout.finalize();
                // post-processing.
                if let EmitMode::Out = emit{
                    if let CompileTarget::QCIS = target{
                        // get parameters
                        let par_int = match int_par{
                            Some(x) => x,
                            None => vec![]
                        };
                        let par_double = match double_par{
                            Some(x) => x,
                            None => vec![]
                        };
                        // run simulator 
                        let (_, qcis_output_path) = resolve_input_path(&input, "qcis")?;
                        let mut qcis_out = MayDropFile::new(&qcis_output_path)?;
                        let qir_object = output.to_str().unwrap();
                        let qir_object = if qir_object.starts_with("/"){
                            qir_object.to_owned()
                        }else{
                            format!("./{}", qir_object)
                        };
                        
                        let mut v = vec!["-e".into(), "__isq__entry".into()];
                        v.push("--qcisgen".into());
                        v.push(qir_object);
                        for val in par_int{
                            v.push("-i".into());
                            v.push(format!("{}", val));
                        }
                        for val in par_double{
                            v.push("-d".into());
                            v.push(format!("{}", val))
                        }
                        let config_file = qcis_config;//ok_or(QCISConfigNotSpecified)?;
                        let output = if let Some(s) = config_file {
                            exec::exec_command_with_decorator(&root, "simulator", &v, &[], |child|{
                                child.env("QCIS_ROUTE_CONFIG", &s);
                            }).map_err(io_error_when("running qcis classical part"))?
                        }else{
                            exec::exec_command(&root, "simulator", &v, &[]).map_err(io_error_when("running qcis classical part"))?
                        };
                        
                        let outs = std::str::from_utf8(&output).unwrap();
                        if outs.contains("Error"){
                            return Err(QCISGenerateError(outs.to_string()))?
                        }

                        qcis_out.get_file_mut().write_all(&output).map_err(IoError)?;
                        qcis_out.finalize();
                    }else if let CompileTarget::OpenQASM3 = target{
                        todo!("openqasm post-generation");
                    }
                }

            }
            Commands::Simulate{qir_object, cuda, qcis, shots, debug, int_par, double_par, np, qn}=>{

                let qir_object = if qir_object.starts_with("/"){
                    qir_object
                }else{
                    format!("./{}", qir_object)
                };
                let mut v = vec!["-e".into(), "__isq__entry".into()];
                if let Some(x)=cuda{
                    v.push("--cuda".into());
                    v.push(format!("{}", x));
                }else{
                    if qcis{
                        v.push("--qcis".into());
                    }else{
                        v.push("--naive".into());
                    }
                }
                if let Some(x)=shots{
                    v.push("--shots".into());
                    v.push(format!("{}", x));
                }
                if debug{
                    v.push("--debug".into());
                }

                v.push(qir_object);

                // get parameters
                let par_int = match int_par{
                    Some(x) => x,
                    None => vec![]
                };
                let par_double = match double_par{
                    Some(x) => x,
                    None => vec![]
                };
                for val in par_int{
                    v.push("-i".into());
                    v.push(format!("{}", val));
                }
                for val in par_double{
                    v.push("-d".into());
                    v.push(format!("{}", val))
                }
                v.push("--np".into());
                v.push(format!("{}", np));

                v.push("--qn".into());
                v.push(format!("{}", qn));

                exec::raw_exec_command(&root, "simulator", &v).map_err(IoError)?;
            }
            Commands::Exec{exec_command}=>{
                exec::raw_exec_command(&root, &exec_command[0], &exec_command[1..], ).map_err(IoError)?;
            }
        }
    }
    return Ok(())
}
