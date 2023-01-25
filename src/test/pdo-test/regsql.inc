<?php
  require_once("def.inc");
  require_once("mod/database.inc");
  require_once("collections.inc");

	function getfiles( $dir )
	{
	$tree = array();

	if( $handle = opendir( $dir ) )
	{
		$i = 0;
		while ( false !== $file = readdir( $handle ) )
		{
			if( $file != "." && $file != ".." )
			{
					$tree[$i] = $file;
					$i++;
			}
		}
		closedir( $handle );
		uasort( $tree, "strcmp" ); 
	}
	return $tree;
	}

  function dosql($trans) {
		$context = NULL;

		if($trans)
		{
    	$file = "SQLlist/".$trans;
			if(is_file($file))
			{
    		$context=file_get_contents($file);
			}
		}

		if(!$context)
		{
			$files =  getfiles("SQLlist");
			$files_num = count($files) -1;
      $num = rand(0,$files_num);
    	$context=file_get_contents("SQLlist/".$files[$num]);						
		}
		$sqlarray = explode(";",$context);
    $con = dbconnect();

		for($i = 0; $i<count($sqlarray) -1;  $i++)
		{
			$sql = $sqlarray[$i];
			echo $sql."<BR>";
    	trans_end($con,$sql);
		}
  }
?>
