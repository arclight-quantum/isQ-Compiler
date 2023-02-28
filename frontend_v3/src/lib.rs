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
 * Identifier resolution pass.
 * Contains package resolution.
 */
pub mod resolve;
pub mod error;
#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        let result = 2 + 2;
        assert_eq!(result, 4);
    }
}
