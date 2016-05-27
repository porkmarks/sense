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

	if ($_POST["password"] != $_POST["repassword"])
	{
		header("Location: register.php?error=Sorry...the password did not match! Try again.");
	} 
	else
	{
		$fname = $_POST["fname"];
		$lname = $_POST["lname"];
		$company =$_POST["company"];
		$email = $_POST["email"];
		$password = $_POST["password"];
		$repassword = $_POST["repassword"];

		$fname = mysqli_real_escape_string($db, $fname);
		$lname = mysqli_real_escape_string($db, $lname);
		$email = mysqli_real_escape_string($db, $email);
		$company = mysqli_real_escape_string($db, $company);
		$password = md5($password);
		$password = mysqli_real_escape_string($db, $password);
		
		
		
		
		$sql="SELECT email FROM Users WHERE email='$email'";
		$result=mysqli_query($db,$sql);
		$row=mysqli_fetch_array($result,MYSQLI_ASSOC);
		if(mysqli_num_rows($result) > 0)
		{
			header("Location: register.php?error=Sorry...This email already exist...");
		}
		else
		{
			$query = mysqli_query($db, "INSERT INTO Users (first_name, last_name, company, email, password) VALUES ('$fname', '$lname', '$company', '$email', '$password')");
			echo("bubu");
			if($query)
			{
				echo("caca");
				$msg = "Thank You! you are now registered.";
				header('Location: overview.php');
			}
		}
	}
}
?>
