<?php

  function debug($con,$result,$indent,$screen) {
    $str = $indent.$result."......";
    eval ("\$result = $result;");
    if ($screen == 'ON' || $screen == 'on' || $screen == 'On') {
      echo $str.$result."<br>";
    }
    return $result;
  }
?>
