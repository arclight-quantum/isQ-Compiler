//use std::marker::Destruct;

use super::location::Span;

#[derive(Copy, Clone, Debug, PartialOrd, Ord, PartialEq, Eq)]
pub enum CmpType{
    EQ,
    NE, GT, LE, GE, LT
}
#[derive(Copy, Clone, Debug, PartialOrd, Ord, PartialEq, Eq)]
pub enum BinaryOp{
    Add, Sub, Mul, Div, Mod, And, Or, BitAnd, BitOr, BitXor, Cmp(CmpType), Pow, Shl, Shr
}
#[derive(Copy, Clone, Debug, PartialOrd, Ord, PartialEq, Eq)]
pub enum UnaryOp{
    Neg, Pos, Not, Borrow, Deref
}

#[derive(Debug, Clone)]
pub struct Ident<T>(pub String, pub T);

#[derive(Debug, Clone)]
pub struct Qualified<T>(pub Vec<Ident<T>>, pub T);

#[derive(Debug, Clone)]
pub enum GateModifierType{
    Inv, Ctrl(bool, isize)
}

#[derive(Debug, Clone)]
pub struct GateModifier<T>(pub GateModifierType, pub T);

#[derive(Debug, Clone)]
pub enum ExprNode<E>{
    Ident(Ident<E>),
    Qualified(Qualified<E>),
    Binary{
        op: BinaryOp,
        lhs: Expr<E>,
        rhs: Expr<E>
    },
    Paren(Expr<E>),
    Unary{
        op: UnaryOp,
        arg: Expr<E>
    },
    Subscript{
        base: Expr<E>, offset: Expr<E>
    },
    Call{
        callee: Expr<E>,
        args: Vec<Expr<E>>
    },
    LitInt(isize),
    LitFloat(f64),
    LitImag(f64),
    LitBool(bool),
    Range{
        lo: Option<Expr<E>>,
        hi: Option<Expr<E>>,
        step: Option<Expr<E>>,
    },
    List(Vec<Expr<E>>),
    Unit
}

#[derive(Debug, Clone)]
pub struct Expr<E>(pub Box<ExprNode<E>>, pub E);


/** These are builtin data types supported by isQ. */
#[derive(Debug, Clone)]
pub enum VarLexicalTyType<T>{
    /** Default integer type.*/
    Int, 
    /** Reference-alike qubit type. */
    Qbit, 
    /** Default fp64 type. */
    Double, 
    /** Boolean type. */
    Boolean, 
    /** Reserved for qualified type. */
    Named(Qualified<T>), 
    /** Owned value reference that can automatically degenerate into value.  */
    Owned(VarLexicalTy<T>), 
    /** Reference type that does not own underlying value. */
    Ref(VarLexicalTy<T>), 
    /** A contiguous array of data with determined length. */
    Array(VarLexicalTy<T>, usize), 
    /** Slice type that does not own underlying array.
     * Contains length and underlying pointer information.
     */
    Slice(VarLexicalTy<T>), 
    /** Range (lo, hi) that can be used in loops as well as slicing contiguous array. */
    Range, 
    /** Range (lo, hi, step) that can be used in loops. */
    Range3 ,
    /** Unit type */
    Unit
}

#[derive(Debug, Clone)]
pub struct VarLexicalTy<T>(pub Box<VarLexicalTyType<T>>, pub T);

impl<T> VarLexicalTy<T>{
    pub fn unit(span: T)->Self{
        Self(Box::new(VarLexicalTyType::Unit), span)
    }
}

#[derive(Debug, Clone)]
pub struct VarDef<T>{
    pub var: Ident<T>,
    pub ty: Option<VarLexicalTy<T>>,
}

#[derive(Debug, Clone)]
pub enum DerivingClauseType{
    Gate, Oracle
}
#[derive(Debug, Clone)]
pub struct DerivingClause<T>(pub DerivingClauseType, pub T);

#[derive(Debug, Clone)]
pub struct ASTBlock<E, T>(pub Vec<AST<E, T>>, pub T);


#[derive(Debug, Clone)]
pub enum ImportEntryType<T>{
    Single(Qualified<T>, Option<Ident<T>>),
    Tree(Option<Qualified<T>>, Vec<ImportEntry<T>>),
    All(Option<Qualified<T>>)
}
#[derive(Debug, Clone)]
pub struct ImportEntry<T>(pub ImportEntryType<T>, pub T);
impl<T> ImportEntry<T>{
    pub fn single(q: Qualified<T>, alias: Option<Ident<T>>, annotation: T)->Self{
        Self(ImportEntryType::Single(q, alias), annotation)
    }
    pub fn tree(base: Option<Qualified<T>>, list: Vec<ImportEntry<T>>, annotation: T)->Self{
        Self(ImportEntryType::Tree(base, list), annotation)
    }
    pub fn all(name: Option<Qualified<T>>, annotation: T)->Self{
        Self(ImportEntryType::All(name), annotation)
    }
    pub fn set_tree_root(&mut self, base: Qualified<T>){
        if let ImportEntryType::Tree(a, _)= &mut self.0{
            *a = Some(base);
        }else{
            unreachable!();
        }
    }
}

