<?php
ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);

include ("db.php");
session_start();	


if(isset($_POST["submit"]))
{

	$db = mysqli_connect(DB_SERVER,DB_USERNAME,DB_PASSWORD,DB_DATABASE);
	if (mysqli_connect_errno())
	{
		echo "Failed to connect to MySQL: " . mysqli_connect_error();
	}

	$password = $_POST["password"];
	$repassword = $_POST["repassword"];

	if (empty($password) || $password != $repassword)
	{
		header("Location: settings.php?error=Sorry...the password field was empty or the passwords did not match! Try again.");
	} 
	else
	{
		$fname = mysqli_real_escape_string($db, $_POST["fname"]);
		$lname = mysqli_real_escape_string($db, $_POST["lname"]);
		$email = mysqli_real_escape_string($db, $_POST["company"]);
		$company = mysqli_real_escape_string($db, $_POST["email"]);
		$password = mysqli_real_escape_string($db, md5($password));
		$currentFname = $_SESSION["fname"];
		
		$sql = "UPDATE Users SET first_name = '$fname', last_name ='$lname',company = '$company', email = '$email', password = '$password' WHERE first_name = '$currentFname';";
		echo $sql;

		$query = mysqli_query($db, $sql);
		if($query)
		{
			echo("caca");
			//header('Location: settings.php');
		}
	}
}
?>