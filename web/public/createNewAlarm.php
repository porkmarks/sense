<?php
include('checklogin.php');
include ("db.php");	

if(isset($_POST["create"]))
{

	$db = mysqli_connect(DB_SERVER,DB_USERNAME,DB_PASSWORD, $_SESSION["sensorDB"]);
	if (mysqli_connect_errno())
	{
		echo "Failed to connect to MySQL: " . mysqli_connect_error();
	}
	else
	{
		$source = $_POST["source"];
		$direction = $_POST["direction"];
		$value =$_POST["value"];
		$duration = $_POST["duration"];

		$sensor_id = 0;
		$measurement = 0;

		if ($source == "any_h")
		{
			$sensor_id = -1;
			$measurement = 1;
		}
		else if ($source == "any_t")
		{
			$sensor_id = -1;
			$measurement = 0;
		}
		else
		{
			$sensor_id = abs(intval($source));
			if(intval($source) < 0)
			{
				$measurement = 1;
			}
			else
			{
				$measurement = 0;
			}
		}

		echo $sensor_id;
		echo "<br>";
		echo $measurement;



		$sql = "INSERT INTO alerts (sensor_id, above, value, duration, measurement) VALUES ('$sensor_id', '$direction', '$value', '$duration', '$measurement')";
		$query = mysqli_query($db, $sql);
			
		if ($query)
		{				
			$msg = "Alarm created";
			echo$msg;
			header('Location: createAlarms.php');
		}
		else
		{
			echo"Error - SQLSTATE %s.\n", mysqli_sqlstate($db);
		}	

	}
	mysqli_close();

}

?>
