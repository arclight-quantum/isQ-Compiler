// Hand-written parser.

mod expr;
mod types;
mod statement;

use nom::{IResult, combinator::*, error::ErrorKind, multi::*, sequence::*, branch::*};

use self::types::{parse_base_type, parse_full_type};

use super::{tokens::{TokenLoc, ReservedOp, Token, ReservedId}, ast::{LExpr, ExprNode, Expr, UnaryOp, Ident, Qualified, BinaryOp, CmpType, LAST, ASTNode, AST, LASTBlock, ASTBlock, VarDef, VarLexicalTy, VarLexicalTyType}, location::Span};



use ReservedOp::*;
use ReservedId::*;

use expr::parse_expr;
use std::ops::Fn;
#[derive(Debug, Copy, Clone)]
pub enum ParseError<'s, 'a>{
    // TODO: merge unexpected token for better error hint.
    UnexpectedToken(TokenLoc<'a>),
    UnexpectedEOF,
    UnitaryOpExpectCallExpr(Span),
    // TODO: map the error to the errors above.
    NomError(nom::error::ErrorKind, TokenStream<'s, 'a>)
}

impl<'s, 'a> nom::error::ParseError<TokenStream<'s, 'a>> for ParseError<'s, 'a>{
    fn from_error_kind(input: TokenStream<'s, 'a>, kind: ErrorKind) -> Self{
        ParseError::NomError(kind, input)
    }
    fn append(input: TokenStream<'s, 'a>, kind: ErrorKind, mut other: Self) -> Self{
        other
    }
    fn or(self, other: Self)->Self{
        match (self, other){
            (ParseError::UnexpectedToken(a), ParseError::UnexpectedToken(b))=>{
                if a.1.byte_offset<b.1.byte_offset{
                    other
                }else{
                    self
                }
            }
            _=>{
                other
            }
        }
    }
}

type TokenStream<'s, 'a> = &'s [TokenLoc<'a>];
type ParseResult<'s, 'a, T> = IResult<TokenStream<'s, 'a>, T, ParseError<'s, 'a>>;

fn next<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, TokenLoc<'a>>{
    if s.len()==0{
        return Err(nom::Err::Error(ParseError::UnexpectedEOF));
    }else{
        return Ok((&s[1..], s[0]))
    }
}

/*
TODO: currently we handle tokens where one is prefix of the other at parser level.
(e.g. hard-encode possibly merged tokens by hand.
```
let foo: lifted<i32>=lift(0); // This requires us to parse the token `>=`!
```
)
We need a better approach to deal with situations like this. For example, making the token sequence `>=' match both `>` `=` and `>=`.

*/
fn reserved_op(op_type: ReservedOp)->impl for <'s, 'a> Fn(TokenStream<'s, 'a>)->ParseResult<'s, 'a, TokenLoc<'a>>{
    move |s: TokenStream |{
        let (s, tok) = next(s)?;
        if let Token::ReservedOp(op) = tok.0{
            if op==op_type{
                return Ok((s, tok));
            }
        }
        return unexpected_token(tok);
    }
}
fn reserved_id(id_type: ReservedId)->impl for <'s, 'a> Fn(TokenStream<'s, 'a>)->ParseResult<'s, 'a, TokenLoc<'a>>{
    move |s: TokenStream |{
        let (s, tok) = next(s)?;
        if let Token::ReservedId(id) = tok.0{
            if id==id_type{
                return Ok((s, tok));
            }
        }
        return unexpected_token(tok);
    }
}

fn unexpected_token<'s, 'a, T>(tok: TokenLoc<'a>)->ParseResult<'s, 'a, T>{
    return Err(nom::Err::Error(ParseError::UnexpectedToken(tok)));
}

fn unitary_op_expect_call_expr<'s, 'a, T>(span: Span)->ParseResult<'s, 'a, T>{
    return Err(nom::Err::Error(ParseError::UnitaryOpExpectCallExpr(span)));
}

fn parse_ident<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, Ident<Span>>{
    let (s, tok) = next(s)?;
    if let Token::Ident(id) = tok.0{
        return Ok((s, Ident(id.to_owned(), tok.1)));
    }
    return unexpected_token(tok);
}

fn parse_qualified<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, Qualified<Span>>{
    let (s, parts) = separated_list1(reserved_op(ReservedOp::Scope), parse_ident)(s)?;
    let span = parts.first().unwrap().1.span_over(parts.last().unwrap().1);
    
    return Ok((s, Qualified(parts, span)));
}



fn ok_ast<'s, 'a, E, T>(node: ASTNode<E, T>, pos: T)->AST<E, T>{
    AST(Box::new(node), pos)
}

fn tok_natural<'s, 'a>(s0: TokenStream<'s, 'a>)->ParseResult<'s, 'a, usize>{
    let (s, tok) = next(s0)?;
    if let Token::Natural(x) = tok.0{
        return Ok((s, x as usize));
    }
    return unexpected_token(tok);
}

fn tok_eof<'s, 'a>(s0: TokenStream<'s, 'a>)->ParseResult<'s, 'a, ()>{
    let (s, tok) = next(s0)?;
    if let Token::EOF = tok.0{
        return Ok((s, ()));
    }
    return unexpected_token(tok);
}
pub use statement::parse_toplevel_statement;

pub fn parse_program<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, Vec<LAST>>{
    all_consuming(parse_toplevel_statement)(s)
}

#[cfg(test)]
mod tests{
    use nom::combinator::all_consuming;

    use crate::lang::{tokens::TokenLoc, tokenizer::tokenizer};

    use super::{TokenStream, ParseResult};
    pub fn tokenize<'a>(expr: &'a str)->Vec<TokenLoc<'a>>{
        tokenizer(expr).unwrap().1
    }
    pub fn test_parser<T>(parser: for<'s, 'a> fn(TokenStream<'s, 'a>)->ParseResult<'s, 'a, T>)->impl for<'r> Fn(&'r str)->T{
        move |st| {
            let mut p = parser;
            let tokens = tokenize(st);
            let ret = all_consuming(&mut p)(&tokens).unwrap().1;
            ret
        }
        
    }
}

