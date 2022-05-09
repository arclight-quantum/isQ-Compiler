#![feature(label_break_value)]
extern crate clap;

mod exec;
mod frontend;
mod error;
use std::{fs::File, io::{Read, Write}, path::{Path, PathBuf}, ffi::OsStr};

use clap::*;
use error::*;
use std::str::FromStr;
use tempfile::tempfile;
use crate::frontend::resolve_isqc1_output;

fn opt_level(s: &str) -> Result<(), String> {
    usize::from_str(s)
        .map(|o| o>=0 && o<=3)
        .map_err(|e| e.to_string())
        .and_then(|result| match result {
            true => Ok(()),
            false => Err(format!(
                "Optimization level should be within 0-3"
            )),
        })
}

#[derive(Parser)]
#[clap(author, version, about)]
pub struct Arguments {
    #[clap(subcommand)]
    command: Commands,
}
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, ArgEnum)]
pub enum EmitMode {
    MLIR,
    MLIRQIR,
    LLVM,
    MLIROptimized,
    Binary,
    Out
}
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, ArgEnum)]
pub enum CompileTarget{
    QIR,
    OpenQASM3,
    QCIS
}


#[derive(Subcommand)]
pub enum Commands{
    Compile{
        input: String,
        #[clap(long, short)]
        output: Option<String>,
        #[clap(long, short='O', validator = opt_level)]
        opt_level: Option<usize>,
        #[clap(long, arg_enum, default_value = "out")]
        emit: EmitMode,
        #[clap(long, arg_enum, default_value = "qir")]
        target: CompileTarget,
        #[clap(long)]
        qcis_config: Option<String>
    },
    Simulate{
        #[clap(required(true))]
        qir_object: String,
        #[clap(long)]
        cuda: Option<usize>
    },
    Exec{
        #[clap(multiple_occurrences(true), required(true))]
        exec_command: Vec<String>
    },
    Run{
        input: String
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
    let root = std::env::var("ISQV2_ROOT").map_err(|_| NoISQv2RootError)?;
    
    match cli.command{
        Commands::Run{input}=>{
            let (input_path, default_output_path) = resolve_input_path(&input, "so")?;
            exec::system_exec_command(&root, "isqc", 
            &[OsStr::new("compile"), input_path.as_os_str(), OsStr::new("-o"), default_output_path.as_os_str()]
            ).map_err(ioErrorWhen("Calling isqc compile"))?;
            exec::system_exec_command(&root, "isqc", 
            &[OsStr::new("simulate"), default_output_path.as_os_str()]
            ).map_err(ioErrorWhen("Calling isqc simulate"))?;
        }
        Commands::Compile{input, output, opt_level, emit, target, qcis_config}=>'command:{
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
            let mut f = File::open(input_path).map_err(IoError)?;
            let mut buf = String::new();
            f.read_to_string(&mut buf).unwrap();
            let mlir = exec::exec_command_text::<&str>(&root, "isqc1", &[], &buf).map_err(ioErrorWhen("Calling isqc1"))?;
            let resolved_mlir = resolve_isqc1_output(input_path.file_name().unwrap().to_str().unwrap(), &buf, &mlir)?;
            if let EmitMode::MLIR = emit{
                writeln!(fout.get_file_mut(), "{}", resolved_mlir).map_err(IoError)?;
                fout.finalize();
                break 'command;
            }
            let qcis_flags = "-pass-pipeline=isq-recognize-famous-gates,isq-eliminate-neg-ctrl,isq-target-qcis,isq-convert-famous-rot,canonicalize,cse,isq-pure-gate-detection,canonicalize,isq-fold-decorated-gates,canonicalize,isq-decompose-ctrl-u3,isq-convert-famous-rot,isq-decompose-known-gates-qsd,isq-remove-trivial-sq-gates,isq-expand-decomposition,canonicalize,cse,isq-target-qcis,canonicalize,cse";
            let normal_flags = "-pass-pipeline=isq-recognize-famous-gates,isq-eliminate-neg-ctrl,isq-convert-famous-rot,canonicalize,cse,isq-pure-gate-detection,canonicalize,isq-fold-decorated-gates,canonicalize,isq-decompose-ctrl-u3,isq-convert-famous-rot,isq-decompose-known-gates-qsd,isq-remove-trivial-sq-gates,isq-expand-decomposition,canonicalize,cse";
            let flags = if let CompileTarget::QCIS = target {qcis_flags} else {normal_flags};
            let optimized_mlir = exec::exec_command_text(&root, "isq-opt", &[
                flags,
                "--mlir-print-debuginfo"
            ], &resolved_mlir).map_err(ioErrorWhen("Calling isq-opt"))?;
            if optimized_mlir.trim().is_empty(){
                return Err(InternalCompilerError("Optimization failed".to_owned()))?;
            }
            if let EmitMode::MLIROptimized = emit{
                writeln!(fout.get_file_mut(), "{}", optimized_mlir).map_err(IoError)?;
                fout.finalize();
                break 'command;
            }
            let llvm_mlir = exec::exec_command_text(&root, "isq-opt", &[
                "-pass-pipeline=symbol-dce,cse,isq-remove-gphase,lower-affine,isq-lower-to-qir-rep,cse,canonicalize,builtin.func(convert-math-to-llvm),isq-lower-qir-rep-to-llvm,canonicalize,cse,symbol-dce,llvm-legalize-for-export",
                "--mlir-print-debuginfo"
            ], &optimized_mlir).map_err(ioErrorWhen("Calling isq-opt"))?;
            if llvm_mlir.trim().is_empty(){
                return Err(InternalCompilerError("Generate LLVM IR failed".to_owned()))?;
            }
            if let EmitMode::MLIRQIR = emit{
                writeln!(fout.get_file_mut(), "{}", llvm_mlir).map_err(IoError)?;
                fout.finalize();
                break 'command;
            }
            let llvm = exec::exec_command_text(&root, "mlir-translate", &["--mlir-to-llvmir"], &llvm_mlir).map_err(ioErrorWhen("Calling mlir-translate"))?;
            if let EmitMode::LLVM = emit{
                writeln!(fout.get_file_mut(), "{}", llvm).map_err(IoError)?;
                fout.finalize();
                break 'command;
            }
            // linking with stub. This step we use byte output.
            let linked_llvm = exec::exec_command(&root, "llvm-link", &[
                format!("-"),
                format!("{}/share/isq-simulator/isq-simulator.bc", &root)
            ], llvm.as_bytes()).map_err(ioErrorWhen("Calling llvm-link"))?;
            let mut opt_args: Vec<String> = Vec::new();
            if let Some(o) = opt_level{
                opt_args.push(format!("-O{}", o));
            }
            let optimized_llvm = exec::exec_command(&root, "opt", &opt_args, &linked_llvm).map_err(ioErrorWhen("Calling opt"))?;
            let compiled_obj = exec::exec_command(&root, "llc", &["-filetype=obj"], &optimized_llvm).map_err(ioErrorWhen("Calling llc"))?;
            // create obj file.
            let mut tmpfile = tempfile::NamedTempFile::new().map_err(ioErrorWhen("Creating tempfile"))?;
            tmpfile.write_all(&compiled_obj).map_err(IoError)?;
            tmpfile.flush().map_err(IoError)?;
            // link obj file.
            let linked_obj = exec::exec_command(&root, "lld", &["-flavor", "gnu", "-shared", tmpfile.path().as_os_str().to_str().unwrap(), "-o", "-"], &[]).map_err(ioErrorWhen("Calling ld.lld"))?;
            drop(tmpfile);
            
            fout.get_file_mut().write_all(&linked_obj).map_err(IoError)?;
            fout.finalize();
            // post-processing.
            if let EmitMode::Out = emit{
                if let CompileTarget::QCIS = target{
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
                    v.push("--qcis".into());
                    v.push(qir_object);
                    let config_file = qcis_config;//ok_or(QCISConfigNotSpecified)?;
                    let output = if let Some(s) = config_file {
                        exec::exec_command_with_decorator(&root, "simulator", &v, &[], |child|{
                            child.env("QCIS_ROUTE_CONFIG", &s);
                        }).map_err(ioErrorWhen("running qcis classical part"))?
                    }else{
                        exec::exec_command(&root, "simulator", &v, &[]).map_err(ioErrorWhen("running qcis classical part"))?
                    };
                    
                    qcis_out.get_file_mut().write_all(&output).map_err(IoError)?;
                    qcis_out.finalize();
                }else if let CompileTarget::OpenQASM3 = target{
                    todo!("openqasm post-generation");
                }
            }

        }
        Commands::Simulate{qir_object, cuda}=>{
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
                v.push("--naive".into());
            }
            v.push(qir_object);
            exec::raw_exec_command(&root, "simulator", &v).map_err(IoError)?;
        }
        Commands::Exec{exec_command}=>{
            exec::raw_exec_command(&root, &exec_command[0], &exec_command[1..], ).map_err(IoError)?;
        }
    }
    return Ok(())
}
