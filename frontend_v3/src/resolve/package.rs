/*
 * Rationale for global symbol: we want to dump the AST or all modules to MLIR, at least in the form where different MLIR modules can be concatenated to form a bigger module.
 * In this sense, all symbols defined in all packages must be mangled such that there will be no symbol conflict when placed together.
 * Moreover, the symbol table should be able to be exported out, such that external libraries can link against the symbol table and the compiled MLIR.
 */

use std::sync::Arc;
use std::collections::BTreeMap;
use ethnum::u256;
use std::collections::HashMap;

use nom::{InputIter};

/* 
 * Path relative to the module root.
 */
#[derive(Clone)]
pub struct ModulePath(Vec<Arc<String>>);

#[derive(Clone, Hash, Eq, PartialEq)]
pub struct PackageMetadata{
    pub name: String,
    pub hash: u256
}

impl PackageMetadata{
    pub fn new(name: &str, hash: u256)->Self{
        PackageMetadata{name: name.to_owned(), hash}
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
    package: Arc<PackageMetadata>,
    path: Arc<ModulePath>,
    leaf: String,
    mangled: String,
    hidden: bool
}



impl Symbol{
    pub fn new(package: Arc<PackageMetadata>, path: Arc<ModulePath>, leaf: String, hidden: bool)->Self{
        let mangled = Self::mangle(&package, &path, &leaf);
        Symbol{
            package, path, leaf, mangled, hidden
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
    pub fn package(&self)->&Arc<PackageMetadata>{
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
    pub fn mangle(package: &PackageMetadata, path: &ModulePath, leaf: &str)->String{
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
    package: Arc<PackageMetadata>,
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
    pub fn insert_symbol(&mut self, name: &str, hidden: bool)->Option<&mut Symbol>{
        if self.symbols.contains_key(name){
            return None;
        }
        let sym = Symbol::new(Arc::clone(&self.package), Arc::clone(&self.module_path), name.to_owned(), hidden);
        self.symbols.insert(name.to_owned(), ModuleEntry::new_symbol(sym));
        self.get_entry_mut(name).and_then(|x| x.as_symbol_mut())
    }
}


pub struct PackageSymbolTable{
    package: Arc<PackageMetadata>,
    root: Module,
}



impl PackageSymbolTable{
    pub fn package(&self)->&Arc<PackageMetadata>{
        &self.package
    }
    pub fn new_from_metadata(package: Arc<PackageMetadata>)->Self{

        PackageSymbolTable { package: Arc::clone(&package) , root: Module {
            package: Arc::clone(&package),
            module_path: Arc::new(ModulePath(vec![])),
            symbols: Default::default()
        } }
    }
    pub fn new(package_name: &str, package_hash: u256)->Self{
        let package = Arc::new(PackageMetadata::new(
            package_name,
            package_hash
        ));
        Self::new_from_metadata(package)
    }
    pub fn root_module(&self)->&Module{
        &self.root
    }
    pub fn root_module_mut(&mut self)->&mut Module{
        &mut self.root
    }
    pub fn resolve_module<'a>(&mut self, path: &'a ModulePath)->Result<&Module, (&'a [Arc<String>], bool)>{
        let mut module = self.root_module();
        let mut good = 0;
        for part in path.0.iter(){
            good += 1;
            let next = module.get_entry(&**part);
            match next{
                None => return Err((&path.0[0..good], false)),
                Some(ModuleEntry::Module(module_2)) => {
                    module = module_2
                }
                Some(ModuleEntry::Symbol(_)) => return Err((&path.0[0..good], true))
            }
            
        }
        Ok(module)
    }
    pub fn resolve_module_entry<'a>(&mut self, path: &'a ModulePath)->Result<&mut Module, &'a [Arc<String>]>{
        let mut module = self.root_module_mut();
        let mut good = 0;
        for part in path.0.iter(){
            good += 1;
            module = module.entry_module(&**part).ok_or_else(||{
                &path.0[0..good]
            })?;
            
        }
        Ok(module)
    }
}

/*
    Closure for one package (project), including metadata and symbol table for all dependencies.
*/
pub struct PackageClosure{
    me: Arc<PackageMetadata>,
    dependent_packages: HashMap<Arc<PackageMetadata>, PackageSymbolTable>,
    visible_packages: BTreeMap<String, Arc<PackageMetadata>>
}

impl PackageClosure{
    pub fn visible_dependency_packages(&self)->impl Iterator<Item = (&String, &Arc<PackageMetadata>, &PackageSymbolTable)>{
        self.visible_packages.iter().map(|(name, meta)| {
            let table = self.dependent_packages.get(meta).unwrap();
            (name, meta, table)
        })
    }
    pub fn me(&self)->&PackageSymbolTable{
        self.dependent_packages.get(&self.me).unwrap()
    }
    pub fn me_mut(&mut self)->&mut PackageSymbolTable{
        self.dependent_packages.get_mut(&self.me).unwrap()
    }
    pub fn new(me: PackageMetadata)->Self{
        let me = Arc::new(me);
        let table = PackageSymbolTable::new_from_metadata(me.clone());
        let mut all_packages_map = HashMap::new();
        all_packages_map.insert(me.clone(), table);
        let mut visible_packages_map = BTreeMap::new();
        visible_packages_map.insert(me.name.clone(), me.clone());
        //map.insert();
        PackageClosure{
            me, dependent_packages: all_packages_map, visible_packages: visible_packages_map
        }
    }
    pub fn add_dependency(&mut self, dep: PackageClosure){
        self.visible_packages.insert(dep.me.name.clone(), dep.me.clone());
        self.dependent_packages.extend(dep.dependent_packages.into_iter());
    }
}

#[cfg(test)]
mod tests{
    use super::*;

    #[test]
    fn test_moduletree(){
        let mut package = PackageSymbolTable::new("testPackage", u256::new(114514));
        let sym = package.root_module_mut().insert_symbol("foo", false).unwrap();
        println!("{}", sym.mangled_name());
        let module = package.root_module_mut().entry_module("foobar").unwrap();
        let sym2 = module.insert_symbol("bar", false).unwrap();
        println!("{}", sym2.mangled_name());
    }
}