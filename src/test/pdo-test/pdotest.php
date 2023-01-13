<?php
  require_once('def.inc');
  require_once('regsql.inc');
  ini_set("track_errors", "1"); 
  error_reporting(0);
	$trans = NULL;
  if (isset($_GET["query"])) {
    $trans=$_GET["query"];
  }
  dosql($trans); 
?>
