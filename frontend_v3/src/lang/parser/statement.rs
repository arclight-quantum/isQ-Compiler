use crate::lang::ast::{GateModifier, GateModifierType, ImportEntry, DerivingClauseType, DerivingClause};

use super::*;
pub fn parse_toplevel_statement<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, Vec<LAST>>{
    let (s, package_decl) = opt(parse_statement_package)(s)?;
    let (s, top_stmts) = many0(alt((
        parse_statement_import,
        parse_statement_procedure,
        parse_statement_gatedef,
        parse_statement_let_defvar,
        // Legacy grammar
        parse_cstyle_defvar,
    )))(s)?;
    Ok((s, package_decl.into_iter().chain(top_stmts.into_iter()).collect()))
}
pub fn parse_statement<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    alt((
        parse_statement_blockstmt,
        parse_statement_if,
        parse_statement_for,
        parse_statement_while,
        parse_statement_assign_and_expr,
        parse_statement_let_defvar,
        parse_statement_unitary,
        parse_statement_gatedef,
        parse_statement_import,
        parse_statement_procedure,
        parse_statement_pass,
        parse_statement_return,
        parse_statement_continue,
        parse_statement_break,
        parse_statement_empty,
        // Legacy grammar
        parse_cstyle_defvar
    ))(s)
}


fn parse_statement_block<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LASTBlock>{
    map(tuple((reserved_op(LBrace),
     many0(parse_statement),
     reserved_op(RBrace))),  |(a, b, c)|{
        ASTBlock(b, a.1.span_over(c.1))
     })(s)
}

fn parse_statement_blockstmt<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, block) = parse_statement_block(s)?;
    let span = block.1;
    Ok((s, ok_ast(ASTNode::Block(block), span)))
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
    let (s, _kw2) = reserved_id(In)(s)?;
    let (s, expr_range) = parse_expr(s)?;
    let (s, block) = parse_statement_block(s)?;
    let span = kw.1.span_over(block.1);
    Ok((s, ok_ast(ASTNode::For { var: var, range: expr_range, body: block }, span)))
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
    let (s, _tok_assign) = reserved_op(ReservedOp::Assign)(s)?;
    let (s, mat_expr) = parse_expr(s)?;
    let (s, tok_semicolon) = reserved_op(Semicolon)(s)?;
    Ok((s, ok_ast(ASTNode::Gatedef { name, definition: mat_expr }, tok_gatedef.1.span_over(tok_semicolon.1))))    
}



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
    let (s, _tok) = reserved_op(Colon)(s)?;
    parse_full_type(s)
}
fn parse_let_initval<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, Expr<Span>>{
    let (s, _tok) = reserved_op(Assign)(s)?;
    parse_expr(s)
}

fn parse_statement_let_defvar<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, tok) = reserved_id(Let)(s)?;
    let (s, ident) = parse_ident(s)?;
    let (s, type_annotation) = opt(parse_let_defvar_type)(s)?;
    let (s, initval) = opt(parse_let_initval)(s)?;
    let (s, tok_semicolon) = reserved_op(Semicolon)(s)?;
    Ok((s, ok_ast(ASTNode::Defvar { definitions: vec![
        (VarDef{var: ident, ty: type_annotation}, initval)
    ] }, tok.1.span_over(tok_semicolon.1))))
}


fn parse_gate_decoration_inv<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, GateModifier<Span>>{
    let (s, tok) = reserved_id(Inv)(s)?;
    Ok((s, GateModifier(GateModifierType::Inv, tok.1)))
}
fn parse_gate_decoration_ctrl<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, GateModifier<Span>>{
    let (s, (tok, flag)) = alt((
        pair(reserved_id(Ctrl), success(true)),
        pair(reserved_id(NCtrl), success(false))
    ))(s)?;
    let (s, langle) = opt(reserved_op(Less))(s)?;
    if langle.is_some(){
        let (s, ctrl_size) = tok_natural(s)?;
        let (s, rangle) = reserved_op(Greater)(s)?;
        Ok((s, GateModifier(GateModifierType::Ctrl(flag, ctrl_size as isize), tok.1.span_over(rangle.1))))
    }else{
        Ok((s, GateModifier(GateModifierType::Ctrl(flag, 1), tok.1)))
    }

}

fn parse_statement_gate_decoration<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, Vec<GateModifier<Span>>>{
    many1(alt((parse_gate_decoration_ctrl, parse_gate_decoration_inv)))(s)
}

