<?php

include ("db.php");	

if(isset($_POST["submit"]))
{
	$db = $_SESSION["mainDb"];

	if (mysqli_connect_errno())
	{
		echo "Failed to connect to MySQL: " . mysqli_connect_error();
	}
	$name = $_POST["name"];
	$email = $_POST["email"];
	$password = $_POST["password"];

	$name = mysqli_real_escape_string($db, $name);
	$email = mysqli_real_escape_string($db, $email);
	$password = mysqli_real_escape_string($db, $password);
	$password = md5($password);
	
	
	
	$sql="SELECT email FROM Users WHERE email='$email'";
	$result=mysqli_query($db,$sql);
	$row=mysqli_fetch_array($result,MYSQLI_ASSOC);
	if(mysqli_num_rows($result) > 0)
	{
		header("Location: register.php?error=Sorry...This email already exist...");
	}
	else
	{
		$query = mysqli_query($db, "INSERT INTO Users (name, email, password) VALUES ('$name', '$email', '$password')");
		if($query)
		{
			$msg = "Thank You! you are now registered.";
			header('Location: index.php');
		}
	}

	echo $msg;
}
?>
