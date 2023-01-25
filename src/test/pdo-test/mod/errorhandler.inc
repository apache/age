<?php
function userErrorHandler ($errno, $errmsg, $filename, $linenum, $vars) {
    $dt = date("m-d H:i:s");
    $errortype = array (
                1   =>  "Error",
                2   =>  "Warning",
                4   =>  "Parsing Error",
                8   =>  "Notice",
                16  =>  "Core Error",
                32  =>  "Core Warning",
                64  =>  "Compile Error",
                128 =>  "Compile Warning",
                256 =>  "User Error",
                512 =>  "User Warning",
                1024=>  "User Notice"
                );
    $err = $dt." ";
    $err .= $errortype[$error]." ";
    $err .= $errmsg." ";
    $err .= $filename." ";
    $err .= $linenum."\n";
    $filename='log.txt';
    $handle = fopen($filename, 'a');
    fwrite($handle, $err);
    fclose($handle);
}
?>
