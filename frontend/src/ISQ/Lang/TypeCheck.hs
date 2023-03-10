module ISQ.Lang.TypeCheck where
import ISQ.Lang.ISQv2Grammar
import ISQ.Lang.ISQv2Tokenizer
import qualified Data.MultiMap as MultiMap
import Data.List.Extra (firstJust)
import Control.Monad (join)
import Control.Monad.Except
import Control.Monad.State.Lazy
import Debug.Trace

type EType = Type ()
type TCAST = AST TypeCheckData

data TypeCheckData = TypeCheckData{
    sourcePos :: Pos,
    termType :: EType,
    termId :: Int
} deriving (Eq, Show)

type TypeCheckInfo = TypeCheckData

data Symbol = SymVar String | SymTempVar Int | SymTempArg Int deriving (Show, Eq, Ord)

getSymbolName :: Symbol -> String
getSymbolName sym = case sym of {SymVar str -> str; _ -> ""}

data TypeCheckError =
      RedefinedSymbol { pos :: Pos, symbolName :: Symbol, firstDefinedAt :: Pos}
    | UndefinedSymbol { pos :: Pos, symbolName :: Symbol}
    | AmbiguousSymbol { pos :: Pos, symbolName :: Symbol, firstDefinedAt :: Pos, secondDefinedAt :: Pos}
    | TypeMismatch { pos :: Pos, expectedType :: [MatchRule], actualType :: Type ()}
    | ViolateNonCloningTheorem { pos :: Pos }
    | GateNameError { pos :: Pos }
    | ArgNumberMismatch { pos :: Pos, expectedArgs :: Int, actualArgs :: Int }
    | BadProcedureArgType { pos :: Pos, arg :: (Type (), Ident)}
    | BadProcedureReturnType { pos :: Pos, ret :: (Type (), Ident)}
    | ICETypeCheckError
    | MainUndefined
    | BadMainSignature { actualMainSignature :: Type () }
    deriving (Eq, Show)
type SymbolTableLayer = MultiMap.MultiMap Symbol DefinedSymbol
type SymbolTable = [SymbolTableLayer]

querySymbol :: Symbol -> SymbolTable -> [DefinedSymbol]
querySymbol sym [] = []
querySymbol sym (x:xs) = do
    let res = MultiMap.lookup sym x
    case res of
        [] -> querySymbol sym xs
        lis -> lis

insertSymbol :: Symbol->DefinedSymbol->SymbolTable->Either TypeCheckError SymbolTable
insertSymbol sym ast [] = insertSymbol sym ast [MultiMap.empty]
insertSymbol sym ast (x:xs) = case MultiMap.lookup sym x of
    [] -> Right $ MultiMap.insert sym ast x : xs
    (y:ys) -> Left $ RedefinedSymbol (definedPos ast) sym (definedPos y)


data TypeCheckEnv = TypeCheckEnv {
    symbolTable :: SymbolTable,
    ssaAllocator :: Int,
    mainDefined :: Bool
}

type TypeCheck = ExceptT TypeCheckError (State TypeCheckEnv)

data DefinedSymbol = DefinedSymbol{
    definedPos :: Pos,
    definedType :: EType,
    definedSSA :: Int,
    isGlobal :: Bool,
    qualifiedName :: String
} deriving (Show)

addSym :: Symbol->DefinedSymbol->TypeCheck ()
addSym k v = do
    symtable<-gets symbolTable
    new_table <-liftEither $ insertSymbol k v symtable
    modify' (\x->x{symbolTable=new_table})

getSym :: Pos->Symbol->TypeCheck DefinedSymbol
getSym pos k = do
    symtable<-gets symbolTable
    case querySymbol k symtable of
        [] -> throwError $ UndefinedSymbol pos k
        [x] -> return x
        (x:y:rest) -> throwError $ AmbiguousSymbol pos k (definedPos x) (definedPos y)

defineSym :: Symbol->Pos->EType->TypeCheck Int
defineSym a b c= do
    ssa<-nextId
    addSym a (DefinedSymbol b c ssa False "")
    return ssa

