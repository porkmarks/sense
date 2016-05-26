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

	$mesP = floor($_POST["mesp"] * 60.0); //convert minutes to seconds
	$mesC = floor($_POST["mesc"] * 60.0); //convert minutes to seconds

	$mesP = mysqli_real_escape_string($db, $mesP);
	$mesC = mysqli_real_escape_string($db, $mesC);
	
	$query = "UPDATE configs SET measurement_period=$mesP, comms_period=$mesC;";

	mysqli_query($db, $query);

	mysqli_close($db);

    header("Location: sensorSettings.php");
}


?>
