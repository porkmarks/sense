<?php

session_start();
include('../data/db.php');


if(isset($_POST["submit"]))
{
	$db = mysqli_connect(DB_SERVER,DB_USERNAME,DB_PASSWORD, $_SESSION["sensorDB"]);
	if (mysqli_connect_errno())
	{
		echo "Failed to connect to MySQL: " . mysqli_connect_error();
	}

	$mesP = $_POST["mesp"];
	$mesC = $_POST["mesc"];

	if ( (int)$mesP == $mesP && (int)$mesP > 0 && (int)$mesP == $mesP && (int)$mesP > 0 )
	{
		$mesP = mysqli_real_escape_string($db, $mesP);
		$mesC = mysqli_real_escape_string($db, $mesC);
		$location = "Location: ../settings/sensorSettings.php";

		$query = "INSERT INTO configs (measurement_period, comms_period) VALUES ($mesP, $mesC);";
		echo $query;

		mysqli_query($db, $query);

		mysqli_close($db);
	    unset($db);

		header($location);

	}	
	else 
	{
		echo "Please enter a positive number " . mysqli_connect_error();

	}
}


?>