#[derive(Debug, Clone)]
pub enum ASTNode<E, T>{
    Block(ASTBlock<E, T>),
    Expr(Expr<E>),
    If{
        condition: Expr<E>,
        then_block: ASTBlock<E, T>,
        else_block: Option<ASTBlock<E, T>>
    },
    For{
        var: Ident<T>,
        range: Expr<E>,
        body: ASTBlock<E, T>,
    },
    While{
        condition: Expr<E>,
        body: ASTBlock<E, T>,
    },
    Defvar{
        definitions: Vec<(VarDef<T>, Option<Expr<E>>)>
    },
    Assign{
        lhs: Expr<E>,
        rhs: Expr<E>,
    },
    Gatedef {
        name: Ident<T>,
        definition: Expr<E>
    },
    Unitary {
        modifiers: Vec<GateModifier<T>>,
        call: Expr<E>
    },
    Package(Qualified<T>),
    Import(ImportEntry<T>),
    Procedure{
        name: Ident<T>,
        args: Vec<VarDef<T>>,
        body: ASTBlock<E, T>,
        deriving_clauses: Vec<DerivingClause<T>>
    },
    Pass, Return(Option<Expr<E>>), Continue, Break, Empty
}

#[derive(Debug, Clone)]
pub struct AST<E, T>(pub Box<ASTNode<E, T>>, pub T);

impl<E, T> ASTBlock<E, T>{
    pub fn lift<E2, T2, F: Fn(E)->E2, G: Fn(T)->T2>(self, f: &F, g: &G)->ASTBlock<E2, T2>{
        ASTBlock(self.0.into_iter().map(|x| x.lift(f, g)).collect(), g(self.1) )       
    }
}

impl<E> Expr<E>{
    pub fn lift<E2, F: Fn(E)->E2>(self, f: &F)->Expr<E2>{
        Expr(Box::new(match *self.0{
            ExprNode::Ident(id) => ExprNode::Ident(id.lift(f)),
            ExprNode::Qualified(q) => ExprNode::Qualified(q.lift(f)),
            ExprNode::Binary { op, lhs, rhs } => ExprNode::Binary { op, lhs: lhs.lift(f), rhs: rhs.lift(f) },
            ExprNode::Paren(sub) => ExprNode::Paren(sub.lift(f)),
            ExprNode::Unary { op, arg } => ExprNode::Unary { op, arg: arg.lift(f) },
            ExprNode::Subscript { base, offset } => ExprNode::Subscript { base: base.lift(f), offset: offset.lift(f) },
            ExprNode::Call { callee, args } => ExprNode::Call { callee: callee.lift(f), args: args.into_iter().map(|x| x.lift(f)).collect()},
            ExprNode::LitInt(x) => ExprNode::LitInt(x),
            ExprNode::LitFloat(x) => ExprNode::LitFloat(x),
            ExprNode::LitImag(x) => ExprNode::LitImag(x),
            ExprNode::LitBool(x) => ExprNode::LitBool(x),
            ExprNode::Range { lo, hi, step } => ExprNode::Range { lo: lo.map(|x| x.lift(f)), hi: hi.map(|x| x.lift(f)), step: step.map(|x| x.lift(f)) },
            ExprNode::List(xs) => ExprNode::List(xs.into_iter().map(|x| x.lift(f)).collect()),
            ExprNode::Unit => ExprNode::Unit,
        }), f(self.1))    
    }
}

impl<T> Ident<T>{
    pub fn lift<U, F: Fn(T)->U>(self, f: &F)->Ident<U>{
        Ident(self.0, f(self.1))
    }
}

impl<T> GateModifier<T>{
    pub fn lift<U, F: Fn(T)->U>(self, f: &F)->GateModifier<U>{
        GateModifier(self.0, f(self.1))
    }
}
impl<T> DerivingClause<T>{
    pub fn lift<U, F: Fn(T)->U>(self, f: &F)->DerivingClause<U>{
        DerivingClause(self.0, f(self.1))
    }
}
impl<T> Qualified<T>{
    pub fn lift<U, F: Fn(T)->U>(self, f: &F)->Qualified<U>{
        Qualified(self.0.into_iter().map(|x| x.lift(f)).collect(), f(self.1))
    }
}


