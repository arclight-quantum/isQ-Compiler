use std::{sync::Arc, collections::BTreeMap};

use crate::lang::{location::Span, ast::{AST, LAST}};

use super::package::*;


pub type SymAST<'a> = AST<SymbolInfoAnnotation<'a>, SymbolInfoAnnotation<'a>>;  

pub fn last_to_symast<'a>(orig: Vec<LAST>)->Vec<SymAST<'a>>{
    orig.into_iter().map(|x| x.lift(&SymbolInfoAnnotation::empty, &SymbolInfoAnnotation::empty)).collect()
}


pub struct SymbolInfoAnnotation<'a>{
    pub symbol_info: Option<SymbolInfo<'a>>,
    pub loc: Span
}
impl<'a> SymbolInfoAnnotation<'a>{
    pub fn empty(loc: Span)->Self{
        SymbolInfoAnnotation { symbol_info: None, loc }
    }
}


#[derive(Clone, Copy)]
pub enum SymbolInfo<'a>{
    // Imported or local module entry.
    GlobalModule(&'a Module),
    // Imported or local symbol entry.
    GlobalSymbol(&'a Symbol),
    // Local value
    Local(usize)
}




pub struct SymbolTableLayer<'a>{
    //global_modules: BTreeMap<String, SymbolInfo<'a>>,

    /// Modules imported by glob: use foo::*;
    imported_glob_modules: Vec<&'a Module>,
    /// Imported entries and global packages.
    global_symbols: BTreeMap<String, SymbolInfo<'a>>,
    /// Local identifiers.
    locals: BTreeMap<String, SymbolInfo<'a>>,
    local_counter: usize,
}
impl<'a> SymbolTableLayer<'a>{
    pub fn new()->Self{
        Self{
            imported_glob_modules: Vec::new(), 
            global_symbols: BTreeMap::new(), 
            locals: Default::default(),
            local_counter: 0
        }
    }
    pub fn add_global_module(&mut self, sym: &str, entry: &'a Module)->Result<(), ()>{
        if self.global_symbols.contains_key(sym){
            return Err(());
        }else{
            self.global_symbols.insert(sym.to_owned(), SymbolInfo::GlobalModule(entry));
            return Ok(());
        }
    }
    pub fn add_global_symbol(&mut self, sym: &str, entry: &'a Symbol)->Result<(), ()>{
        if self.global_symbols.contains_key(sym){
            return Err(());
        }else{
            self.global_symbols.insert(sym.to_owned(), SymbolInfo::GlobalSymbol(entry));
            return Ok(());
        }
    }
    pub fn define_local(&mut self, sym: &str)->Option<usize>{
        if self.locals.contains_key(sym){
            return None;
        }else{
            let local = self.local_counter;
            self.local_counter+=1;
            self.locals.insert(sym.to_owned(), SymbolInfo::Local(local));
            return Some(local);
        }
    }
    
    // pull top-level declarations (procedure definitions, gate definitions, global imports) into symbol table.
    pub fn pull_declarations(&mut self, global: &'a PackageClosure, current_module: &'a Module, body: &mut Vec<SymAST<'a>>)->Result<(), ()>{
        for item in body.iter(){
            match &*item.0{
                crate::lang::ast::ASTNode::Gatedef { name, definition } => {
                    let ent = current_module.get_entry(&name.0).unwrap();
                    let sym = ent.as_symbol().unwrap();
                    self.add_global_symbol(&name.0, sym)?;
                },
                crate::lang::ast::ASTNode::Import(imp) => {
                    let imported_entries = imp.imports();
                    for import in imported_entries{
                        let mut parts_iter = import.0.iter().copied().chain(import.1.iter().map(|x| x.0));
                        let package_name = parts_iter.next().unwrap();
                        let (_, mut package) = global.visible_package(package_name).ok_or(())?;
                        let mut curr : Result<&Module, &Symbol> = Ok(package.root_module());
                        for part in parts_iter{
                            if let Ok(module) = curr{
                                let next = module.get_entry(part).ok_or(())?;
                                match next{
                                    ModuleEntry::Module(module) => curr = Ok(module),
                                    ModuleEntry::Symbol(symbol) => curr = Err(symbol),
                                }
                            }else{
                                return Err(());
                            }
                        }
                        if let Ok(module)=curr{
                            match import.1{
                                Some((import_leaf, imported_name)) => {
                                    // import one entry, probably renamed.
                                    let entry = module.get_entry(import_leaf).ok_or(())?;
                                    let imported_name = imported_name.unwrap_or(import_leaf);
                                    match entry{
                                        ModuleEntry::Module(new_module) => {
                                            self.add_global_module(imported_name, new_module)?;
                                        }
                                        ModuleEntry::Symbol(new_symbol) => {
                                            self.add_global_symbol(imported_name, new_symbol)?;
                                        }
                                    }
                                }
                                None => {
                                    // import everything.
                                    self.imported_glob_modules.push(module);
                                },
                            }
                        }else{
                            return Err(());
                        }
                        
                    }
                },
                crate::lang::ast::ASTNode::Defvar { definitions }=>{
                    for def in definitions.iter(){
                        let sym = current_module.get_entry(&def.0.var.0).unwrap().as_symbol().unwrap();
                        self.add_global_symbol(&def.0.var.0, sym)?;
                    }
                }
                crate::lang::ast::ASTNode::Procedure { name, args, body, deriving_clauses } => {
                    let sym = current_module.get_entry(&name.0).unwrap().as_symbol().unwrap();
                    self.add_global_symbol(&name.0, sym)?;
                },
                _=>{
                    // nope.
                }
            }
        }
        Ok(())
        /*
        for item in prog.iter(){
            match 
        }
        */
    }
    pub fn resolve(&self, name: &str)->Result<SymbolInfo<'a>, ()>{
        if let Some(local) = self.locals.get(name){
            return Ok(*local);
        }
        if let Some(global) = self.global_symbols.get(name){
            return Ok(*global);
        }
        let mut glob_matched = vec![];
        for module in self.imported_glob_modules.iter(){
            if let Some(ent) = module.get_entry(name){
                match ent{
                    ModuleEntry::Module(module) => {
                        glob_matched.push(SymbolInfo::GlobalModule(module))
                    },
                    ModuleEntry::Symbol(symbol) => {
                        glob_matched.push(SymbolInfo::GlobalSymbol(symbol))
                    },
                }
            }
        }
        if glob_matched.len()==1{
            return Ok(glob_matched[0])
        }else if glob_matched.len()>1{
            return Err(())
        }
        return Err(());
    }
    
}

pub struct SymbolTable<'a> {
    package: &'a PackageClosure,
    layers: Vec<SymbolTableLayer<'a>>
}

impl<'a> SymbolTable<'a>{
    /**
        Create a new symbol table and import all modules into the symbol table root.
    */
    pub fn new(package: &'a PackageClosure, module_path: Arc<ModulePath>)->Self{
        let mut package_layer = SymbolTableLayer::new();
        let module_layer = SymbolTableLayer::new();
        for external_package in package.visible_dependency_packages(){
            // must be successful.
            package_layer.add_global_module(external_package.0, external_package.2.root_module()).unwrap();
        }
        // pull all top level declarations to table.


        let table = SymbolTable{
            layers: vec![package_layer, module_layer],
            package
        };
        
        table
    }
}