fn parse_statement_unitary<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, decor) = parse_statement_gate_decoration(s)?;
    let (s, call_expr) = parse_expr(s)?;
    let (s, tok_semicolon) = reserved_op(Semicolon)(s)?;
    let mut span = call_expr.1;
    if let Some(x) = decor.first(){
        span = x.1;
    }
    span = span.span_over(tok_semicolon.1);
    if let ExprNode::Call{..} = &*call_expr.0 {
        return Ok((s, ok_ast( ASTNode::Unitary { 
            modifiers: decor, 
            call: call_expr 
        }, span)));
    }else{
        return unitary_op_expect_call_expr(span);
    }
}
fn parse_statement_package<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, tok_package) = reserved_id(Package)(s)?;
    let (s, pkg_name) = parse_qualified(s)?;
    let (s, tok_semicolon) = reserved_op(Semicolon)(s)?;
    return Ok((s, ok_ast(ASTNode::Package(pkg_name), tok_package.1.span_over(tok_semicolon.1))));
}

fn parse_import_entry_wildcard<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, ImportEntry<Span>>{
    let (s, tok_import) = reserved_op(Mult)(s)?;
    Ok((s, ImportEntry::all(
        None, tok_import.1
    )))
}
fn parse_import_entry_qualified<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, ImportEntry<Span>>{
    let (s, qualified) = parse_qualified(s)?;
    let s_fail = s;
    let (s, tok1) = opt(next)(s)?;
    match tok1{
        Some(tok1@TokenLoc(Token::ReservedOp(Scope),_))=>{
            let (s2, lookahead) = next(s)?;
            match lookahead.0{
                Token::ReservedOp(Mult)=>{
                    let span = qualified.1.span_over(lookahead.1);
                    Ok((s2, ImportEntry::all(Some(qualified), span)))
                }
                Token::ReservedOp(LBrace)=>{
                    let (s, mut tree) = parse_import_entry_tree(s)?;
                    tree.1 = qualified.1.span_over(tree.1);
                    tree.set_tree_root(qualified);
                    
                    Ok((s, tree))
                }
                _ => return unexpected_token(tok1)
            }
        }
        Some(tok1@TokenLoc(Token::ReservedId(As),_))=>{
            let (s, name) = parse_ident(s)?;
            let span = qualified.1.span_over(name.1);
            Ok((s, ImportEntry::single(qualified, Some(name), span)))
        }
        _=>{
            let span = qualified.1;
            Ok((s_fail, ImportEntry::single(qualified, None, span)))
        }
    }
}
fn parse_import_entry_tree<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, ImportEntry<Span>>{
    let (s, tok_lbrace) = reserved_op(LBrace)(s)?;
    let (s, imports) = separated_list1(reserved_op(Comma), parse_import_entry)(s)?;
    let (s, tok_rbrace) = reserved_op(RBrace)(s)?;
    Ok((s,ImportEntry::tree(None, imports, tok_lbrace.1.span_over(tok_rbrace.1))))
}

fn parse_import_entry<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, ImportEntry<Span>>{
    alt((parse_import_entry_tree, parse_import_entry_qualified, parse_import_entry_wildcard))(s)
}

fn parse_statement_import<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, tok_import) = reserved_id(Import)(s)?;
    let (s, import_entry) = parse_import_entry(s)?;
    let (s, tok_semicolon) = reserved_op(Semicolon)(s)?;
    let span = tok_import.1.span_over(tok_semicolon.1);
    Ok((s, ok_ast(ASTNode::Import(import_entry), span)))
}

fn parse_procedure_deriving<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, Vec<DerivingClause<Span>>>{
    let (s, tok_derive) = reserved_id(Deriving)(s)?;
    fn parse_single_deriving<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, DerivingClause<Span>>{
        alt((
            map(reserved_id(Gate), |tok| DerivingClause(DerivingClauseType::Gate, tok.1)),
            map(reserved_id(Oracle), |tok| DerivingClause(DerivingClauseType::Oracle, tok.1)),
        ))(s)
    }
    alt((
        map(parse_single_deriving, |x| vec![x]),
        delimited(reserved_op(LParen), many1(parse_single_deriving), reserved_op(RParen))
    ))(s)
}

fn parse_procedure_return_type<'s, 'a>(unit_span: Span, s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, VarLexicalTy<Span>>{
    let (s, ret_type) = opt(preceded(reserved_op(Arrow), parse_full_type))(s)?;
    if let Some(ret_type) = ret_type{
        return Ok((s, ret_type));
    }else{
        return Ok((s, VarLexicalTy::unit(unit_span)))
    }
}

fn parse_procedure_argument<'s, 'a>( s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, VarDef<Span>>{
    let (s, ident) = parse_ident(s)?;
    let (s, tok_colon) = reserved_op(Colon)(s)?;
    let (s, ty) = parse_full_type(s)?;
    Ok((s, VarDef{var: ident, ty: Some(ty)}))
}


fn parse_statement_procedure<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, tok_procedure) = alt((reserved_id(Procedure), reserved_id(Fn)))(s)?;
    let (s, name) = parse_ident(s)?;
    let (s, tok_lparen) = reserved_op(LParen)(s)?;
    let (s, args) = separated_list0(reserved_op(Comma),  parse_procedure_argument)(s)?;
    let (s, tok_rparen) = reserved_op(RParen)(s)?;
    let (s, return_type) = parse_procedure_return_type(tok_rparen.1, s)?;
    let signature_span = tok_procedure.1.span_over(return_type.1);
    let (s, body) = parse_statement_block(s)?;
    let (s, deriving_clause) = opt(parse_procedure_deriving)(s)?;
    Ok((s, ok_ast(ASTNode::Procedure { name: name, args: vec![], body, deriving_clauses: deriving_clause.unwrap_or(vec![]) }, signature_span)))
}

