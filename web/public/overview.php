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

    function compute_percent($x, $min, $max)
    {
        $x = (($x - $min) / ($max - $min)) * 100.0;
        $x = min(max($x, 0.0), 100.0);
        return $x;
    }

?>
<link href= "css/overview.css" rel="stylesheet" type ="text/css"/>



<body>

	<div id="Holder">
		<?php
			include('mainTopBar.php');
		?>
	</div>

	<div id="Content">
        <div id ="ContentLeft">
    		<?php
    			include('mainSideBar.php');
    		?>
        </div>
		<div id="ContentRight">

        <table id ="overviewTable" style="width:80%">

        <?php
        
	        while ($row = mysqli_fetch_array($result, MYSQLI_ASSOC))
            {
                $battery_percent = compute_percent($row["vcc"], 2.0, 3.4);
                if ($battery_percent <= 25)
                {
                    $battery_image = "assets/battery25.png";
                }
                else if ($battery_percent <= 50)
                {
                    $battery_image = "assets/battery50.png";
                }
                else if ($battery_percent <= 75)
                {
                    $battery_image = "assets/battery75.png";
                }
                else
                {
                    $battery_image = "assets/battery100.png";
                }

                $upload_signal_percent = compute_percent($row["b2s_input_dBm"], -110.0, -50.0);
                $download_signal_percent = compute_percent($row["s2b_input_dBm"], -110.0, -50.0);

                $reading_time = $row["timestamp"];    
                $elapsed_time = time() - $reading_time;

                $elapsed_text = gmdate('H:i:s', $elapsed_time);

                echo "<tr>";

                echo 
                    "<td>".$row["name"]."</td>".
                    "<td><div align=\"right\"><img src=\"assets/thermometer.png\" width=\"32\"/></td>".
                    "<td><div align=\"left\">Temperature<br>".$elapsed_text."</td>".
                    "<td><div align=\"left\">".$row["temperature"]."°C</td>".
                    "<td><div align=\"right\"><img src=\"assets/humidity.png\" width=\"32\"/></td>".
                    "<td><div align=\"left\">Humidity<br>".$elapsed_text."</td>".
                    "<td><div align=\"left\">".number_format($row["humidity"], 0)."%</td>".
                    "<td><div align=\"right\"><img src=\"".$battery_image."\" width=\"32\"/></td>".
                    "<td><div align=\"left\">".number_format($battery_percent, 0)."%</td>".
                    "<td>".($row["flags"] == 1 ? "Bad Sensor" : "None")."</td>".
                    "<td><div align=\"right\"><img src=\"assets/upload_signal.png\" width=\"32\"/></td>".
                    "<td><div align=\"left\">".number_format($upload_signal_percent, 0)."%</td>".
                    "<td><div align=\"right\"><img src=\"assets/download_signal.png\" width=\"32\"/></td>".
                    "<td><div align=\"left\">".number_format($download_signal_percent, 0)."%</td>";

                echo "</tr>\n";
            }
        ?>

            </table>
		
	</div>
    </div>

</body>
</html>
