// Hand-written parser.

mod expr;

use nom::{IResult, combinator::{verify, opt, map}, error::ErrorKind, multi::{separated_list1, separated_list0, many0}, sequence::{tuple}, branch::alt};

use super::{tokens::{TokenLoc, ReservedOp, Token, ReservedId}, ast::{LExpr, ExprNode, Expr, UnaryOp, Ident, Qualified, BinaryOp, CmpType, LAST, ASTNode, AST, LASTBlock, ASTBlock, VarDef, VarLexicalTy, VarLexicalTyType}, location::Span};

use ReservedOp::*;
use ReservedId::*;

use expr::parse_expr;
use std::ops::Fn;
#[derive(Debug)]
pub enum ParseError<'s, 'a>{
    // TODO: merge unexpected token for better error hint.
    UnexpectedToken(TokenLoc<'a>),
    UnexpectedEOF,
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

fn reserved_op(op_type: ReservedOp)->impl for <'s, 'a> Fn(TokenStream<'s, 'a>)->ParseResult<'s, 'a, TokenLoc<'a>>{
    move |s: TokenStream |{
        verify(next, |tok| {
            if let Token::ReservedOp(op) = tok.0{
                if op==op_type{
                    return true;
                }
            }
            return false;
        })(s)
    }
}
fn reserved_id(id_type: ReservedId)->impl for <'s, 'a> Fn(TokenStream<'s, 'a>)->ParseResult<'s, 'a, TokenLoc<'a>>{
    move |s: TokenStream |{
        verify(next, |tok| {
            if let Token::ReservedId(id) = tok.0{
                if id==id_type{
                    return true;
                }
            }
            return false;
        })(s)
    }
}

fn unexpected_token<'s, 'a, T>(tok: TokenLoc<'a>)->ParseResult<'s, 'a, T>{
    return Err(nom::Err::Error(ParseError::UnexpectedToken(tok)));
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


fn parse_statement<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    todo!();
}

fn parse_statement_block<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LASTBlock>{
    map(tuple((reserved_op(LBrace),
     many0(parse_statement),
     reserved_op(RBrace))),  |(a, b, c)|{
        ASTBlock(b, a.1.span_over(c.1))
     })(s)
}

fn parse_statement_if<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, kw) = reserved_id(If)(s)?;
    let (s, expr) = parse_expr(s)?;
    let (s, then_block) = parse_statement_block(s)?;
    let (s, kw_else) = opt(reserved_id(Else))(s)?;
    let (s, else_block, end_span) = if kw_else.is_some(){
        let (s, else_block) = parse_statement_block(s)?;
        let end_span = else_block.1;
        (s, Some(else_block), end_span)
    }else{
        (s, None, then_block.1)
    };
    let span = kw.1.span_over(end_span);
    Ok((s, ok_ast(ASTNode::If { condition: expr, then_block: then_block, else_block: else_block }, span)))
}

fn parse_statement_while<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, kw) = reserved_id(While)(s)?;
    let (s, expr) = parse_expr(s)?;
    let (s, block) = parse_statement_block(s)?;
    let span = kw.1.span_over(block.1);
    Ok((s, ok_ast(ASTNode::While { condition: expr, body: block }, span)))
}
fn parse_statement_for<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, kw) = reserved_id(For)(s)?;
    let (s, var) = parse_ident(s)?;
    let (s, kw2) = reserved_id(In)(s)?;
    let (s, exprRange) = parse_expr(s)?;
    let (s, block) = parse_statement_block(s)?;
    let span = kw.1.span_over(block.1);
    Ok((s, ok_ast(ASTNode::For { var: var, range: exprRange, body: block }, span)))
}

// we fuse the two together since they both start with parse_expr.
fn parse_statement_assign_and_expr<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, lhs) = parse_expr(s)?;
    let (s, tok) = alt((reserved_op(Semicolon), reserved_op(Assign)))(s)?;
    if let Token::ReservedOp(Assign) = tok.0{
        let (s, rhs) = parse_expr(s)?;
        let (s, tok2) = reserved_op(Semicolon)(s)?;
        let span = lhs.1.span_over(tok2.1);
        Ok((s, ok_ast(ASTNode::Assign { lhs, rhs }, span)))
    }else{
        let span = lhs.1.span_over(tok.1);
        Ok((s, ok_ast(ASTNode::Expr(lhs), span)))
    }
}

fn parse_statement_gatedef<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, tok_gatedef) = alt((
        reserved_id(Defgate),
        reserved_id(Gate)
    ))(s)?;
    let (s, name) = parse_ident(s)?;
    let (s, tok_assign) = reserved_op(ReservedOp::Assign)(s)?;
    let (s, mat_expr) = parse_expr(s)?;
    let (s, tok_semicolon) = reserved_op(Semicolon)(s)?;
    Ok((s, ok_ast(ASTNode::Gatedef { name, definition: mat_expr }, tok_gatedef.1.span_over(tok_semicolon.1))))    
}


