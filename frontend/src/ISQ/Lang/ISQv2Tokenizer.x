{
{-# LANGUAGE FlexibleInstances #-}
module ISQ.Lang.ISQv2Tokenizer where
import Control.Exception (Exception)
import Data.List.Split
import Debug.Trace
import ISQ.Lang.Utils
}

%wrapper "monadUserState"

$digit = 0-9
$alpha = [a-zA-Z]

$idfirstchar  = [$alpha \_ \@]
$idrestchar   = [$alpha $digit \_]

@ident = $idfirstchar $idrestchar*
@qualified = (@ident \.)* @ident

@decimal = $digit+
@exponent = [eE] [\-\+] @decimal

@reservedid = 
	if|else|for|in|while|procedure|int|qbit|measure|print|defgate|pass|bp|return|package|import|
    ctrl|nctrl|inv|bool|true|false|let|const|unit|M|break|continue|double|as|extern|gate|deriving|oracle|to
@reservedop = "|0>"|"=="|"="|"+"|"-"|"*"|"/"|"<"|">"|"<="|">="|"!="|and|"&&"|or|"||"|not|"!"|"%"|"&"|"|"|"^"
              |">>"|"<<"|","|"("|")"|"{"|"}"|"["|"]"|"."|":"|";"|"->"|"**"
              |".."

tokens :-
    <0> $white+ {skip}
    <0, commentSC> "/*" {beginComment}
    <commentSC> "*/" {endComment}
    <commentSC> [.\n] {skip}
    <0> \" {beginString}
    <stringSC> \" {endString}
    <stringSC> . {appendString}
    <stringSC> \\[\nt\"] {escapeString}
    <0> "//".* {skip}
    <0> @reservedid {tokenReservedId}
    <0> @reservedop {tokenReservedOp}
    <0> @decimal { tokenNatural }
    <0> @decimal \. @decimal @exponent?
      | @decimal @exponent  { tokenFloat }
    <0> @decimal j
      | @decimal \. @decimal @exponent? j
      | @decimal @exponent j  { tokenImagPart }
    <0> @ident {tokenIdent}
    <0> @qualified {tokenQualified}
    <0> . {errc}

{
data Token ann = 
    TokenReservedId {annotationToken :: ann, tokenReservedIdV :: String}
  | TokenReservedOp {annotationToken :: ann, tokenReservedOpV :: String} | TokenNatural {annotationToken :: ann, tokenNaturalV :: Int}
  | TokenFloat {annotationToken :: ann, tokenFloatV :: Double}  | TokenImagPart {annotationToken :: ann, tokenImagPartV :: Double}  
  | TokenIdent {annotationToken :: ann, tokenIdentV :: String} 
  | TokenQualified {annotationToken :: ann, tokenIdentV :: String}
  | TokenEOF {annotationToken :: ann} 
  | TokenStringLit {annotationToken :: ann, tokenStringLitV :: String} deriving (Eq, Show)
data Span = Span {}

type ISQv2Token = Token Pos

instance Exception [ISQv2Token]

data AlexUserState = AlexUserState{
  stringBuffer :: String,
  stringPos :: Pos,
  commentNestLevel :: Int,
  posfilePos :: Pos,
  startRow :: Int
} deriving Show

alexInitUserState = AlexUserState "" undefined 0 (Pos 0 0 "") 0

get = Alex (\s->Right (s, alex_ust s))
put x = Alex (\s->Right (s{alex_ust = x}, ()))

instance Annotated Token where
  annotation = annotationToken

updateCommentStatus = do
  s<-get
  alexSetStartCode $ if (commentNestLevel s)==0 then 0 else commentSC

beginComment _ _ = do
  s<-get
  put s{commentNestLevel = (commentNestLevel s)+1}
  updateCommentStatus
  return Nothing
endComment _ _ = do
  s<-get
  put s{commentNestLevel = (commentNestLevel s)-1}
  updateCommentStatus
  return Nothing

beginString (pos, _, _, _) _ = do
  s<-get
  put s{stringBuffer = "", stringPos = position pos}
  alexSetStartCode stringSC
  return Nothing
appendString (_, _, _, c:_) _ = do
  s<-get
  put s {stringBuffer = c:(stringBuffer s)}
  return Nothing
escapeString (_, _, _, _:c:_) _ = do
  let unesc =
        case c of
          'n' -> '\n'
          't' -> '\t'
          '"' -> '"'
  s <- get
  put s{stringBuffer = unesc:(stringBuffer s)}
  return Nothing
endString _ _ = do
  s<-get
  let buf = stringBuffer s
  let Pos r c f = posfilePos s
  let Pos sr sc _ = stringPos s
  let start_row = startRow s
  put s{stringBuffer = ""}
  alexSetStartCode 0
  return $ Just $ TokenStringLit (Pos (sr-r-1+start_row) sc f) $ reverse buf
position (AlexPn _ line column) = Pos line column ""
tokenReservedId (pos, _, _, str) len = do
  s <- get
  let Pos r c f = posfilePos s
  let start_row = startRow s
  let Pos tr tc _ = position pos
  return $ Just $ TokenReservedId (Pos (tr-r-1+start_row) tc f) (take len str)
tokenReservedOp (pos, _, _, str) len = do
  s <- get
  let Pos r c f = posfilePos s
  let start_row = startRow s
  let Pos tr tc _ = position pos
  return $ Just $ TokenReservedOp (Pos (tr-r-1+start_row) tc f) (take len str)
tokenIdent (pos, _, _, str) len = do
  s <- get
  let Pos r c f = posfilePos s
  let start_row = startRow s
  let Pos tr tc _ = position pos
  return $ Just $ TokenIdent (Pos (tr-r-1+start_row) tc f) (take len str)
tokenQualified (pos, _, _, str) len = do
  s <- get
  let Pos r c f = posfilePos s
  let start_row = startRow s
  let Pos tr tc _ = position pos
  return $ Just $ TokenQualified (Pos (tr-r-1+start_row) tc f) (take len str)
tokenFloat (pos, _, _, str) len = do
  s <- get
  let Pos r c f = posfilePos s
  let start_row = startRow s
  let Pos tr tc _ = position pos
  return $ Just $ TokenFloat (Pos (tr-r-1+start_row) tc f) (read $ take len str)
tokenImagPart (pos, _, _, str) len = do
  s <- get
  let Pos r c f = posfilePos s
  let start_row = startRow s
  let Pos tr tc _ = position pos
  return $ Just $ TokenImagPart (Pos (tr-r-1+start_row) tc f) (read $ take (len-1) str)
tokenNatural (pos, _, _, str) len = do
  s <- get
  let Pos r c f = posfilePos s
  let start_row = startRow s
  let Pos tr tc _ = position pos
  return $ Just $ TokenNatural (Pos (tr-r-1+start_row) tc f) (read $ take len str)

alexEOF = return $ Just $ TokenEOF undefined

errc (pos, _, _, str) len = do
  s <- get
  let Pos r c f = posfilePos s
  let start_row = startRow s
  let Pos tr tc _ = position pos
  alexError $ "lex err: line " ++ (show (tr-r-1+start_row)) ++ ", col "++(show tc) ++ f

tokenize str = runAlex str $ do
  let step = do
        tok<-alexMonadScan
        case tok of
          (Just (TokenEOF _)) -> return []
          Nothing -> step
          (Just tok) -> do
              rest<-step
              return $ tok:rest
  step
  
}