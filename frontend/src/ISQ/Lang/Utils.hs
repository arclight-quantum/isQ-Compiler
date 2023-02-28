module ISQ.Lang.Utils where

import Control.Exception (Exception)
data Pos = Pos {line :: Int, column :: Int} deriving (Show, Eq)
data Span = Span {spanBegin :: Pos, spanEnd :: Pos}

spanOver a b = Span (spanBegin a) (spanEnd b)


spanToken pos a = Span pos (Pos (line pos) (column pos + a))
class Annotated x where
  annotation :: x ann->ann
instance Exception [Token Pos]