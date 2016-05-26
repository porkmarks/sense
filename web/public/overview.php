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
    $sql = "SELECT s.id, s.name, m.timestamp, m.temperature, m.humidity, m.vcc, m.b2s_input_dBm, m.s2b_input_dBm\n"
        . "FROM sensors s\n"
        . "LEFT JOIN (\n"
        . " \n"
        . " SELECT m1.sensor_id, m1.timestamp, m1.temperature, m1.humidity, m1.vcc, m1.b2s_input_dBm, m1.s2b_input_dBm\n"
        . "	FROM measurements m1\n"
        . "	INNER JOIN (\n"
        . "	SELECT sensor_id, max(timestamp) AS timestamp\n"
        . "	FROM measurements\n"
        . "	GROUP BY sensor_id\n"
        . "	) m2 ON m1.sensor_id = m2.sensor_id AND m1.timestamp = m2.timestamp\n"
        . ") m ON s.id = m.sensor_id;";

	$result = mysqli_query($db, $sql);
?>

<!DOCTYPE html>
<head><title>Overview</title></head>
<?php
	include('includes.php');
?>
<link href= "css/overview.css" rel="stylesheet" type ="text/css"/>



<body>

	<div id="Holder">
		<?php
			include('mainTopBar.php');
		?>
	</div>

	<div id="Content">
		<?php
			include('mainSideBar.php');
		?>
		<div id="ContentRight">
			<h2>Overview</h2>

        <?php
	        while ($row = mysqli_fetch_array($result, MYSQLI_ASSOC))
            {
                echo $row["id"] . " " . 
                    $row["name"] . " " . 
                    $row["timestamp"] . " " . 
                    $row["temperature"] . " " . 
                    $row["humidity"] . " " . 
                    $row["vcc"] . " " . 
                    $row["b2s_input_dBm"] . " " . 
                    $row["s2b_input_dBm"] . "<br>";
            }
        ?>

			<div class="alert warning">
				<p class ="subtler">
				You have new
				<a href="alarms.php">alarm notifications</a>
				</p>
		</div>
	</div>

</body>
</html>
