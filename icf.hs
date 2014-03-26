import System.Environment   
import Data.List 

-- . every line read is trimmed
-- . lines starting with #, //, or -- are comments
-- . first column specifies selector, second denotes group, the rest defines attributes
-- . selectos form tree structure
-- . groups are sets of symbols; groups can be spcifies as per set operations; symbols separated by , can appear as group
-- . #include in effect copies the content of included file in place, recursive include incurs program exit
-- . diff displayed per symbol, or per group if possible
-- . xuefei yang

main = do
    args <- getArgs
    progName <- getProgName
    mapM putStrLn args
