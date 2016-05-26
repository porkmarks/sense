<?php

session_start();
include('db.php');


if(isset($_POST["submit"]))
{
	$db = mysqli_connect(DB_SERVER,DB_USERNAME,DB_PASSWORD, $_SESSION["sensorDB"]);
	if (mysqli_connect_errno())
	{
		echo "Failed to connect to MySQL: " . mysqli_connect_error();
	}

	$mesP = $_POST["mesp"];
	$mesC = $_POST["mesc"];

	$mesP = mysqli_real_escape_string($db, $mesP);
	$mesC = mysqli_real_escape_string($db, $mesC);
	
	$query = "INSERT INTO configs (measurement_period, comms_period) VALUES ($mesP, $mesC);";

	mysqli_query($db, $query);

	mysqli_close($db);

    header("Location: sensorSettings.php");
}


?>
