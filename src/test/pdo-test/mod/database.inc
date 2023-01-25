

<?php
require_once('def.inc');
	function dbconnect() {
		try { 
			$string ="pgsql:host=".HOSTNAME.";port=".PORT.";dbname=".DBNAME;
			$dbh = NULL;
			$dbh = new PDO($string,USER,PASS);
			$dbh->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);;
			return $dbh;
		} catch (Exception $e) {
			echo "fail : " .$e->getMessage()." <BR>";
			ErrorResult($php_errormsg);
			return NULL;
		}
	}

	function trans_end($dbh,$trans) {
		try { 
			$stmt = $dbh->prepare($trans);
			$stmt->execute();
			print_r($stmt->fetch());
		} catch (Exception $e) {
			ErrorResult($php_errormsg);
		}
	}

	function ErrorResult($php_errormsg) {
		header('HTTP/1.0 402 SQL ERROR');
		require_once("errorhandler.inc");
		set_error_handler("userErrorHandler");
		trigger_error($php_errormsg, E_USER_ERROR);
		exit;
	}
?>
