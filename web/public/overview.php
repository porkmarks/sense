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
    $sql = "SELECT s.id, s.name, m.timestamp, m.temperature, m.humidity, m.vcc, m.flags, m.b2s_input_dBm, m.s2b_input_dBm\n"
        . "FROM sensors s\n"
        . "LEFT JOIN (\n"
        . "  SELECT m1.sensor_id, m1.timestamp, m1.temperature, m1.humidity, m1.vcc, m1.flags, m1.b2s_input_dBm, m1.s2b_input_dBm\n"
        . "	 FROM measurements m1\n"
        . "	 INNER JOIN (\n"
        . "	   SELECT sensor_id, max(timestamp) AS timestamp\n"
        . "	   FROM measurements\n"
        . "	   GROUP BY sensor_id\n"
        . "	 ) m2 ON m1.sensor_id = m2.sensor_id AND m1.timestamp = m2.timestamp\n"
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

            <table style="width:100%">
              <tr>
                <th>Id</td>
                <th>Name</td> 
                <th>Date/Time</td>
                <th>Temperature</td>
                <th>Humidity</td>
                <th>Battery</td>
                <th>Errors</td>
                <th>Base Signal</td>
                <th>Sensor Signal</td>
        <?php
	        while ($row = mysqli_fetch_array($result, MYSQLI_ASSOC))
            {
                echo "<tr>";

                echo "<td>".$row["id"]."</td>".
                    "<td>".$row["name"]."</td>".
                    "<td>".$row["timestamp"]."</td>".
                    "<td>".$row["temperature"]."°</td>".
                    "<td>".$row["humidity"]."%</td>".
                    "<td>".$row["vcc"]."V</td>".
                    "<td>".($row["flags"] == 1 ? "Bad Sensor" : "None")."</td>".
                    "<td>".$row["b2s_input_dBm"]."dBm</td>".
                    "<td>".$row["s2b_input_dBm"]."dBm</td>";

                echo "</tr>";
            }
        ?>

            </table>
			<div class="alert warning">
				<p class ="subtler">
				You have new
				<a href="alarms.php">alarm notifications</a>
				</p>
		</div>
	</div>

</body>
</html>
