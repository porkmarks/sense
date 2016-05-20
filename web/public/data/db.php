<?php
ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);

define('DB_SERVER', '192.168.1.36');
define('DB_USERNAME', 'sense');
define('DB_PASSWORD', 'nHP3cNx4WTwpGaFP');
define('DB_DATABASE', 'TempR');

if (!isset($_SESSION["mainDb"]))
{
	$_SESSION["mainDb"] = mysqli_connect(DB_SERVER,DB_USERNAME,DB_PASSWORD,DB_DATABASE);
	if (mysqli_connect_errno())
	{
		echo "Failed to connect to MySQL: " . mysqli_connect_error();
	}
}
?>
