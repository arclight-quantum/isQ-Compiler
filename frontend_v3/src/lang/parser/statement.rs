use super::*;
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


mod legacy{
    use super::super::*;
    fn parse_cstyle_defvar<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
        let (s, base_type) = parse_base_type(false, s)?;
        let (s, vars) = separated_list1(reserved_op(Comma), map(|s| parse_cstyle_defvar_term(base_type.clone(), s), |(a, b, c)|{
            (VarDef{var: a, ty: Some(b)}, c)
        }))(s)?;
        let (s, tok) = reserved_op(Semicolon)(s)?;
        let span = base_type.1.span_over(tok.1);
        Ok((s, ok_ast(ASTNode::Defvar { definitions: vars }, span)))
    }
    
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


}
use legacy::*;