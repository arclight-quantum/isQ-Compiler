{-# LANGUAGE ViewPatterns #-}
module ISQ.Lang.DeriveGate where
import ISQ.Lang.ISQv2Grammar
import Control.Monad.Extra (concatMapM)
import Control.Monad (void)

data DeriveError =
      BadGateSignature Pos
    | BadOracleSignature Pos deriving (Eq, Show)
mangleGate x = "$_ISQ_GATEDEF_"++x
mangleOracle x = "@"++x

isQuantumData :: Type ann->Bool
isQuantumData (Type _ Qbit _) = True
isQuantumData (Type _ _ xs) = any isQuantumData xs
countGateSize :: [LType]->Maybe Int
countGateSize = (snd<$>). foldr go (Just (False, 0)) where
    go _ Nothing = Nothing
    go (isQuantumData->True) (Just (True, _)) = Nothing
    go (isQuantumData->False) (Just (True, x)) = Just (True, x)
    go (isQuantumData->False) (Just (False, x)) = Just (True, x)
    go (Type _ Qbit []) (Just (False, x)) = Just (False, x+1)
    go (isQuantumData->True) (Just (False, x)) = Nothing

checkOracleDef :: [LType]->Int->Bool
checkOracleDef types size = length types >= size &&
    let (extra_args, bits) = splitAt (length types - size) types
        check_bit (Type _ Bool []) = True
        check_bit _ = False
    in not (any isQuantumData extra_args) && all check_bit bits

getStatement :: [LAST]->Bool->[LAST]
getStatement [] b = []
getStatement (x:xs) b = do
    let r = getStatement xs b
    case x of
        (NCall ann exp) -> if b then r ++ [NCallWithInv ann exp [Inv]] else []
        (NCoreUnitary ann gate ops mods) -> if b then r ++ [NCoreUnitary ann gate ops (mods++[Inv])] else []
        _ -> if b then r else [x] ++ r

deriveGate' :: LAST->Either DeriveError [LAST]
deriveGate' (NProcedureWithDerive ann returnType procName args body (Just DeriveGate)) = do
    let unitaryState = getStatement body True
    let otherState = getStatement body False
    --traceM $ show unitaryState
    case countGateSize (map fst args) of
        Nothing -> Left $ BadGateSignature ann
        (Just x) -> let mangled_gate = mangleGate procName in Right [
            NProcedure ann returnType mangled_gate args body,
            NDerivedGatedef ann procName mangled_gate (take (length args - x) $ void . fst <$> args) x,
            NProcedure ann returnType (mangled_gate ++ "_inv") args (otherState ++ unitaryState),
            NDerivedGatedef ann (procName ++ "_inv") (mangled_gate ++ "_inv") (take (length args - x) $ void . fst <$> args) x
            ]
deriveGate' (NProcedureWithDerive ann returnType procName args body (Just (DeriveOracle bitcount))) =
    if checkOracleDef (map fst args) bitcount then
        (let oracle_name = mangleOracle procName
        in Right [
            NProcedure ann returnType procName args body,
            NDerivedGatedef ann oracle_name procName (take (length args - bitcount) $ void . fst <$> args) bitcount])
    else Left $ BadGateSignature ann
deriveGate' (NProcedureWithDerive ann returnType procName args body Nothing) =
    Right [NProcedure ann returnType procName args body]
deriveGate' x = Right [x]

passDeriveGate :: [LAST] -> Either DeriveError [LAST]
passDeriveGate = concatMapM deriveGate'
