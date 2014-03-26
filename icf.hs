import System.Environment   
import Data.List 

main = do
    args <- getArgs
    progName <- getProgName
    mapM putStrLn args
