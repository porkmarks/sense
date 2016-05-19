<?php
	ini_set('display_errors', 1);
	ini_set('display_startup_errors', 1);
	error_reporting(E_ALL);

	include('../data/checklogin.php');
	include('../data/db.php');

	$db = $_SESSION["sensorsDb"];
			$sql = "SELECT * FROM 'configs' ORDER BY creation_timestamp DESC;";

			$result = mysqli_query($db,$sql);
			$rows = mysqli_fetch_array($result, MYSQLI_NUM);
			
			
			if (mysqli_num_rows($result) > 0)
			{
				$cp = $row[0]["comms_period"];
				$mp = $row[0]["measurement_period"];
				
			} 

?>
<!DOCTYPE html>
<head><title>Sensor settings</title></head>
<?php
	
	include('../data/includes.php');
?>

<body>
	<header>
		<?php
			include('../data/topBar.php');
		?>
    </header>

<div id="Content">
		<?php
			include('../settings/sideBarSettings.php');
			
			
		?>
		<div id="ContentRight">
		<?php echo $mp; ?>
			<form action ="updateSensorSettings.php" method="post" id="SensorSettingsForm">
			<table width="400" border="0" cellpadding="10" cellspacing="10">
			<tr>
				<td style="font-weight: bold"><div align="right"><label for="name">Measurement period</label><p> the frequency of the measurements</p></div></td>
				<td><input name="period" type="text" value ="<?php echo $mp; ?>" class="StyleTxtField" size="25" required /></td>
			</tr>
			<tr>
				<td style="font-weight: bold"><div align="right"><label for="email">Measurement comms</label><p>the frequency of measurement reporting</p></div></td>
				<td><input name="comm" type="text" value ="<?php echo $cp; ?>" class="StyleTxtField" size="25" required /></td>
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