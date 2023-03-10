// isQ tokenizer written in Nom.
use super::{tokens::*, location::Span};
use nom::{combinator::*, IResult, bytes::complete::{tag}, branch::alt, character::complete::{char, anychar, u64,  newline, multispace0, satisfy}, error::{ErrorKind, Error}, number::complete::double, sequence::pair, multi::{many_till, many0}, AsChar};
use std::cell::Cell;
use nom_locate::{LocatedSpan, position};

type NomSpan<'a> = LocatedSpan<&'a str>;
impl<'a> From<NomSpan<'a>> for Span{
    fn from(value: NomSpan<'a>)->Span{
        Span{
            byte_offset: value.location_offset(),
            byte_len: value.fragment().as_bytes().len(),
        }
    }
}

fn span_over<T: Into<Span> + Copy>(a: T, b: T)->Span{
    let sa = a.into();
    let sb = b.into();
    sa.span_over(sb)
}

// Comment line.

fn token_comment_line<'a>(s: NomSpan<'a>)->IResult<NomSpan<'a>, TokenLoc<'a>>{
    map(recognize(pair(
        tag("//"), many_till(anychar, newline)
    )), |v: NomSpan| {
        TokenLoc(Token::Comment, v.into())
    })(s)
}

// Comment block. Nested comment is supported.
fn token_comment_block<'a>(s: NomSpan<'a>)->IResult<NomSpan<'a>, TokenLoc<'a>>{
    fn block<'a>(s:NomSpan<'a>)->IResult<NomSpan<'a>, ()> {
        let (s, _start) = tag("/*")(s)?;
        let indent = Cell::new(1);
        let incr_indent = |s: NomSpan<'a>|{
            let (s, _) = tag("/*")(s)?;
            indent.set(indent.get()+1);
            Ok((s, ()))
        };
        let decr_indent = |s: NomSpan<'a>| {
            let (s, _) = tag("*/")(s)?;
            indent.set(indent.get()-1);
            Ok((s, ()))
        };
        let mut s = s;
        while indent.get() >0{
            let (s2, _) = alt((&incr_indent, &decr_indent, map(anychar, |_|{()})))(s)?;
            s = s2;
        }
        return Ok((s, ()));
    }
    map(recognize(block), |v: NomSpan| {
        TokenLoc(Token::Comment, v.into())
    })(s)
}

// Identifier.

fn ident_head_char(chr: char)->bool{
    chr.is_alpha() || chr=='_'
}
fn ident_tail_char(chr: char)->bool{
    chr.is_alphanum() || chr=='_'
}

fn token_identifier<'a>(s: NomSpan<'a>)->IResult<NomSpan<'a>, TokenLoc<'a>>{
    map(recognize(pair(
        satisfy(ident_head_char),
        many0(satisfy(ident_tail_char))
    )), |v: NomSpan|{
        TokenLoc(Token::Ident(v.fragment()), v.into())
    })(s)
}

/*
 Parse all numbers, including:
 - Decimal integer numbers.
 - Floating point numbers.
 - Imaginary integer and floating point numbers.
 */
fn token_number<'a>(s: NomSpan<'a>)->IResult<NomSpan<'a>, TokenLoc<'a>>{
    let input = s;
    let (s, start) = position(s)?;
    let (s, int_part) = opt(u64)(s)?;
    let (s, dot) = opt(char('.'))(s)?;
    if dot.is_none() && int_part.is_none(){
        return Err(nom::Err::Error(Error::new(input, ErrorKind::Digit)));
    }
    if dot.is_none(){
        // integer or integer imaginary
        let (s, imag) = opt(char('j'))(s)?;
        let (s, end) = position(s)?;
        let span = span_over(start, end);
        if imag.is_some(){
            return Ok((s, TokenLoc(Token::Imag(int_part.unwrap() as f64), span)));
        }else{
            return Ok((s, TokenLoc(Token::Natural(int_part.unwrap() as isize), span)));
        }
    }else{
        // floating point
        let (s, val) = double(input)?;
        let (s, imag) = opt(char('j'))(s)?;
        let (s, end) = position(s)?;
        let span = span_over(start, end);
        if imag.is_some(){
            return Ok((s, TokenLoc(Token::Imag(val), span)));
        }else{
            return Ok((s, TokenLoc(Token::Real(val), span)));
        }
    }
}