/* 
fn parse_statement_unitary<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{

}*/

/* 
We provide two ways for defining variables.
The legacy way is C-styled definition, where types are placed before the variable name and array size after. Only singleton variable (w/o init value) and array definition are supported.

int a;
int b = 10;
qbit c;
int arr[20];

The new way is let-binding, with explicit type or inferred type. We consider it more flexible since it writes type signature as a whole.

let a : int;
let b : int = 10;
let b_infer = 10;
let c : qbit;
let arr1 : int[5] = [1,2,3,4,5];
let arr2 = [1,2,3,4,5];
let arr3 = [1,2,3,4,5];


*/
fn parse_cstyle_defvar_term<'s, 'a>(ty: VarLexicalTy<Span>, s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, (Ident<Span>, VarLexicalTy<Span>, Option<Expr<Span>>)>{
    let (s, ident) = parse_ident(s)?;
    let (after_lookahead, lookahead) = next(s)?;
    match lookahead.0{
        Token::ReservedOp(LSquare)=>{
            let (s, size) = tok_natural(after_lookahead)?;
            let (s, r_brace) = reserved_op(RSquare)(s)?;
            let span = ident.1.span_over(r_brace.1);
            Ok((s, (
                ident,
                VarLexicalTy(Box::new(VarLexicalTyType::Array(ty, size)), span),
                None
            )))
        }
        Token::ReservedOp(Assign)=>{
            let (s, init_val) = parse_expr(after_lookahead)?;
            let span = ident.1.span_over(init_val.1);
            Ok((s, (
                ident,
                ty,
                Some(init_val)
            )))
        }
        _=>{
            Ok((s, (ident, ty, None)))
        }
    }
}
fn parse_base_type<'s, 'a>(allow_qualified: bool, s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, VarLexicalTy<Span>>{
    let base_type = |tok: ReservedId, ty: VarLexicalTyType<Span>| {
        map(reserved_id(tok), move |s| VarLexicalTy(Box::new(ty.clone()), s.1))
    };
    let qualified_type = map(parse_qualified, |qident| {
        let span = qident.1;
        VarLexicalTy(Box::new(VarLexicalTyType::Named(qident)), span)
    });
    let mut builtin_types = alt(
        (
            base_type(Int, VarLexicalTyType::Int),
            base_type(Qbit, VarLexicalTyType::Qbit),
            base_type(Double,VarLexicalTyType::Double),
            base_type(Bool,VarLexicalTyType::Boolean)
        )
    );
    if allow_qualified{
        alt((builtin_types, qualified_type))(s)
    }else{
        builtin_types(s)
    }
}



fn parse_cstyle_defvar<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, base_type) = parse_base_type(false, s)?;
    let (s, vars) = separated_list1(reserved_op(Comma), map(|s| parse_cstyle_defvar_term(base_type.clone(), s), |(a, b, c)|{
        (VarDef{var: a, ty: Some(b)}, c)
    }))(s)?;
    let (s, tok) = reserved_op(Semicolon)(s)?;
    let span = base_type.1.span_over(tok.1);
    Ok((s, ok_ast(ASTNode::Defvar { definitions: vars }, span)))
}

fn parse_reference_header<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, TokenLoc<'a>>{
    reserved_op(BitAnd)(s)
    // TODO: parse lifetime annotation?
}

