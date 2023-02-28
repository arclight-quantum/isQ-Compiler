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


pub enum SymbolInfo<'a>{
    // Imported or local module entry.
    GlobalModule(&'a Module),
    // Imported or local symbol entry.
    GlobalSymbol(&'a Symbol),
    // Local value
    Local(usize)
}

pub struct SymbolTableLayer<'a>{
    symbols: BTreeMap<String, SymbolInfo<'a>>,
    local_counter: usize,
}


impl<'a> SymbolTableLayer<'a>{
    pub fn new()->Self{
        Self{symbols: Default::default(), local_counter: 0}
    }
    pub fn add_global_module(&mut self, sym: &str, entry: &'a Module)->bool{
        if self.symbols.contains_key(sym){
            return false;
        }else{
            self.symbols.insert(sym.to_owned(), SymbolInfo::GlobalModule(entry));
            return true;
        }
    }
    pub fn add_global_symbol(&mut self, sym: &str, entry: &'a Symbol)->bool{
        if self.symbols.contains_key(sym){
            return false;
        }else{
            self.symbols.insert(sym.to_owned(), SymbolInfo::GlobalSymbol(entry));
            return true;
        }
    }
    pub fn define_local(&mut self, sym: &str)->Option<usize>{
        if self.symbols.contains_key(sym){
            return None;
        }else{
            let local = self.local_counter;
            self.local_counter+=1;
            self.symbols.insert(sym.to_owned(), SymbolInfo::Local(local));
            return Some(local);
        }
    }
    
    // pull top-level declarations (procedure definitions, gate definitions, global imports) into symbol table.
    pub fn pull_declarations(&mut self, body: &mut Vec<SymAST<'a>>)->Result<(), ()>{
        todo!();
        /*
        for item in prog.iter(){
            match 
        }
        */
    }
    
}

pub struct SymbolTable<'a> {
    package: &'a PackageClosure,
    layers: Vec<SymbolTableLayer<'a>>
}

impl<'a> SymbolTable<'a>{
    /*
        Create a new symbol table and import all modules into the symbol table root.
    */
    pub fn new(package: &'a PackageClosure, module_path: Arc<ModulePath>)->Self{
        let mut package_layer = SymbolTableLayer::new();
        let mut local_definition_layer = SymbolTableLayer::new();
        for external_package in package.visible_dependency_packages(){
            // must be successful.
            package_layer.add_global_module(external_package.0, external_package.2.root_module());
        }
        // pull all top level declarations to table.


        let table = SymbolTable{
            layers: vec![package_layer],
            package
        };
        
        table
    }
}