// TODO: better error.

use crate::lang::location::Span;

pub enum ISQFrontendError{
    GeneralError(&'static str, Span),
    RedefinedSymbol(Span),
}

pub type Result<T> = core::result::Result<T, ISQFrontendError>;