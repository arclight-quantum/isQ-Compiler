#[macro_use]
extern crate nom;
#[macro_use]
extern crate nom_locate;

/**
 * Language definition, tokenizer and parser.
 */
pub mod lang;
/**
 * MLIR AST generation.
 */
pub mod mlir;
/**
 * Package resolution and and mangling.
 */
pub mod package;
#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        let result = 2 + 2;
        assert_eq!(result, 4);
    }
}
