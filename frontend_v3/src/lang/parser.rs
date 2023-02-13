// Hand-written parser.

/*
isQ operator precedence and associativity table:
Precedence Operator  Associativity
13         :         N/A
12         || or     Left-to-right
11         && and    Left-to-right
10         |         Left-to-right
9          ^         Left-to-right
8          &         Left-to-right
7          == !=     Left-to-right
6          < > >= <= N/A
5          >> <<     Left-to-right
4          + -       Left-to-right
3          * / %     Left-to-right
2          **        Right-to-left
1          + - ! not Right-to-left         
0          [] ()     Left-to-right
*/

trait Precedence{
    fn get_binaryop_type(self)->Option<BinaryOp>;
    fn is_right_to_left(self)->Option<bool>;
    fn get_precedence(self)->usize;
}
use ReservedOp::*;
impl Precedence for ReservedOp{
    fn is_right_to_left(self)->Option<bool> {
        if let BinaryOp::Cmp(_) = self.get_binaryop_type()?{
            return None;
        }
        // Only power operator is r2l.
        Some(self==Pow)
    }
    fn get_binaryop_type(self)->Option<BinaryOp> {
        let x = match self{
            Or=>BinaryOp::Or,
            And=>BinaryOp::And,
            BitOr=>BinaryOp::BitOr,
            BitXor=>BinaryOp::BitXor,
            BitAnd=>BinaryOp::BitAnd,
            Eq=>BinaryOp::Cmp(CmpType::EQ),
            NEq=>BinaryOp::Cmp(CmpType::NE),
            Greater=>BinaryOp::Cmp(CmpType::GT),
            GreaterEq=>BinaryOp::Cmp(CmpType::GE),
            Less=>BinaryOp::Cmp(CmpType::LT),
            LessEq=>BinaryOp::Cmp(CmpType::LE),
            LShift=>BinaryOp::Shl,
            RShift=>BinaryOp::Shr,
            Plus=>BinaryOp::Add,
            Minus=>BinaryOp::Sub,
            Mult=>BinaryOp::Mul,
            Div=>BinaryOp::Div,
            Mod=>BinaryOp::Mod,
            Pow=>BinaryOp::Pow,
            _=>{return None;}
        };
        return Some(x);
    }
    fn get_precedence(self)->usize{
        match self{
            Colon=>13,
            Or=>12,
            And=>11,
            BitOr=>10,
            BitXor=>9,
            BitAnd=>8,
            Eq=>7, NEq=>7,
            Greater=>6, Less=>6,
            GreaterEq=>6, LessEq=>6,
            RShift=>5, LShift=>5,
            Plus=>4, Minus=>4,
            Mult=>3, Div=>3, Mod=>3,
            Pow=>2,
            Not=>1,
            LBrace=>0,
            LBracket=>0,
            _=>14,
        }
    }
}
use nom::{IResult, combinator::{verify, opt, map}, error::ErrorKind, multi::{separated_list1, separated_list0}, sequence::{delimited, tuple}};

use super::{tokens::{TokenLoc, ReservedOp, Token, ReservedId}, ast::{LExpr, ExprNode, Expr, UnaryOp, Ident, Qualified, BinaryOp, CmpType}, location::Span};

