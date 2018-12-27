<?php
include('checklogin.php');
include ("db.php");	

if(isset($_POST["delete"]))
{

	$db = mysqli_connect(DB_SERVER,DB_USERNAME,DB_PASSWORD, $_SESSION["sensorDB"]);
	if (mysqli_connect_errno())
	{
		//echo "Failed to connect to MySQL: " . mysqli_connect_error();

		echo"Error - SQLSTATE %s.\n", mysqli_sqlstate($db);
	

	}
	else
	{
  
		$cheks = implode("','", $_POST['checkbox']);
		$sql = "DELETE FROM alerts WHERE alerts.id IN ('$cheks')";
		$result = mysqli_query($db, $sql)

		or die(mysqli_error());
		
		

        if ($result)
		{				
			$msg = "Alarm deleted";
			echo $msg;
			header('Location: createAlarms.php');
		}
      }
      
	mysqli_close();
} 
?>