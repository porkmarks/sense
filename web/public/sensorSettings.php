<?php
	ini_set('display_errors', 1);
	ini_set('display_startup_errors', 1);
	error_reporting(E_ALL);

	include('checklogin.php');
	include('db.php');

	$db = mysqli_connect(DB_SERVER,DB_USERNAME,DB_PASSWORD, $_SESSION["sensorDB"]);
	if (mysqli_connect_errno())
	{
		echo "Failed to connect to MySQL: " . mysqli_connect_error();
	}
	$sql = "SELECT * FROM configs ORDER BY start_timestamp DESC;";

	$result = mysqli_query($db,$sql);
	$row = mysqli_fetch_array($result, MYSQLI_ASSOC);
	
	
	if (mysqli_num_rows($result) > 0)
	{
		$cp = $row["comms_period"] / 60.0; //convert seconds to minutes
		$mp = $row["measurement_period"] / 60.0; //convert seconds to minutes
	} 

?>
<!DOCTYPE html>
<head><title>Sensor settings</title></head>
<?php
	
	include('includes.php');
?>

<body>
	<header>
		<?php
			include('mainTopBar.php');
		?>
    </header>

<div id="Content">
		<?php
			include('settingsSideBar.php');
			
			
		?>
		<div id="ContentRight">
		
			<form action ="updateSensorSettings.php" method="post" id="SensorSettingsForm">
			<table width="400" border="0" cellpadding="10" cellspacing="10">
			<tr>
				<td style="font-weight: bold"><div align="right"><label for="mesp">Measurement period</label><p> the frequency of the measurements in minutes</p></div></td>
				<td><input name="mesp" type="text" value ="<?php echo $mp; ?>" class="StyleTxtField" size="25" required /></td>
			</tr>
			<tr>
				<td style="font-weight: bold"><div align="right"><label for="mesc">Measurement comms</label><p>the frequency of measurement reporting in minutes</p></div></td>
				<td><input name="mesc" type="text" value ="<?php echo $cp; ?>" class="StyleTxtField" size="25" required /></td>
			</tr>
			<tr>
				<td height="23" <div align="right"></td>
			    <td> <input type="submit" name="submit" value="Submit" class="StyleTxtField" /></td>
			</tr>
			
            </table>
		</form>
		</div>
	</div>


</body>
</html>
