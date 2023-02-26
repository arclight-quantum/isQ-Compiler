/*
 * Rationale for global symbol: we want to dump the AST or all modules to MLIR, at least in the form where different MLIR modules can be concatenated to form a bigger module.
 * In this sense, all symbols defined in all packages must be mangled such that there will be no symbol conflict when placed together.
 * Moreover, the symbol table should be able to be exported out, such that external libraries can link against the symbol table and the compiled MLIR.
 */

use std::sync::Arc;
use std::collections::BTreeMap;
use ethnum::u256;

use nom::{InputIter};
#[derive(Clone)]
pub struct ModulePath(Vec<Arc<String>>);

#[derive(Clone)]
pub struct Package{
    name: String,
    hash: u256
}

impl Package{
    pub fn new(name: &str, hash: u256)->Self{
        Package{name: name.to_owned(), hash}
    }
    fn hash_base62(&self)->String{
        if self.hash==0{
            return "_".to_owned();
        }
        let val = self.hash-1;
        let bytes = val.to_le_bytes();
        base_62::encode(&bytes)
    }
}
pub struct Symbol{
    package: Arc<Package>,
    path: Arc<ModulePath>,
    leaf: String,
    mangled: String,
}



impl Symbol{
    pub fn new(package: Arc<Package>, path: Arc<ModulePath>, leaf: String)->Self{
        let mangled = Self::mangle(&package, &path, &leaf);
        Symbol{
            package, path, leaf, mangled
        }
    }
    pub fn mangled_name(&self)->&str{
        &self.mangled
    }
    pub fn symbol_name(&self)->&str{
        &self.leaf
    }
    pub fn parent_module_path(&self)->&Arc<ModulePath>{
        &self.path
    }
    pub fn package(&self)->&Arc<Package>{
        &self.package
    }
    fn mangled_ident(ident: &str)->String{
        let head : Option<char> = ident.iter_elements().next();
        let tok = if let Some(chr) = head{
            if chr.is_digit(10) || chr=='_'{
                "_"
            }else {
                ""
            }
        }else {
            ""
        };
        format!("{}{}{}", ident.len(), tok, ident)
    }
    // https://rust-lang.github.io/rfcs/2603-rust-symbol-name-mangling-v0.html
    pub fn mangle(package: &Package, path: &ModulePath, leaf: &str)->String{
        let mut name = format!("Ps{}_{}", package.hash_base62(), Self::mangled_ident(&package.name));
        for part in path.0.iter(){
            name = format!("Nm{}{}", name, Self::mangled_ident(part));
        }
        name = format!("Ns{}{}", name, Self::mangled_ident(leaf));
        name = format!("__ISQ{}", name);
        name
    }
}

pub enum ModuleEntry{
    Module(Module),
    Symbol(Symbol)
}
impl ModuleEntry{
    pub fn as_module(&self)->Option<&Module>{
        match self{
            ModuleEntry::Module(x)=>Some(x),
            _=>None
        }
    }
    pub fn as_module_mut(&mut self)->Option<&mut Module>{
        match self{
            ModuleEntry::Module(x)=>Some(x),
            _=>None
        }
    }
    pub fn as_symbol(&self)->Option<&Symbol>{
        match self{
            ModuleEntry::Symbol(x)=>Some(x),
            _=>None
        }
    }
    pub fn as_symbol_mut(&mut self)->Option<&mut Symbol>{
        match self{
            ModuleEntry::Symbol(x)=>Some(x),
            _=>None
        }
    }
    pub fn new_module(module: Module)->Self{
        Self::Module(module)
    }
    pub fn new_symbol(symbol: Symbol)->Self{
        Self::Symbol(symbol)
    }
}

pub struct Module{
    package: Arc<Package>,
    module_path: Arc<ModulePath>,
    symbols: BTreeMap<String, ModuleEntry>,
}

impl Module{
    pub fn path(&self)->&ModulePath{
        &self.module_path
    }
    pub fn get_entry(&self, sym: &str)->Option<&ModuleEntry>{
        self.symbols.get(sym)
    }
    pub fn get_entry_mut(&mut self, sym: &str)->Option<&mut ModuleEntry>{
        self.symbols.get_mut(sym)
    }

    pub fn entry_module(&mut self, name: &str)->Option<&mut Module>{
        if !self.symbols.contains_key(name){
            let mut path = (*self.module_path).clone();
            path.0.push(Arc::new(name.to_owned()));
            self.symbols.insert(name.to_owned(), ModuleEntry::new_module(Module{
                package: Arc::clone(&self.package),
                module_path: Arc::new(path),
                symbols: Default::default()
            }));
        }
        self.get_entry_mut(name).and_then(|x| x.as_module_mut())
    }
    pub fn insert_symbol(&mut self, name: &str)->Option<&mut Symbol>{
        if self.symbols.contains_key(name){
            return None;
        }
        let sym = Symbol::new(Arc::clone(&self.package), Arc::clone(&self.module_path), name.to_owned());
        self.symbols.insert(name.to_owned(), ModuleEntry::new_symbol(sym));
        self.get_entry_mut(name).and_then(|x| x.as_symbol_mut())
    }
}


pub struct PackageSymbolTable{
    package: Arc<Package>,
    root: Module,
}



impl PackageSymbolTable{
    pub fn package(&self)->&Arc<Package>{
        &self.package
    }
    pub fn new(package_name: &str, package_hash: u256)->Self{
        let package = Arc::new(Package::new(
            package_name,
            package_hash
        ));
        PackageSymbolTable { package: Arc::clone(&package) , root: Module {
            package: Arc::clone(&package),
            module_path: Arc::new(ModulePath(vec![])),
            symbols: Default::default()
        } }
    }
    pub fn root_module(&self)->&Module{
        &self.root
    }
    pub fn root_module_mut(&mut self)->&mut Module{
        &mut self.root
    }
}


#[cfg(test)]
mod tests{
    use super::*;

    #[test]
    fn test_moduletree(){
        let mut package = PackageSymbolTable::new("testPackage", u256::new(114514));
        let sym = package.root_module_mut().insert_symbol("foo").unwrap();
        println!("{}", sym.mangled_name());
        let module = package.root_module_mut().entry_module("foobar").unwrap();
        let sym2 = module.insert_symbol("bar").unwrap();
        println!("{}", sym2.mangled_name());
    }
}