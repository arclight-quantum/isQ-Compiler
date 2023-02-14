use super::*;


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

pub fn parse_full_type<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, VarLexicalTy<Span>>{
    alt((
        parse_reference_type_doubleref,
        parse_slice_type_or_refarray,
        parse_reference_type,
        parse_array_type,
        |s| parse_base_type(true, s)
    ))(s)
}

pub fn parse_base_type<'s, 'a>(allow_qualified: bool, s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, VarLexicalTy<Span>>{
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

#[cfg(test)]
mod tests{
    use nom::combinator::all_consuming;


    use crate::lang::{ast::VarLexicalTy, location::Span};

    use super::{super::tests::*, parse_full_type};
    fn test_parse_type(expr: &str)->VarLexicalTy<Span>{
        let tokens = tokenize(expr);
        let s = all_consuming(parse_full_type)(&tokens).unwrap().1;
        s
    }
    #[test]
    fn test_parse_type_examples(){
        println!("{:?}", test_parse_type("&&[&[qbit]; 10]"));
        println!("{:?}", test_parse_type("&&[&[int; 10]; 10]"));
    }
}