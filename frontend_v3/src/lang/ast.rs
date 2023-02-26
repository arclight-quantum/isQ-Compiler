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

// Lexical expr with span info attached.
pub type LExpr = Expr<Span>;
pub type LAST = AST<Span, Span>;
pub type LASTBlock = ASTBlock<Span, Span>;