// &[T] or &[T;N] by lookahead
// Note that currently we only allow &[T] as a whole instead of introducing entire DST mechanism.
fn parse_slice_type_or_refarray<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, VarLexicalTy<Span>>{
    let (s, tok_ref) =  parse_reference_header(s)?;
    let (s, tok_lsquare) = reserved_op(LSquare)(s)?;
    let (s, ty) = parse_full_type(s)?;
    let (s, tok) = next(s)?;
    match tok.0{
        Token::ReservedOp(RSquare)=>{
            // &[T]
            Ok((s, VarLexicalTy(Box::new(VarLexicalTyType::Slice(ty)), tok_ref.1.span_over(tok_ref.1.span_over(tok.1)))))
        }
        Token::ReservedOp(Semicolon)=>{
            // &[T;N]
            let (s, size) = tok_natural(s)?;
            let (s, tok) = reserved_op(RSquare)(s)?;
            Ok((s, VarLexicalTy(Box::new(VarLexicalTyType::Ref(
                VarLexicalTy(Box::new(VarLexicalTyType::Array(ty, size)), tok_lsquare.1.span_over(tok.1))
            )), tok_ref.1.span_over(tok_ref.1.span_over(tok.1)))))
        }
        _ => return unexpected_token(tok)
    }
    
}
// &T
fn parse_reference_type<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, VarLexicalTy<Span>>{
    let (s, tok_ref) =  parse_reference_header(s)?;
    let (s, ty) = parse_full_type(s)?;
    let span_end = ty.1;
    Ok((s, VarLexicalTy(Box::new(VarLexicalTyType::Ref(
       ty
    )), tok_ref.1.span_over(tok_ref.1.span_over(span_end)))
    ))
}
// &&T. Backdoor for current terrible tokenizer.
fn parse_reference_type_doubleref<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, VarLexicalTy<Span>>{
    let (s, tok_doubleref) =  reserved_op(And)(s)?;
    let (s, ty) = parse_full_type(s)?;
    let span_end = ty.1;
    let outer_span = tok_doubleref.1.span_over(tok_doubleref.1.span_over(span_end));
    let inner_span = Span{
        byte_offset: outer_span.byte_offset+1,
        byte_len: outer_span.byte_len-1
    };
    let inner_ref = VarLexicalTy(Box::new(VarLexicalTyType::Ref(
        ty
     )), outer_span);
    let outer_ref = VarLexicalTy(Box::new(VarLexicalTyType::Ref(
        inner_ref
     )), inner_span);
    Ok((s, outer_ref))
}
// [T; N]
fn parse_array_type<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, VarLexicalTy<Span>>{
    let (s, tok_lsquare) = reserved_op(LSquare)(s)?;
    let (s, ty) = parse_full_type(s)?;
    let (s, semicolon) = reserved_op(Semicolon)(s)?;
    let (s, size) = tok_natural(s)?;
    let (s, tok_rsquare) = reserved_op(RSquare)(s)?;
    Ok((s, 
        VarLexicalTy(Box::new(VarLexicalTyType::Array(ty, size)), tok_lsquare.1.span_over(tok_rsquare.1))
    ))
}
fn parse_unit_type<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, VarLexicalTy<Span>>{
    let (s, tok_lparen) = reserved_op(LParen)(s)?;
    let (s, tok_rparen) = reserved_op(RParen)(s)?;
    Ok((s, 
        VarLexicalTy(Box::new(VarLexicalTyType::Unit), tok_lparen.1.span_over(tok_rparen.1))
    ))
}

fn parse_full_type<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, VarLexicalTy<Span>>{
    alt((
        parse_reference_type_doubleref,
        parse_slice_type_or_refarray,
        parse_reference_type,
        parse_array_type,
        |s| parse_base_type(true, s)
    ))(s)
}

fn parse_let_defvar_type<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, VarLexicalTy<Span>>{
    let (s, tok) = reserved_op(Colon)(s)?;
    parse_full_type(s)
}
fn parse_let_initval<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, Expr<Span>>{
    let (s, tok) = reserved_op(Assign)(s)?;
    parse_expr(s)
}

fn parse_let_defvar<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, tok) = reserved_id(Let)(s)?;
    let (s, ident) = parse_ident(s)?;
    let (s, type_annotation) = opt(parse_let_defvar_type)(s)?;
    let (s, initval) = opt(parse_let_initval)(s)?;
    let (s, tok_semicolon) = reserved_op(Semicolon)(s)?;
    Ok((s, ok_ast(ASTNode::Defvar { definitions: vec![
        (VarDef{var: ident, ty: type_annotation}, initval)
    ] }, tok.1.span_over(tok_semicolon.1))))
}


#[cfg(test)]
mod tests{
    use nom::combinator::all_consuming;

    use crate::lang::{ast::{LExpr, VarLexicalTy}, tokenizer::tokenizer, tokens::TokenLoc, location::Span};

    use super::{parse_expr, parse_full_type};

    fn tokenize<'a>(expr: &'a str)->Vec<TokenLoc<'a>>{
        tokenizer(expr).unwrap().1
    }
    fn test_parse_expr(expr: &str)->LExpr{
        let tokens = tokenize(expr);
        let s = all_consuming(parse_expr)(&tokens).unwrap().1;
        s
    }
    fn test_parse_type(expr: &str)->VarLexicalTy<Span>{
        let tokens = tokenize(expr);
        let s = all_consuming(parse_full_type)(&tokens).unwrap().1;
        s
    }

    #[test]
    fn test_expr(){
        println!("{:?}", test_parse_expr("1+2+3+2*4++1--2**2*(1+foo(bar, 12+34+arr[1](arr, arr)[baz]))"));
        println!("{:?}", test_parse_expr("(f(x(x)))(f(x(x)))"));
    }
    #[test]
    fn test_type(){
        println!("{:?}", test_parse_type("&&[&[qbit]; 10]"));
        println!("{:?}", test_parse_type("&&[&[int; 10]; 10]"));
    }
}