defineGlobalSym :: String -> String -> Pos -> EType -> TypeCheck Int
defineGlobalSym prefix name b c= do
    ssa<-nextId
    when (name == "main" && c /= Type () FuncTy [Type () Unit []] && c /= Type () FuncTy [Type () Unit [], Type () UnknownArray [intType ()], Type () UnknownArray [doubleType ()]]) $ do
        throwError $ BadMainSignature c
    when (name == "main") $ do
        modify' (\x->x{mainDefined = True})
    let qualifiedName = prefix ++ name
    addSym (SymVar name) (DefinedSymbol b c ssa True qualifiedName)
    addSym (SymVar qualifiedName) (DefinedSymbol b c ssa True qualifiedName)
    return ssa

scope :: TypeCheck ()
scope = modify (\x->x{symbolTable = MultiMap.empty:symbolTable x})

unscope :: TypeCheck SymbolTableLayer
unscope = do {x<-gets (head.symbolTable); modify' (\x->x{symbolTable = tail $ symbolTable x}); return x}

astType :: (Annotated p)=>p TypeCheckData->EType
astType = termType . annotation
nextId :: TypeCheck Int
nextId = do
    id<-gets ssaAllocator
    modify' (\x->x{ssaAllocator = id+1})
    return id

type TCExpr = Expr TypeCheckData

data MatchRule = Exact EType | AnyUnknownList | AnyKnownList Int | AnyList | AnyFunc | AnyGate | AnyRef deriving (Show, Eq)

checkRule :: MatchRule->EType->Bool
checkRule (Exact x) y = x==y
checkRule AnyUnknownList (Type () UnknownArray [_]) = True
checkRule (AnyKnownList x) (Type () (FixedArray y) [_]) = x==y
checkRule AnyList (Type () UnknownArray [_]) = True
checkRule AnyList (Type () (FixedArray y) [_]) = True
checkRule AnyFunc (Type () FuncTy _) = True
checkRule AnyGate (Type () (Gate _) _) = True
checkRule AnyRef (Type () Ref [_]) = True
checkRule _ _ = False


-- try to match two types, using auto dereference and int-to-bool implicit conversion.
matchType' :: [MatchRule]->TCExpr->TypeCheck (Maybe TCExpr)
matchType' wanted e = do
    let current_type = astType e
    let pos = sourcePos $ annotation e
    if any (`checkRule` current_type) wanted then return $ Just e
    else
        case current_type of
            -- Auto dereference rule
            Type () Ref [x] -> do
                id<-nextId
                matchType' wanted (EDeref (TypeCheckData pos x id) e)
            -- Bool-to-int implicit cast
            Type () Bool [] -> if Exact (Type () Int []) `notElem` wanted then return Nothing else do
                id<-nextId
                matchType' wanted (EImplicitCast (TypeCheckData pos (Type () Int [] ) id) e)
            -- int-to-double implicit cast
            Type () Int [] -> if Exact (Type () Double []) `notElem` wanted then return Nothing else do
                id<-nextId
                matchType' wanted (EImplicitCast (TypeCheckData pos (Type () Double [] ) id) e)
            -- float-to-complex implicit cast
            Type () Double [] -> if Exact (Type () Complex []) `notElem` wanted then return Nothing else do
                id<-nextId
                matchType' wanted (EImplicitCast (TypeCheckData pos (Type () Complex [] ) id) e)
            -- Auto list erasure
            Type () (FixedArray x) [y] -> do
                id<-nextId
                matchType' wanted (EEraselist (TypeCheckData pos (Type () UnknownArray [y]) id) e)
            _ -> return Nothing
matchType :: [MatchRule]->TCExpr->TypeCheck TCExpr
matchType wanted e = do
    new_e<-matchType' wanted e
    case new_e of
        Just x->return x
        Nothing -> throwError $ TypeMismatch (sourcePos $ annotation e) wanted (astType e)

exactBinaryCheck :: (Expr Pos->TypeCheck (Expr TypeCheckData)) -> EType -> Pos -> BinaryOperator -> LExpr -> LExpr -> TypeCheck (Expr TypeCheckData)
exactBinaryCheck f etype pos op lhs rhs = do
    ref_lhs <- f lhs
    ref_rhs <- f rhs
    lhs' <- matchType (map Exact [etype]) ref_lhs
    rhs' <- matchType (map Exact [etype]) ref_rhs
    ssa <- nextId
    return $ EBinary (TypeCheckData pos etype ssa) op lhs' rhs'

-- By now we only support bottom-up type checking.
-- All leaf nodes have their own type, and intermediate types are calculated.
typeCheckExpr' :: (Expr Pos->TypeCheck (Expr TypeCheckData))->Expr Pos->TypeCheck (Expr TypeCheckData)
typeCheckExpr' f (EIdent pos ident) = do
    sym<-getSym pos (SymVar ident)
    ssa<-nextId
    case isGlobal sym of
        True ->return $ EGlobalName (TypeCheckData pos (definedType sym) ssa) (qualifiedName sym)
        False -> return $ EResolvedIdent (TypeCheckData pos (definedType sym) ssa) (definedSSA sym)

typeCheckExpr' f (EBinary pos And lhs rhs) = exactBinaryCheck f (boolType ()) pos And lhs rhs
typeCheckExpr' f (EBinary pos Or lhs rhs) = exactBinaryCheck f (boolType ()) pos Or lhs rhs
typeCheckExpr' f (EBinary pos Shl lhs rhs) = exactBinaryCheck f (intType ()) pos Shl lhs rhs
typeCheckExpr' f (EBinary pos Shr lhs rhs) = exactBinaryCheck f (intType ()) pos Shr lhs rhs
typeCheckExpr' f (EBinary pos Pow lhs rhs) = do
    ref_lhs<-f lhs
    ref_rhs<-f rhs
    lhs' <- matchType (map Exact [doubleType ()]) ref_lhs
    rhs' <- matchType (map Exact [doubleType ()]) ref_rhs
    ssa<-nextId
    let lty = astType lhs'
    let rty = astType rhs'
    let return_type = lty
    return $ EBinary (TypeCheckData pos return_type ssa) Pow lhs' rhs'
typeCheckExpr' f (EBinary pos op lhs rhs) = do
    ref_lhs<-f lhs
    ref_rhs<-f rhs
    lhs' <- matchType (map Exact [intType (), doubleType (), complexType ()]) ref_lhs
    rhs' <- matchType (map Exact [intType (), doubleType (), complexType ()]) ref_rhs
    ssa<-nextId
    let lty = astType lhs'
    let rty = astType rhs'
    --traceM $ show rty
    matched_lhs <- case ty rty of
            Double -> matchType [Exact (doubleType ())] lhs'
            _ -> matchType (map Exact [intType (), doubleType (), complexType ()]) lhs'
    matched_rhs <- case ty lty of
            Double -> matchType [Exact (doubleType ())] rhs'
            _ -> matchType (map Exact [intType (), doubleType (), complexType ()]) rhs'
    --traceM $ show matched_lhs
    let return_type = case op of
            Cmp _ -> boolType ()
            _ -> astType matched_lhs
    case op of
        Mod -> if (return_type /= intType ()) then throwError $ TypeMismatch pos [Exact (intType ())] return_type else return $ EBinary (TypeCheckData pos return_type ssa) op matched_lhs matched_rhs
        _ -> return $ EBinary (TypeCheckData pos return_type ssa) op matched_lhs matched_rhs

typeCheckExpr' f (EUnary pos Not lhs) = do
    lhs'<-f lhs
    matched_lhs <- matchType (map Exact [boolType ()]) lhs'
    ssa<-nextId
    let return_type = boolType ()
    return $ EUnary (TypeCheckData pos return_type ssa) Not matched_lhs
typeCheckExpr' f (EUnary pos op lhs) = do
    lhs'<-f lhs
    matched_lhs <- matchType (map Exact [intType (), doubleType (), complexType ()]) lhs'
    ssa<-nextId
    let return_type = astType matched_lhs
    return $ EUnary (TypeCheckData pos return_type ssa) op matched_lhs
typeCheckExpr' f (ESubscript pos base offset) = do
    base'<-f base
    offset'<-f offset
    base''<-matchType [AnyList] base'
    offset''<-matchType [Exact (intType ())] offset'
    ssa<-nextId
    let a = case astType base'' of
            Type () UnknownArray [ax] -> ax
            Type () (FixedArray _) [ax] -> ax
            _ -> undefined
    return $ ESubscript (TypeCheckData pos (refType () a) ssa) base'' offset''
typeCheckExpr' f (ECall pos callee callArgs) = do
    callee'<-f callee
    callee''<-matchType [AnyFunc] callee'
    let callee_ty = astType callee''
    let (ret:args) = subTypes callee_ty
    callArgs'<-mapM f callArgs
    when (length args /= length callArgs') $ throwError $ ArgNumberMismatch pos (length args) (length callArgs')
    callArgs''<-zipWithM (\a->matchType [Exact a]) args callArgs'
    ssa<-nextId
    return $ ECall (TypeCheckData pos ret ssa) callee'' callArgs''
typeCheckExpr' f (EIntLit pos x) = do
    ssa<-nextId
    return $ EIntLit (TypeCheckData pos (intType ()) ssa) x
typeCheckExpr' f (EFloatingLit pos x) = do
    ssa<-nextId
    return $ EFloatingLit (TypeCheckData pos (doubleType ()) ssa) x
typeCheckExpr' f (EImagLit pos x) = do
    ssa<-nextId
    return $ EImagLit (TypeCheckData pos (complexType ()) ssa) x
typeCheckExpr' f (EBoolLit pos x) = do
    ssa<-nextId
    return $ EBoolLit (TypeCheckData pos (boolType ()) ssa) x
typeCheckExpr' f (ERange pos lo hi Nothing) = do
    let step = Just ( EIntLit pos 1)
    f (ERange pos lo hi step)
typeCheckExpr' f (ERange pos lo hi step) = do
    let resolve (Just x) = do {x'<-f x; x''<-matchType [Exact (Type () Int [])] x'; return $ Just x''}
        resolve Nothing = return Nothing
    lo'<-resolve lo
    hi'<-resolve hi
    step'<-resolve step
    ssa<-nextId
    return $ ERange (TypeCheckData pos (Type () IntRange []) ssa) lo' hi' step'
typeCheckExpr' f (ECoreMeasure pos qubit) = do
    qubit'<-f qubit
    qubit''<-matchType [Exact (refType () (qbitType ()))] qubit'
    ssa<-nextId
    return $ ECoreMeasure (TypeCheckData pos (boolType ()) ssa) qubit''
typeCheckExpr' f x@EList{} = error "Not implemented yet."
typeCheckExpr' f x@EDeref{} = error "Unreachable."
typeCheckExpr' f x@EImplicitCast{} = error "Unreachable."
typeCheckExpr' f (ETempVar pos ident) = do
    sym<-getSym pos (SymTempVar ident)
    ssa<-nextId
    return $ EResolvedIdent (TypeCheckData pos (definedType sym) ssa) (definedSSA sym)
typeCheckExpr' f (ETempArg pos ident) = do
    sym<-getSym pos (SymTempArg ident)
    ssa<-nextId
    return $ EResolvedIdent (TypeCheckData pos (definedType sym) ssa) (definedSSA sym)
typeCheckExpr' f (EUnitLit pos) = EUnitLit . TypeCheckData pos (unitType ()) <$> nextId
typeCheckExpr' f x@EResolvedIdent{} = error "Unreachable."
typeCheckExpr' f x@EGlobalName{} = error "Unreachable."
typeCheckExpr' f x@EEraselist{} = error "Unreachable."
typeCheckExpr :: Expr Pos -> TypeCheck (Expr TypeCheckData)
typeCheckExpr = fix typeCheckExpr'

okStmt :: Pos->TypeCheckData
okStmt pos = (TypeCheckData pos (unitType ()) (-1))


-- Transforms a defvar-type into a ref type.
-- Lists are passed by value and thus are right-values.
definedRefType :: EType->EType
definedRefType x@(Type () (FixedArray _) _) = x
definedRefType x@(Type () UnknownArray _) = x
definedRefType x = Type () Ref [x]


typeCheckAST' :: (AST Pos->TypeCheck (AST TypeCheckData))->AST Pos->TypeCheck (AST TypeCheckData)
typeCheckAST' f (NBlock pos lis) = do
    scope
    lis' <- mapM f lis
    unscope
    return $ NBlock (okStmt pos) lis'
typeCheckAST' f (NIf pos cond bthen belse) = do
    cond'<-typeCheckExpr cond
    cond''<-matchType [Exact (boolType ())] cond'
    scope
    bthen'<-mapM f bthen
    unscope
    scope
    belse'<-mapM f belse
    unscope
    return $ NIf (okStmt pos) cond'' bthen' belse'
typeCheckAST' f (NFor pos v r b) = do
    scope
    r'<-typeCheckExpr r
    v'<-defineSym (SymVar v) pos (intType ())
    r''<-matchType [Exact (Type () IntRange [])] r'
    b'<-mapM f b
    unscope
    return $  NResolvedFor (okStmt pos) v' r'' b'
typeCheckAST' f (NEmpty pos) = return $ NEmpty (okStmt pos)
typeCheckAST' f (NPass pos) = return $ NPass (okStmt pos)
typeCheckAST' f (NBp pos) = do
    temp_ssa<-nextId
    let annotation = TypeCheckData pos (unitType ()) temp_ssa
    return $ NBp annotation
typeCheckAST' f (NWhile pos cond body) = do
    cond'<-typeCheckExpr cond
    cond''<-matchType [Exact (boolType ())] cond'
    scope
    body'<-mapM f body
    unscope
    return $ NWhile (okStmt pos) cond'' body'
typeCheckAST' f (NCall pos c@(ECall _ callee args)) = do
    callee'<-typeCheckExpr callee
    callee''<-matchType [AnyFunc, AnyGate] callee'
    let callee_ty = astType callee''
    case ty $ callee_ty of
        FuncTy -> do
            c'<-typeCheckExpr c
            return $ NCall (okStmt pos) c'
        Gate _ -> f (NCoreUnitary pos callee args [])
        _ -> undefined
typeCheckAST' f (NCall pos c) = error "unreachable"
typeCheckAST' f (NDefvar pos defs) = do
    let def_one (ty, name, initializer) = do
            i'<-case initializer of
                Just r->do
                        r'<-typeCheckExpr r
                        r''<-matchType [Exact (void ty)] r'
                        return $ Just r''
                Nothing -> return Nothing
            s<-defineSym (SymVar name) pos $ definedRefType $ void ty
            return (definedRefType $ void ty, s, i')
    defs'<-mapM def_one defs
    return $ NResolvedDefvar (okStmt pos) defs'
typeCheckAST' f (NAssign pos lhs rhs) = do
    lhs'<-typeCheckExpr lhs
    lhs''<-matchType [AnyRef] lhs'
    rhs'<-typeCheckExpr rhs
    let Type () Ref [lhs_ty] = astType lhs''
    when (ty lhs_ty==Qbit) $ throwError $ ViolateNonCloningTheorem pos
    rhs''<-matchType [Exact lhs_ty] rhs'
    return $ NAssign (okStmt pos) lhs'' rhs''
typeCheckAST' f (NGatedef pos lhs rhs _) = error "unreachable"
typeCheckAST' f (NReturn _ _) = error "unreachable"
typeCheckAST' f (NCoreUnitary pos gate operands modifiers) = do
    gate'<-typeCheckExpr gate
    gate''<-matchType [AnyGate] gate'
    let Type _ (Gate x) extra = astType gate''
    let total_qubits = sum (map addedQubits modifiers) + x
    let total_operands = length extra + total_qubits
    when (total_operands /= length operands) $ throwError $ ArgNumberMismatch pos total_qubits (length operands)
    operands'<-mapM typeCheckExpr operands
    let (op_extra, op_qubits) = splitAt (total_operands - total_qubits) operands'
    op_extra'<-zipWithM (\x y->matchType [Exact x] y) extra op_extra
    op_qubits'<-mapM (matchType [Exact (refType () $ qbitType ())]) op_qubits
    let operands'' = op_extra' ++ op_qubits'
    return $ NCoreUnitary (okStmt pos) gate'' operands'' modifiers
typeCheckAST' f (NCoreReset pos qubit) = do
    qubit'<-typeCheckExpr qubit
    qubit''<-matchType [Exact (refType () $ qbitType ())] qubit'
    temp_ssa<-nextId
    let annotation = TypeCheckData pos (unitType ()) temp_ssa
    return $ NCoreReset annotation qubit''
typeCheckAST' f (NCorePrint pos val) = do
    val'<-typeCheckExpr val
    val''<-matchType [
        Exact (intType ()),
        Exact (doubleType ()),
        Exact (complexType ())
        ] val'
    return $ NCorePrint (okStmt pos) val''
typeCheckAST' f (NCoreMeasure pos qubit) = do
    qubit'<-typeCheckExpr qubit
    qubit''<-matchType [Exact (boolType ())] qubit'
    return $ NCoreMeasure (okStmt pos) qubit''
typeCheckAST' f (NProcedure _ _ _ _ _) = error "unreachable"
typeCheckAST' f (NContinue _) = error "unreachable"
typeCheckAST' f (NBreak _) = error "unreachable"
typeCheckAST' f (NResolvedFor _ _ _ _) = error "unreachable"
typeCheckAST' f (NResolvedGatedef pos name matrix size _) = error "unreachable"
typeCheckAST' f (NWhileWithGuard pos cond body break) = do
    cond'<-typeCheckExpr cond
    cond''<-matchType [Exact (boolType ())] cond'
    break'<-typeCheckExpr break
    break''<-matchType [Exact (boolType ())] break'
    body'<-mapM f body
    return $ NWhileWithGuard (okStmt pos) cond'' body' break''
typeCheckAST' f (NProcedureWithRet _ _ _ _ _ _) = error "not top"
typeCheckAST' f (NResolvedProcedureWithRet _ _ _ _ _ _ _) = error "unreachable"
typeCheckAST' f (NJumpToEndOnFlag pos flag) = do
    flag'<-typeCheckExpr flag
    flag''<-matchType [Exact (boolType ())] flag'
    return $ NJumpToEndOnFlag (okStmt pos) flag''
typeCheckAST' f (NJumpToEnd pos) = return $ NJumpToEnd (okStmt pos)
typeCheckAST' f (NTempvar pos def) = do
    let def_one (ty, id, initializer) = do
            i'<-case initializer of
                Just r->do
                        r'<-typeCheckExpr r
                        r''<-matchType [Exact (void ty)] r'
                        return $ Just r''
                Nothing -> return Nothing
            s<-defineSym (SymTempVar id) pos $ definedRefType $ void ty
            return (definedRefType $ void ty, s, i')
    def'<-def_one def
    return $ NResolvedDefvar (okStmt pos) [def']
typeCheckAST' f x@NResolvedExternGate{} = return $ fmap okStmt x
typeCheckAST' f x@NDerivedGatedef{} = return $ fmap okStmt x
typeCheckAST' f x@NDerivedOracle{} = return $ fmap okStmt x
typeCheckAST' f NExternGate{} = error "unreachable"
typeCheckAST' f NProcedureWithDerive{} = error "unreachable"
typeCheckAST' f NResolvedDefvar{} = error "unreachable"
typeCheckAST' f NGlobalDefvar {} = error "unreachable"
typeCheckAST' f NOracle{} = error "unreachable"
typeCheckAST' f NOracleTable{} = error "unreachable"
typeCheckAST :: AST Pos -> TypeCheck (AST TypeCheckData)
typeCheckAST = fix typeCheckAST'


argType :: Type Pos->Ident->TypeCheck EType
argType ty = argType' (annotation ty) ty 
argType' :: Pos->Type ann->Ident->TypeCheck EType
argType' pos ty i = case ty of
    Type _ Int [] -> return $ void ty
    Type _ Double [] -> return $ void ty
    Type _ Bool [] -> return $ void ty
    Type _ Qbit [] -> return $ Type () Ref [void ty]
    Type _ UnknownArray [a] -> return $ void ty
    Type _ (FixedArray _) [a] -> return $ void ty
    _ -> throwError $ BadProcedureArgType pos (void ty, i)

typeCheckToplevel :: Bool -> String -> [AST Pos]->TypeCheck ([TCAST], SymbolTableLayer, Int)
typeCheckToplevel isMain prefix ast = do
    
    (resolved_defvar, varlist)<-flip runStateT [] $ do
        mapM (\node->case node of
                NDefvar pos def -> do
                    -- Create separate scope to prevent cross-reference.
                    lift scope
                    p<-lift $ typeCheckAST node
                    let (NResolvedDefvar a defs') = p
                    s<-lift unscope
                    modify' (MultiMap.map (\x->x{isGlobal=True}) s:)
                    let node' = NGlobalDefvar a (zipWith (\(a1, a2, a3) (_, a4, _) ->(a1, a2, prefix ++ a4, a3)) defs' def)
                    return $ Right node'
                x -> return $ Left x
            ) ast
    -- Add all vars into table.
    let vars=concatMap MultiMap.toList varlist
    let qualifiedVars = concat $ map (\tup -> do
            let sym = fst tup
            let symName = getSymbolName sym
            let qualified = prefix ++ symName
            let qualifiedData = (snd tup){qualifiedName = qualified}
            [(sym, qualifiedData), (SymVar qualified, qualifiedData)]) vars
    mapM_ (uncurry addSym) $ reverse qualifiedVars
    
    -- Resolve all gates and procedures.
    resolved_headers<-mapM (\node->case node of
            Right x->return (Right x)
            Left (NResolvedGatedef pos name matrix size qir) -> do
                defineGlobalSym prefix name pos (Type () (Gate size) [])
                return $ Right (NResolvedGatedef (okStmt pos) (prefix ++ name) matrix size qir)
            Left (NExternGate pos name extra size qirname) -> do
                extra'<-mapM (\x->argType' pos x "<anonymous>") extra
                defineGlobalSym prefix name pos (Type () (Gate size) extra')
                return $ Right $ NResolvedExternGate (okStmt pos) (prefix ++ name) (fmap void extra) size qirname
            Left (NOracleTable pos name source value size) -> do
                defineGlobalSym prefix name pos (Type () (Gate size) [])
                return $ Right (NOracleTable (okStmt pos) (prefix ++ name) (prefix ++ source) value size)
            Left x@(NDerivedGatedef pos name source extra size) -> do
                extra'<-mapM (\x->argType' pos x "<anonymous>") extra
                defineGlobalSym prefix name pos (Type () (Gate size) extra')
                return $ Right (NDerivedGatedef (okStmt pos) (prefix ++ name) (prefix ++ source) extra' size)
            Left x@(NDerivedOracle pos name source extra size)->do
                extra'<-mapM (\x->argType' pos x "<anonymous>") extra
                defineGlobalSym prefix name pos (Type () (Gate size) extra')
                return $ Right (NDerivedOracle (okStmt pos) (prefix ++ name) (prefix ++ source) extra size)
            Left (NProcedureWithRet pos ty name args body ret) -> do
                -- check arg types and return types
                ty'<-case ty of
                    Type _ Int [] -> return $ void ty
                    Type _ Unit [] -> return $ void ty
                    Type _ Double [] -> return $ void ty
                    Type _ Bool [] -> return $ void ty
                    _ -> throwError $ BadProcedureReturnType pos (void ty, name)
                let new_args = if name == "main" && (length args) == 0 then [(Type pos UnknownArray [intType pos], "main$par1"), (Type pos UnknownArray [doubleType pos], "main$par2")] else args
                args'<-mapM (uncurry argType) new_args
                defineGlobalSym prefix name (annotation ty) (Type () FuncTy (ty':args'))
                -- NTempvar a (void b, procRet, Nothing)
                let procName = case name of {"main" -> "main"; x -> prefix ++ name}
                return $ Left (pos, ty', procName, zip args' (fmap snd new_args), body, ret)
            Left x -> error $ "unreachable" ++ show x
        ) resolved_defvar
    -- Finally, resolve procedure bodies.
    -- Note that we need to store byval-passed values (e.g. int) into new variables.
    body<-mapM (\node->case node of
        Right x->return x
        Left (pos, ty, func_name, args, body, ret@(ETempVar pret ret_id))-> do
            scope
            -- resolve return value
            ret_var<-case ty of
                Type _ Unit [] -> do
                    s<-defineSym (SymTempVar ret_id) pret (Type () Ref [ty])
                    return $ Nothing
                _ -> do
                    s<-defineSym (SymTempVar ret_id) pret (Type () Ref [ty])
                    return $ Just (Type () Ref [ty], s)
            -- resolve args
            (args', new_tempvars)<-flip runStateT [] $ mapM (\(ty, i)->case ty of
                Type _ Int [] -> do
                    temp_arg<-lift $ nextId -- Temporary argument
                    s<-lift $ defineSym (SymTempArg temp_arg) pos (intType ())
                    -- Leave the good name for defvar.
                    --real_arg<-lift $ defineSym (SymVar i) pos (refType () (intType ()))
                    modify' (++[NDefvar pos [(intType pos, i, Just $ ETempArg pos temp_arg )]])
                    return (ty, s)
                x -> do
                    s<-lift $ defineSym (SymVar i) pos x
                    return (ty, s) 
                    ) args
            -- resolve body
            body'<-mapM typeCheckAST (new_tempvars++body)
            ret''<-case ty of
                Type _ Unit [] -> return Nothing
                _ -> do
                    ret'<-typeCheckExpr ret
                    ret''<-matchType [Exact ty] ret'
                    return $ Just ret''
            unscope
            return $ NResolvedProcedureWithRet (okStmt pos) ty func_name args' body' ret'' ret_var
        Left _ -> error "unreachable"
        ) resolved_headers

    m <- gets mainDefined
    when (isMain && not m) $ do
        throwError $ MainUndefined

    -- Extract global symbols
    symtable <- gets symbolTable
    let topLayer = getSecondLast symtable
    let lis = MultiMap.toList topLayer
    let globalLis = filter (isGlobal . snd) lis
    let globalLayer = MultiMap.fromList globalLis
    ssaId <- gets ssaAllocator
    return (body, globalLayer, ssaId)

getSecondLast :: [a] -> a
getSecondLast [] = error "Empty list"
getSecondLast [x] = error "Single-element list"
getSecondLast (x:_:[]) = x
getSecondLast (x:xs) = getSecondLast xs

typeCheckTop :: Bool -> String -> [LAST] -> SymbolTableLayer -> Int -> Either TypeCheckError ([TCAST], SymbolTableLayer, Int)
typeCheckTop isMain prefix ast stl ssaId = do
    let env = TypeCheckEnv [MultiMap.empty, stl] ssaId False
    evalState (runExceptT $ typeCheckToplevel isMain prefix ast) env

-- TODO: unification-based type check and type inference.

data TyAtom= TInt | TQbit | TBool | TDouble | TComplex | TList | TKnownList Int | TUser String | TRange | TGate Int | TRef | TVal | TFunc deriving (Show, Eq)
data Ty = TMultiple { tyArgs :: [Ty] } | TAtom TyAtom | TVar Int deriving (Show, Eq)