fn parse_statement_return<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, tok_return) = reserved_id(Return)(s)?;
    let (s, expr) = opt(parse_expr)(s)?;
    let (s, tok_semicolon) = reserved_op(Semicolon)(s)?;
    let span = tok_return.1.span_over(tok_semicolon.1);
    Ok((s, ok_ast(ASTNode::Return(expr), span)))
}


fn parse_statement_pass<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, tok) = reserved_id(Pass)(s)?;
    let (s, tok_semicolon) = reserved_op(Semicolon)(s)?;
    let span = tok.1.span_over(tok_semicolon.1);
    Ok((s, ok_ast(ASTNode::Pass, span)))
}
fn parse_statement_continue<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, tok) = reserved_id(Continue)(s)?;
    let (s, tok_semicolon) = reserved_op(Semicolon)(s)?;
    let span = tok.1.span_over(tok_semicolon.1);
    Ok((s, ok_ast(ASTNode::Continue, span)))
}
fn parse_statement_break<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, tok) = reserved_id(Break)(s)?;
    let (s, tok_semicolon) = reserved_op(Semicolon)(s)?;
    let span = tok.1.span_over(tok_semicolon.1);
    Ok((s, ok_ast(ASTNode::Break, span)))
}

fn parse_statement_empty<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
    let (s, tok_semicolon) = reserved_op(Semicolon)(s)?;
    Ok((s, ok_ast(ASTNode::Empty, tok_semicolon.1)))
}



/**
 * Legacy C-styled grammars from early isQ version.
 * These grammars may be preserved for compatibility.
 * However, there is no need to extend these grammars with new features.
 */
mod legacy{
    use super::super::*;

    pub fn parse_cstyle_defvar<'s, 'a>(s: TokenStream<'s, 'a>)->ParseResult<'s, 'a, LAST>{
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
        let (after_lookahead, lookahead) = opt(next)(s)?;
        if let Some(lookahead) = lookahead{
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
        }else{
            Ok((after_lookahead, (ident, ty, None)))
        }
    }


}
use legacy::*;
use nom::sequence;

#[cfg(test)]
mod tests{
    use nom::combinator::all_consuming;

    use crate::lang::{ast::{ImportEntry}, parser::statement::parse_import_entry, location::Span};

    use super::{super::tests::*, parse_statement_procedure, parse_toplevel_statement};
    fn test_parse_import_entry(expr: &str)->ImportEntry<Span>{
        let tokens = tokenize(expr);
        let s = all_consuming(parse_import_entry)(&tokens).unwrap().1;
        s
    }
    #[test]
    fn test_parse_expr_entry_examples(){

        println!("{:?}", test_parse_import_entry("crate::lang::{ast::{ImportEntry}, parser::statement::parse_import_entry, location::Span}"));
        println!("{:?}", test_parse_import_entry("std"));
        println!("{:?}", test_parse_import_entry("*"));
        println!("{:?}", test_parse_import_entry("std::*"));
        println!("{:?}", test_parse_import_entry("std::{foo::{bar as baz}, *, baz as baz}"));
    }
    #[test]
    fn test_parse_procedure(){
        let parser = test_parser(parse_statement_procedure);
        println!("{:?}", parser("
        procedure first_sum(x: int)->int{
            let range = 1:x;
            let sum = 0;
            for i in range{
                sum = sum + i;
            }
            return sum;
        }
        "));
        println!("{:?}", parser("
        procedure fib(x: int)->int{
            if x<=2{
                return 1;
            }else{
                return fib(x-2) + fib(x-1);
            }
        }
        "));
    }
    #[test]
    fn test_parse_program(){
        let parser = test_parser(parse_toplevel_statement);
        println!("{:?}", parser("
        package std::qmpi;
        import std::prelude::*;
        fn create_bell_pair(q: &[qbit]){
            reset(q[0]); reset(q[1]);
            H(q[0]);
            ctrl X(q[0], q[1]);
        } deriving gate
        "));
    }

}