// Macros for creating reserved tokens.
macro_rules! reservedId {
    ($name: tt, $tag: expr, $ret: expr) => {
        fn $name<'a>(s: NomSpan<'a>)->IResult<NomSpan<'a>, TokenLoc<'a>>{
            let (s, tok) = tag($tag)(s)?;
            // the next character must not be part of ident.
            not(satisfy(ident_tail_char))(s)?;
            Ok((s, 
                TokenLoc(Token::ReservedId($ret), tok.into())
            ))
        }
    };
    ($name: tt, $tag: expr, $ret: expr, op) => {
        fn $name<'a>(s: NomSpan<'a>)->IResult<NomSpan<'a>, TokenLoc<'a>>{
            let (s, tok) = tag($tag)(s)?;
            not(satisfy(ident_tail_char))(s)?;
            Ok((s, 
                TokenLoc(Token::ReservedOp($ret), tok.into())
            ))
        }
    };
}

macro_rules! reservedOp {
    ($name: tt, $tag: expr, $ret: expr) => {
        fn $name<'a>(s: NomSpan<'a>)->IResult<NomSpan<'a>, TokenLoc<'a>>{
            let (s, tok) = tag($tag)(s)?;
            Ok((s, 
                TokenLoc(Token::ReservedOp($ret), tok.into())
            ))
        }
    };
}

// Reserved Tokens

reservedId!(token_if, "if", ReservedId::If);
reservedId!(token_else, "else", ReservedId::Else);
reservedId!(token_for, "for", ReservedId::For);
reservedId!(token_in, "in", ReservedId::In);
reservedId!(token_while, "while", ReservedId::While);
reservedId!(token_procedure, "procedure", ReservedId::Procedure);
reservedId!(token_fn, "fn", ReservedId::Fn);
reservedId!(token_int, "int", ReservedId::Int);
reservedId!(token_qbit, "qbit", ReservedId::Qbit);
reservedId!(token_measure, "measure", ReservedId::Measure);
reservedId!(token_print, "print", ReservedId::Print);
reservedId!(token_defgate, "defgate", ReservedId::Defgate);
reservedId!(token_pass, "pass", ReservedId::Pass);
reservedId!(token_return, "return", ReservedId::Return);
reservedId!(token_package, "package", ReservedId::Package);
reservedId!(token_import, "import", ReservedId::Import);
reservedId!(token_ctrl, "ctrl", ReservedId::Ctrl);
reservedId!(token_nctrl, "nctrl", ReservedId::NCtrl);
reservedId!(token_inv, "inv", ReservedId::Inv);
reservedId!(token_bool, "bool", ReservedId::Bool);
reservedId!(token_true, "true", ReservedId::True);
reservedId!(token_false, "false", ReservedId::False);
reservedId!(token_let, "let", ReservedId::Let);
reservedId!(token_const, "const", ReservedId::Const);
reservedId!(token_unit, "unit", ReservedId::Unit);
reservedId!(token_break, "break", ReservedId::Break);
reservedId!(token_continue, "continue", ReservedId::Continue);
reservedId!(token_double, "double", ReservedId::Double);
reservedId!(token_as, "as", ReservedId::As);
reservedId!(token_extern, "extern", ReservedId::Extern);
reservedId!(token_gate, "gate", ReservedId::Gate);
reservedId!(token_deriving, "deriving", ReservedId::Deriving);
reservedId!(token_oracle, "oracle", ReservedId::Oracle);
reservedId!(token_to, "to", ReservedId::To);
reservedId!(token_from, "from", ReservedId::From);
reservedId!(token_and_word, "and", ReservedOp::AndWord, op);
reservedId!(token_or_word, "or", ReservedOp::OrWord, op);
reservedId!(token_not_word, "not", ReservedOp::NotWord, op);

