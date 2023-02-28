use std::sync::Arc;

use crate::{lang::ast::{LAST, AST, ASTNode}, error::ISQFrontendError};

use self::package::{PackageClosure, Module, ModulePath};

use crate::error::Result;
pub mod package;
pub mod symboltable;

/*
 * isQ Symbol resolution steps:
 * Step 1: collect all depenedencies of the package.
 * Step 2: collect all definitions in all modules of current package.
 * Step 3: generate PackageClosure from the two results above.
 * Step 4: resolve all symbols in all modules against the PackageClosure.
 */


pub fn collect_definitions(package: &mut PackageClosure, path: &Arc<ModulePath>, prog: &Vec<LAST>)->Result<()>{
    let module = package.me_mut();
    let submodule = module.resolve_module_entry(path).unwrap();
    collect_definitions_module(submodule, prog)
}

fn collect_definitions_module(module: &mut Module, prog: &Vec<LAST>)->Result<()>{
    for item in prog.iter(){
        match &*item.0{
            ASTNode::Defvar { definitions }=> {
                for def in definitions.iter(){
                    let sym = module.insert_symbol(&def.0.var.0, true).ok_or_else(|| {
                        ISQFrontendError::RedefinedSymbol(def.0.var.1)
                    })?;
                }
            }
            ASTNode::Gatedef { name, definition } => {
                let ident = name;
                let sym = module.insert_symbol(&ident.0, true).ok_or_else(|| {
                    ISQFrontendError::RedefinedSymbol(ident.1)
                })?;
            }
            ASTNode::Procedure { name, args, body, deriving_clauses }=>{
                let ident = name;
                let sym = module.insert_symbol(&ident.0, true).ok_or_else(|| {
                    ISQFrontendError::RedefinedSymbol(ident.1)
                })?;
            }
            _ => {

            }
        }
    }
    Ok(())
}

