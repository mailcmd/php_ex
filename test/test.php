<?php
ini_set('display_errors', 0);
header();
header("HTTP/1.0 408 Request Timeout");

$secs = random_int(2, 7);
echo "Sleeping $secs seconds...\n";
//sleep(15); //$secs);

echo "SESS STATUS: ".session_status()."\n";
echo "SESS START: ".session_start()."\n";
echo "SESS STATUS: ".session_status()."\n";
echo "SESS ID: ".session_id()."\n";

echo "Hello world!\n";
header("Location: /");

$_SESSION['hola'] = 'mundo';

echo "GET -------------------\n";
var_dump($_GET);
echo "POST -------------------\n";
var_dump($_POST);
echo "REQUEST -------------------\n";
var_dump($_REQUEST);
echo "SESSION -------------------\n";
var_dump($_SESSION);
echo "COOKIE -------------------\n";
var_dump($_COOKIE);
echo "-------------------\n";

?>
