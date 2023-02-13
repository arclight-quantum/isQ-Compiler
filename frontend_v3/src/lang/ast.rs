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
    Neg, Pos, Not
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
pub struct GateModifier<T>(GateModifierType, pub T);

#[derive(Debug, Clone)]
pub enum ExprNode<E>{
    Ident(Ident<E>),
    Qualified(Qualified<E>),
    Binary{
        op: BinaryOp,
        lhs: Expr<E>,
        rhs: Expr<E>
    },
    Brace(Expr<E>),
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
}

#[derive(Debug, Clone)]
pub struct Expr<E>(pub Box<ExprNode<E>>, pub E);



#[derive(Debug, Clone)]
pub struct VarDef<T>{
    pub var: Ident<T>,
    pub ty: Qualified<T>,
}

#[derive(Debug, Clone)]
pub enum DerivingClauseType{
    Gate
}
#[derive(Debug, Clone)]
pub struct DerivingClause<T>(DerivingClauseType, pub T);



#[derive(Debug, Clone)]
pub enum ASTNode<E, T>{
    Block(AST<E, T>),
    Expr(Expr<E>),
    If{
        condition: Expr<E>,
        then_block: AST<E, T>,
        else_block: AST<E, T>
    },
    For{
        var: Ident<T>,
        range: Expr<E>,
        body: Vec<AST<E, T>>,
    },
    While{
        cond: Expr<E>,
        body: Vec<AST<E, T>>,
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
        modifiers: GateModifier<T>,
        call: Expr<E>
    },
    Package(Qualified<T>),
    Import(Qualified<T>),
    Procedure{
        name: Ident<T>,
        args: Vec<VarDef<T>>,
        deriving_clauses: Vec<DerivingClause<T>>
    },
    Pass, Return , Continue, Break
}

#[derive(Debug, Clone)]
pub struct AST<E, T>(pub Box<ASTNode<E, T>>, pub T);

// Lexical expr with span info attached.
pub type LExpr = Expr<Span>;
pub type LAST = AST<Span, Span>;