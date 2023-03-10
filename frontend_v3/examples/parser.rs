use std::io::Read;

use frontend_v3::lang::{tokenizer::{tokenizer, tokenizer_all}, parser::parse_program};
fn main()->std::io::Result<()>{
    let input = std::io::stdin();
    let mut lock = input.lock();
    let mut buf = String::new();
    lock.read_to_string(&mut buf).unwrap();
    println!("{:?}", buf);
    let tokens = tokenizer_all(&buf).unwrap();
    println!("{:?}", tokens);
    let ast = parse_program(&tokens.1).unwrap();
    println!("{:?}", ast);
    return Ok(());

}