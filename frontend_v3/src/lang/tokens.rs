use super::location::Span;

// Define all tokens for isQ language.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum ReservedId {
    If,
    Else,
    For,
    In,
    While,
    Procedure,
    Fn,
    Int,
    Qbit,
    Measure,
    Print,
    Defgate,
    Pass,
    Return,
    Package,
    Import,
    Ctrl,
    NCtrl,
    Inv,
    Bool,
    True,
    False,
    Let,
    Const,
    Unit,
    Break,
    Continue,
    Double,
    As,
    Extern,
    Gate,
    Deriving,
    Oracle,
    To,
    From
}
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum ReservedOp {
    Ket0,
    Eq,
    Assign,
    Plus,
    Minus,
    Mult,
    Div,
    Less,
    Greater,
    LessEq,
    GreaterEq,
    NEq,
    And,
    Or,
    Not,
    Mod,
    BitAnd,
    BitOr,
    BitXor,
    RShift,
    LShift,
    Comma,
    LBrace,
    RBrace,
    LParen,
    RParen,
    LSquare,
    RSquare,
    Dot,
    Colon,
    Semicolon,
    Arrow,
    Pow,
    Range,
    Scope,
    Quote,
    AndWord,
    OrWord,
    NotWord,
}

#[derive(Debug, Clone, Copy)]
pub enum Token<'a> {
    ReservedId(ReservedId),
    ReservedOp(ReservedOp),
    Natural(isize),
    Real(f64),
    Imag(f64),
    StringLit(&'a str),
    Comment,
    Ident(&'a str),
    EOF,
}

#[derive(Debug, Clone, Copy)]
pub struct TokenLoc<'a>(pub Token<'a>, pub Span);