reservedOp!(token_ket0, "|0>", ReservedOp::Ket0);
reservedOp!(token_eq, "==", ReservedOp::Eq);
reservedOp!(token_assign, "=", ReservedOp::Assign);
reservedOp!(token_plus, "+", ReservedOp::Plus);
reservedOp!(token_minus, "-", ReservedOp::Minus);
reservedOp!(token_mult, "*", ReservedOp::Mult);
reservedOp!(token_div, "/", ReservedOp::Div);
reservedOp!(token_less, "<", ReservedOp::Less);
reservedOp!(token_greater, ">", ReservedOp::Greater);
reservedOp!(token_lesseq, "<=", ReservedOp::LessEq);
reservedOp!(token_greatereq, ">=", ReservedOp::GreaterEq);
reservedOp!(token_neq, "!=", ReservedOp::NEq);
reservedOp!(token_op_and, "&&", ReservedOp::And);
reservedOp!(token_op_or, "||", ReservedOp::Or);
reservedOp!(token_op_not, "!", ReservedOp::Not);
reservedOp!(token_mod, "%", ReservedOp::Mod);
reservedOp!(token_bitand, "&", ReservedOp::BitAnd);
reservedOp!(token_bitor, "|", ReservedOp::BitOr);
reservedOp!(token_bitxor, "^", ReservedOp::BitXor);
reservedOp!(token_rshift, ">>", ReservedOp::RShift);
reservedOp!(token_lshift, "<<", ReservedOp::LShift);
reservedOp!(token_comma, ",", ReservedOp::Comma);
reservedOp!(token_lbrace, "(", ReservedOp::LParen);
reservedOp!(token_rbrace, ")", ReservedOp::RParen);
reservedOp!(token_lbracket, "{", ReservedOp::LBrace);
reservedOp!(token_rbracket, "}", ReservedOp::RBrace);
reservedOp!(token_lsquare, "[", ReservedOp::LSquare);
reservedOp!(token_rsquare, "]", ReservedOp::RSquare);
reservedOp!(token_dot, ".", ReservedOp::Dot);
reservedOp!(token_colon, ":", ReservedOp::Colon);
reservedOp!(token_semicolon, ";", ReservedOp::Semicolon);
reservedOp!(token_arrow, "->", ReservedOp::Arrow);
reservedOp!(token_pow, "**", ReservedOp::Pow);
reservedOp!(token_range, "..", ReservedOp::Range);
reservedOp!(token_scope, "::", ReservedOp::Scope);
reservedOp!(token_quote, "\"", ReservedOp::Quote);


fn token_reserved<'a>(s: NomSpan<'a>)->IResult<NomSpan<'a>, TokenLoc<'a>>{
    alt((
        alt((
            token_if,
            token_else,
            token_for,
            token_int,
            token_in,
            token_while,
            token_procedure,
            token_fn,
            token_qbit,
            token_measure,
            token_print,
            token_defgate,
            token_pass,
            token_return,
            token_package,
            token_import,
            token_ctrl,
            token_nctrl,
            token_inv,
            token_bool,
        )), alt((
            token_true,
            token_false,
            token_let,
            token_const,
            token_unit,
            token_break,
            token_continue,
            token_double,
            token_as,
            token_extern,
            token_gate,
            token_deriving,
            token_oracle,
            token_to,
            token_from,
        )), alt((
            // Multi-character operators.
            token_ket0,
            token_eq,
            token_arrow,
            token_rshift,
            token_lshift,
            token_assign,
            token_pow,
            token_range,
            token_scope,
            token_lesseq,
            token_greatereq,
            token_op_and,
            token_op_or,
            token_op_not,
            token_and_word,
            token_or_word,
            token_not_word,
        )), alt((
            // Single-character operators.
            token_plus,
            token_minus,
            token_mult,
            token_div,
            token_less,
            token_greater,
            token_neq,
            token_mod,
            token_bitand,
            token_bitor,
            token_bitxor,
            token_comma,
        )), alt((
            token_lbrace,
            token_rbrace,
            token_lbracket,
            token_rbracket,
            token_lsquare,
            token_rsquare,
            token_dot,
            token_semicolon,
            token_colon,
            token_quote,
        ))
    ))(s)
}

fn token<'a>(s: NomSpan<'a>)->IResult<NomSpan<'a>, TokenLoc<'a>>{
    alt((
        token_comment_block,
        token_comment_line,
        token_number,
        token_reserved,
        token_identifier,

    ))(s)
}

fn tokenize_lexeme<'a>(s: NomSpan<'a>)->IResult<NomSpan<'a>, Vec<TokenLoc<'a>>>{
    all_consuming(map(pair(many0(map(pair(multispace0, token), |x| x.1)),
    multispace0), |x| x.0))(s)
}

pub fn tokenizer<'a>(s: &'a str)->IResult<NomSpan<'a>, Vec<TokenLoc<'a>>>{
    let s2 = NomSpan::new(s);
    tokenize_lexeme(s2).map(|(s, x)| (s, x.into_iter().filter(|y| if let Token::Comment = y.0 {false} else {true}).collect()))
}

pub fn tokenizer_all<'a>(s: &'a str)->IResult<NomSpan<'a>, Vec<TokenLoc<'a>>>{
    let s2 = NomSpan::new(s);
    all_consuming(tokenize_lexeme)(s2).map(|(s, x)| (s, x.into_iter().filter(|y| if let Token::Comment = y.0 {false} else {true}).collect()))
}


mod test{
    #[test]
    fn lexer_test_classvar1_normal(){
        let src = "
        int i1, i2;
        procedure main(){
            i1 = 3;
            i2 = 4.5+.16+.154e2j+i1;
            print i2;
        }
        ";
        println!("{:?}", super::tokenizer(src));
    }

}