impl<T> VarDef<T>{
    pub fn lift<U, F: Fn(T)->U>(self, f: &F)->VarDef<U>{
        VarDef{var: self.var.lift(f), ty: self.ty.map(|ty| {
            ty.lift(f)
        })}
    }
}


impl<T> VarLexicalTy<T>{
    pub fn lift<U, F: Fn(T)->U>(self, f: &F)->VarLexicalTy<U>{
        use VarLexicalTyType::*;
        VarLexicalTy(Box::new(match *self.0 {
            VarLexicalTyType::Int => Int,
            VarLexicalTyType::Qbit => Qbit,
            VarLexicalTyType::Double => Double,
            VarLexicalTyType::Boolean => Boolean,
            VarLexicalTyType::Named(x) => Named(x.lift(f)),
            VarLexicalTyType::Owned(x) => Owned(x.lift(f)),
            VarLexicalTyType::Ref(x) => Ref(x.lift(f)),
            VarLexicalTyType::Array(x, y) => Array(x.lift(f), y),
            VarLexicalTyType::Slice(x) => Slice(x.lift(f)),
            VarLexicalTyType::Range => Range,
            VarLexicalTyType::Range3 => Range3,
            VarLexicalTyType::Unit => Unit,
        }), f(self.1))
    }
}

impl<T> ImportEntry<T>{
    pub fn lift<U, F: Fn(T)->U>(self, f: &F)->ImportEntry<U>{
        ImportEntry(match self.0{
            ImportEntryType::Single(q, i) => ImportEntryType::Single(q.lift(f), i.map(|x| x.lift(f))),
            ImportEntryType::Tree(q, xs) => ImportEntryType::Tree(q.map(|x| x.lift(f)), xs.into_iter().map(|x| x.lift(f)).collect()),
            ImportEntryType::All(q) => ImportEntryType::All(q.map(|x| x.lift(f))),
        }, f(self.1))
    }
}

impl<E, T> AST<E, T>{
    pub fn lift<E2, T2, F: Fn(E)->E2, G: Fn(T)->T2>(self, f: &F, g: &G)->AST<E2, T2>{
        AST(Box::new(match *self.0{
            ASTNode::Block(block) => ASTNode::Block(block.lift(f, g)),
            ASTNode::Expr(expr) => ASTNode::Expr(expr.lift(f)),
            ASTNode::If { condition, then_block, else_block } => ASTNode::If { condition: condition.lift(f), then_block: then_block.lift(f, g), else_block: else_block.map(|b| b.lift(f, g))},
            ASTNode::For { var, range, body } => ASTNode::For { var: var.lift(g), range: range.lift(f), body: body.lift(f, g) },
            ASTNode::While { condition, body } => ASTNode::While{condition: condition.lift(f), body: body.lift(f, g)},
            ASTNode::Defvar { definitions } => ASTNode::Defvar { definitions: definitions.into_iter().map(|(def, default)| {
                (def.lift(g), default.map(|y| y.lift(f))) 
            }).collect() },
            ASTNode::Assign { lhs, rhs } => ASTNode::Assign { lhs: lhs.lift(f), rhs: rhs.lift(f) },
            ASTNode::Gatedef { name, definition } => ASTNode::Gatedef { name: name.lift(g), definition: definition.lift(f) },
            ASTNode::Unitary { modifiers, call } => ASTNode::Unitary { modifiers: modifiers.into_iter().map(|x| x.lift(g)).collect(), call: call.lift(f) },
            ASTNode::Package(pkg) => ASTNode::Package(pkg.lift(g)),
            ASTNode::Import(imp) => ASTNode::Import(imp.lift(g)),
            ASTNode::Procedure { name, args, body, deriving_clauses } => ASTNode::Procedure { name: name.lift(g), args: args.into_iter().map(|x| x.lift(g)).collect(), body: body.lift(f, g), deriving_clauses: deriving_clauses.into_iter().map(|x| x.lift(g)).collect() },
            ASTNode::Pass => ASTNode::Pass,
            ASTNode::Return(x) => ASTNode::Return(x.map(|y| y.lift(f))),
            ASTNode::Continue => ASTNode::Continue,
            ASTNode::Break => ASTNode::Break,
            ASTNode::Empty => ASTNode::Empty,
        }), g(self.1))
    }
}

// Lexical expr with span info attached.
pub type LExpr = Expr<Span>;
pub type LAST = AST<Span, Span>;
pub type LASTBlock = ASTBlock<Span, Span>;