#[derive(Debug)]
pub enum ParseError<'s, 'a>{
    UnexpectedToken(TokenLoc<'a>),
    UnexpectedEOF,
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



fn ok_expr<'s, 'a, E>(node: ExprNode<E>, pos: E)->Expr<E>{
    Expr(Box::new(node), pos)
}

fn parse_argument_list<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, (Vec<LExpr>, Span)>{
    map(tuple((reserved_op(LBrace), separated_list0(reserved_op(Comma), parse_expr), reserved_op(RBrace))), |tup|{
        (tup.1, tup.0.1.span_over(tup.2.1))
    })(s)
}
fn parse_array_subscript<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, (LExpr, Span)>{
    map(tuple((reserved_op(LSquare),  parse_expr, reserved_op(RSquare))), |tup|{
        (tup.1, tup.0.1.span_over(tup.2.1))
    })(s)
}



fn parse_atom<'s, 'a>(s0: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LExpr>{
    let (s, tok) = next(s0)?;
    match tok.0{
        Token::ReservedOp(LBrace) => {
            let (s, expr) = parse_expr(s)?;
            let (s, rparen) = reserved_op(RBrace)(s)?;
            return Ok((s, ok_expr(ExprNode::Brace(expr), tok.1.span_over(rparen.1))));
        }
        Token::Real(x)=>{
            return Ok((s, ok_expr(ExprNode::LitFloat(x), tok.1)));
        },
        Token::Imag(x)=>{
            return Ok((s, ok_expr(ExprNode::LitImag(x), tok.1)));
        }
        Token::Natural(x)=>{
            return Ok((s, ok_expr(ExprNode::LitInt(x), tok.1)));
        }
        Token::ReservedId(ReservedId::True)=>{
            return Ok((s, ok_expr(ExprNode::LitBool(true), tok.1)));
        }
        Token::ReservedId(ReservedId::False)=>{
            return Ok((s, ok_expr(ExprNode::LitBool(false), tok.1)));
        }
        

        Token::Ident(_)=>{
            // takes out a qualified ident
            let (s, ident) = parse_qualified(s0).unwrap();
            let span = ident.1;
            let ident = ok_expr(ExprNode::Qualified(ident), span);
            return Ok((s, ident));
        }
        _ => {
            return Err(nom::Err::Error(ParseError::UnexpectedToken(tok)));
        }
    }
}



fn parse_level_0<'s, 'a>(s0: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LExpr>{
    let (mut s, mut base) = parse_atom(s0)?;
    loop{
        // Check if it is a function call.
        let (s1, arglist) = opt(parse_argument_list)(s)?;
        if let Some((args, loc)) = arglist{
            s = s1;
            let span = base.1.span_over(loc);
            base = ok_expr(ExprNode::Call {
                callee: base,
                args: args
            }, span);
            continue;
        }
        // Check if it is a subscript.
        let (s1, subscript) = opt(parse_array_subscript)(s)?;
        if let Some((subscript, loc)) = subscript{
            s = s1;
            let span = base.1.span_over(loc);
            base = ok_expr(ExprNode::Subscript {
                base: base,
                offset: subscript
            }, span);
            continue;
        }
        break;
    }
    return Ok((s, base))
}

fn parse_level_1<'s, 'a>(s0: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LExpr>{
    let (s, tok) = next(s0)?;
    match tok.0{
        Token::ReservedOp(Plus)=>{
            let (s, sub) = parse_atom(s)?;
            let span = tok.1.span_over(sub.1);
            return Ok((s, ok_expr(ExprNode::Unary{
                op: UnaryOp::Pos,
                arg: sub
            }, span)));
        }
        Token::ReservedOp(Minus)=>{
            let (s, sub) = parse_atom(s)?;
            let span = tok.1.span_over(sub.1);
            return Ok((s, ok_expr(ExprNode::Unary{
                op: UnaryOp::Neg,
                arg: sub
            }, span)));
        }
        Token::ReservedOp(Not)=>{
            let (s, sub) = parse_atom(s)?;
            let span = tok.1.span_over(sub.1);
            return Ok((s, ok_expr(ExprNode::Unary{
                op: UnaryOp::Not,
                arg: sub
            }, span)));
        }
        Token::ReservedId(ReservedId::Not)=>{
            let (s, sub) = parse_atom(s)?;
            let span = tok.1.span_over(sub.1);
            return Ok((s, ok_expr(ExprNode::Unary{
                op: UnaryOp::Not,
                arg: sub
            }, span)));
        }
        _ => parse_level_0(s0)
    }
}

fn tok_binary_op<'s, 'a>(s0: TokenStream<'s, 'a>)->ParseResult<'s, 'a, (BinaryOp, Option<bool>, usize)>{
    let (s, tok) = next(s0)?;
    if let Token::ReservedOp(bop) = tok.0{
        if let Some(ty) = bop.get_binaryop_type(){
            return Ok((s, (ty, bop.is_right_to_left(), bop.get_precedence())))
        }
    }
    return unexpected_token(tok);
}


// for the rest levels, use precedence climbing.
fn parse_level_2_to_12<'s, 'a>(s0: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LExpr>{
    fn go<'s, 'a>(mut base: LExpr, max_precedence: usize, s0: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LExpr>{
        let (mut s, mut lookahead) = opt(tok_binary_op)(s0)?;
        while let Some((ty0, _r2l0, prec0)) = lookahead{
            if prec0>max_precedence {
                break;
            }
            let (s2, mut term) = parse_level_1(s)?;
            s = s2;
            // s2 is after-lookahead.
            let mut s2;
            (s2, lookahead) = opt(tok_binary_op)(s)?;
            while let Some(dprec) = {
                if let Some ((_ty, r2l, prec)) = lookahead{
                    if prec<prec0{
                        Some(1)
                    }else if prec == prec0 && r2l==Some(true){
                        Some(0)
                    }else{
                        None
                    }
                }else{
                    None
                }
            } {
                (s, term) = go(term, prec0 - dprec, s)?;
                (s2, lookahead) = opt(tok_binary_op)(s)?;
            }
            s = s2;
            let span = base.1.span_over(term.1);
            base = ok_expr(ExprNode::Binary { op: ty0, lhs: base, rhs: term }, span);
        }
        Ok((s, base))
    }
    let (s, term) = parse_level_1(s0)?;
    go(term, 12, s)
}

// Only for range operator
fn parse_level_13<'s, 'a>(s0: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LExpr>{
    let (s, t1) = opt(parse_level_2_to_12)(s0)?;
    let (s, c1) = opt(reserved_op(Colon))(s)?;
    if c1.is_none() && t1.is_some() {return Ok((s, t1.unwrap()));};
    if c1.is_none(){
        let (_, tok) = next(s0)?;
        return unexpected_token(tok);
    }
    let c1 = c1.unwrap();
    let (s, t2) = opt(parse_level_2_to_12)(s)?;
    let (s, c2) = reserved_op(Colon)(s)?;
    let (s, t3) = opt(parse_level_2_to_12)(s)?;
    let start = if let Some(x)=&t1 {x.1} else {c1.1};
    let end = if let Some(x) = &t3 {x.1} else {c2.1};
    Ok((s, ok_expr(ExprNode::Range { lo: t1, hi: t2, step: t3 }, start.span_over(end))))
}

fn parse_expr<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LExpr>{
    parse_level_13(s)
}





#[cfg(test)]
mod tests{
    use nom::combinator::all_consuming;

    use crate::lang::{ast::LExpr, tokenizer::tokenizer, tokens::TokenLoc};

    use super::parse_expr;

    fn tokenize<'a>(expr: &'a str)->Vec<TokenLoc<'a>>{
        tokenizer(expr).unwrap().1
    }
    fn test_parse_expr(expr: &str)->LExpr{
        let tokens = tokenize(expr);
        let s = all_consuming(parse_expr)(&tokens).unwrap().1;
        s
    }

    #[test]
    fn test_expr(){
        println!("{:?}", test_parse_expr("1+2+3+2*4++1--2**2*(1+foo(bar, 12+34+arr[1](arr, arr)[baz]))"));
        println!("{:?}", test_parse_expr("(f(x(x)))(f(x(x)))"));
    }

}

