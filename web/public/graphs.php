<?php
	session_start();
	include('checklogin.php');

	ini_set('display_errors', 1); 
error_reporting(E_ALL);
?>
<!DOCTYPE html>
<head><title>GRAFICE</title>
<?php

	include('includes.php');
	include('db.php');

	$db = mysqli_connect(DB_SERVER, DB_USERNAME, DB_PASSWORD, $_SESSION["sensorDB"]);
	if (mysqli_connect_errno())
	{
		echo "Failed to connect to MySQL: " . mysqli_connect_error();
	}
    $sql = "SELECT * FROM sensors;";
    $sensorListResult = mysqli_query($db, $sql);

    if (!isset($_POST["beginDate"]) || !isset($_POST["endDate"]))
    {
    	$endDate = time();
    	$beginDate = $endDate - 3600*24*7; //one week back
    }
    else
    {
    	$format = '%d/%m/%Y';
		$endDate = strtotime($_POST["endDate"]);
		$beginDate = strtotime($_POST["beginDate"]);
    }

    echo strftime("%D", $beginDate).' xxx '.strftime("%D", $endDate);
    $beginDateStr = strftime("%Y-%m-%d", $beginDate);
    $endDateStr = strftime("%Y-%m-%d", $endDate);
    $sql = "SELECT * FROM measurements WHERE timestamp >= '$beginDateStr' AND timestamp <= '$endDateStr' ORDER BY sensor_id ASC, timestamp ASC;";
    echo $sql;
    $measurementsResult = mysqli_query($db, $sql);

	$measurementsPerSensor = array();
	{
		$sensorIdx = -1;
		$oldSensorId = -1;
		while ($row = mysqli_fetch_array($measurementsResult, MYSQLI_ASSOC))
		{
			$crtSensorId = $row["sensor_id"];
			if ($crtSensorId != $oldSensorId)
			{
				//new sensor detected
				$oldSensorId = $crtSensorId;
				$sensorIdx++;

				$measurementsPerSensor[$sensorIdx] = array();
			}

			array_push($measurementsPerSensor[$sensorIdx], $row);
		}  
	}

	//now see if there are too many entries per sensor and reduce
	foreach ($measurementsPerSensor as $measurements) 
	{
    	$count = count($measurements);
    	$x = $count / 1000;
    }

?>
<link href= "css/graphs.css" rel="stylesheet" type ="text/css"/>

<body>

	<div id="Holder">
		<?php
			include('mainTopBar.php');
		?>
	</div>

		<?php
			include('mainSideBar.php');
		?>
		<script src="http://d3js.org/d3.v3.min.js" charset="utf-8"></script>
		<!--highlight the current section of the navigation bar-->
		<script>
		$(function(){
		  $('a').each(function() {
		    if ($(this).prop('href') == window.location.href) {
		      $(this).addClass('current');
		    }
		  });
		});
		</script>
		<div id="ContentRight">
			
			<form action ="graphs.php" method="post" id="refreshGraphs">
			<input type='submit' id='refresh' name='refresh' value='Refresh'/>

			<div id="datePkr">
			Begin Date: <input type="text" id="beginDate" name="beginDate">
			End Date: <input type="text" id="endDate" name="endDate">
			</div>
			<div id="option">
			    <input name="DayButton" 
			           type="button" 
			           value="Last Day"
			           onclick="updateDay()" />

			    <input name="weekButton" 
			           type="button" 
			           value="Last Week"
			           onclick="updateWeek()"/>

			    <input name="MonthButton" 
			           type="button" 
			           value="Last Month"
			           onclick="updateMonth()"/>

			    <input name="SemesterButton" 
			           type="button" 
			           value="Last Semester"
			           onclick="updateSemester()"/>

			    <input name="YearButton" 
			           type="button" 
			           value="Last Year"
			           onclick="updateYear()"/>
			</div>
			<div class="checkboxListScroll">
			<?php
				////////////////////////////////////////
				//show the sensor names
	        	$colors = ["#FF33AA", "#08B37F", "#22ADFF", "#FF4136", "#FF9900", "#3D0000", "#854144b", "#FF3399", "#AAAAAA"];
	        	$idx = 0;
		        while ($row = mysqli_fetch_array($sensorListResult, MYSQLI_ASSOC))
	            {
	        ?> 
	         <ul id="checkboxList" class="checkboxList"> 		  	
	            <label style = "color: <?php echo $colors[$idx] ?>;"><input type="checkbox" id ="<?php echo $row["name"]; ?>" checked="true"  > <?php echo $row["name"]; ?></label>
	         </ul>
	        <?php
	        		$idx++;
	         	}
				////////////////////////////////////////
	        ?>   	
			</div>
			<div id ="graphContainer"></div>

			<div class = "container">
			<div id = "checkboxes"> 
			   <label><input type="checkbox" id = "temp" checked="true"> Temperature</label>
			   <label><input type="checkbox" id = "hum" checked="true"> Humidity</label>
			</div>

			<p id ="download" onclick="downloadCsv()"><img src="assets/downloadfile.png"  width="24" height="24"> Download CSV</p>
			
			<!-- need to do the downloadCsv() function--> 
			<script>

			$('#beginDate')
				.datepicker({
					//showButtonPanel: true,
					dateFormat: "dd-mm-yy",
					defaultDate: new Date()
				})
				.datepicker('setDate', new Date(<?php echo $beginDate*1000; ?>));
				

			$('#endDate')
				.datepicker
				({
					//showButtonPanel: true,
					dateFormat: "dd-mm-yy",
					defaultDate: new Date()
				})
				.datepicker('setDate', new Date(<?php echo $endDate*1000; ?>));
				

			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			// Globals

			var graphData = {}; //will be filled later

			var colors = ["#FF33AA", "#08B37F", "#22ADFF", "#FF4136", "#FF9900", "#3D0000", "#854144b", "#FF3399", "#AAAAAA", ];

			var selectedPlots = [];

			var checkbTemp = document.getElementById("temp");
			var checkbHum = document.getElementById("hum");

			var parseDate = d3.time.format("%Y-%m-%d %H:%M:%S").parse;

			var measurementDatas = [
			<?php
				foreach ($measurementsPerSensor as $sensorIdx => $measurements) 
				{
					//start a new sensor measurement array
					echo "{\n";
					echo "measurements: [\n";

					foreach ($measurements as $row) 
					{
						echo "{\n";
						echo "\tdate: parseDate(\"".$row["timestamp"]."\"),\n";
						echo "\ttemperature: ".$row["temperature"].",\n";
						echo "\thumidity: ".$row["humidity"].",\n";
						echo "},\n";
					}

					echo "\t],\n";
					echo "\tcolor: \"" . $colors[$sensorIdx] . "\"\n";
					echo "},\n";
				}
			?>
			];


			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			// Functions

			checkbTemp.onchange = function()
			{
				refreshGraphs();
			};
			checkbHum.onchange = function()
			{
				refreshGraphs();
			};

			window.onresize = function(event) 
			{
				d3.select("svg").remove();
				graphData = createGraphs();
				refreshGraphs();
			};

			function updateDay()
			{			
				$("#endDate").datepicker('setDate', new Date());
				var date = $("#endDate").datepicker('getDate'); 
				var days = date.getDate() -1;
				date.setDate(days);
				$("#beginDate").datepicker('setDate', date);
			}

			function updateMonth()
			{
				$("#endDate").datepicker('setDate', new Date());
				var date = $("#endDate").datepicker('getDate'); 
				var month = date.getMonth() - 1;
				date.setMonth(month);
				$("#beginDate").datepicker('setDate', date);
			}
			function updateWeek()
			{			
				$("#endDate").datepicker('setDate', new Date());
				var date = $("#endDate").datepicker('getDate'); 
				var days = date.getDate() -7;
				date.setDate(days);
				$("#beginDate").datepicker('setDate', date);
			}
			function updateSemester() 
			{
				$("#endDate").datepicker('setDate', new Date());
				var date = $("#endDate").datepicker('getDate'); 
				var month = date.getMonth() - 6;
				date.setMonth(month);
				$("#beginDate").datepicker('setDate', date);
			}
			function updateYear() 
			{
				$("#endDate").datepicker('setDate', new Date());
				var date = $("#endDate").datepicker('getDate'); 
				var month = date.getMonth() - 12;
				date.setMonth(month);
				$("#beginDate").datepicker('setDate', date);
			}

			function createGraphs() 
			{
				// Set the dimensions of the canvas / graph
				var windowWidth = window.innerWidth;
				var margin = {top: 30, right: 60, bottom: 30, left: 60};
				var graphWidth = windowWidth- 550- margin.left - margin.right;
				var graphHeight = 300 - margin.top - margin.bottom;

				// Set the ranges
				var xRange = d3.time.scale().range([0, graphWidth]);
				var y0Range = d3.scale.linear().range([graphHeight, 0]);
				var y1Range = d3.scale.linear().range([graphHeight, 0]);

				// Define the axes

				var xAxis = d3.svg.axis().scale(xRange)
				    .orient("bottom").ticks(graphWidth/100).tickFormat(d3.time.format('%d %b'));

				var formatter = d3.format(".1f")
				var yAxisLeft = d3.svg.axis().scale(y0Range)
				    .orient("left").ticks(5).tickFormat(function(d) { return formatter(d) + "°C"; });
				var yAxisRight = d3.svg.axis().scale(y1Range)
				    .orient("right").ticks(5).tickFormat(function(d) { return formatter(d) + "%"; });

				// Adds the svg canvas
				var svg = d3.select("#graphContainer")
							.append("svg")
							.attr("width", graphWidth + margin.left + margin.right)
							.attr("height", graphHeight + margin.top + margin.bottom)
							.append("g")
							.attr("transform", "translate(" + margin.left + "," + margin.top + ")");
				svg.append("g")
					.attr("class", "x axis")
					.attr("transform", "translate(0," + graphHeight + ")")
					.call(xAxis);

				// Add the Y Axis
				svg.append("g")
					.attr("class", "y axis axisLeft")
					.call(yAxisLeft);
				svg.append("g")
					.attr("class", "y axis axisRight")
					.attr("transform", "translate(" + graphWidth + " ,0)")
					.call(yAxisRight);


				var pd = {
					xRange: xRange,
					y0Range: y0Range,
					y1Range: y1Range,
					graph: svg,
					xAxis: xAxis,
					yAxisLeft: yAxisLeft,
					yAxisRight: yAxisRight
				}
				return pd;
			}
	
			function refreshGraphs()
			{	
				var filter = 0;// 1- show temperature, 2- show humidity, 3- show both

				var checkbTemp = document.getElementById("temp");
				var checkbHum = document.getElementById("hum");
				if (checkbTemp.checked == true)
				{
					filter += 1;
					graphData.graph.select(".y.axisLeft").attr("visibility","visible");
				}
				else
				{
					graphData.graph.select(".y.axisLeft").attr("visibility","hidden");
				}
				if (checkbHum.checked == true)
				{
					filter += 2;
					graphData.graph.select(".y.axisRight").attr("visibility","visible");
				}
				else
				{
					graphData.graph.select(".y.axisRight").attr("visibility","hidden");
				}

				var parser = d3.time.format("%d/%m/%Y").parse;
				
				var beginDate = $("#beginDate").datepicker('getDate'); 
				var endDate = $("#endDate").datepicker('getDate'); 
				var min = endDate.getMinutes();
				endDate.setMinutes(min + 1439);

				d3.selectAll('.line').remove();

				if (beginDate != null && endDate != null && beginDate.getTime() < endDate.getTime())
				{
					plotGraph(graphData, measurementDatas, filter)
				}
				if ((endDate.getTime() - beginDate.getTime()) <= 86340000)
				{
					 graphData.xAxis.tickFormat(d3.time.format('%d %b %H:%M'));

				}
				else
				{
					 graphData.xAxis.tickFormat(d3.time.format('%d %b'));
				}
			}

			function plotGraph(graphData, measurementDatas, what)
			{
				var parseDate = d3.time.format("%Y-%m-%d %H:%M:%S").parse;

				// Define the line
				var valuelineTemp = d3.svg.line()
				    .x(function(d) { return graphData.xRange(d.date); })
				    .y(function(d) { return graphData.y0Range(d.temperature); });

				var valuelineHum = d3.svg.line()
				    .x(function(d) { return graphData.xRange(d.date); })
				    .y(function(d) { return graphData.y1Range(d.humidity); });
				       

				// Get the data
				for(var i = 0; i < measurementDatas.length; i++)
				{
					var measurements = measurementDatas[i].measurements;
					var color = measurementDatas[i].color;

					graphData.xRange.domain(d3.extent(measurements, function(d) { return d.date; }));
					if (what == 1 || what == 3)
					{
						graphData.y0Range.domain(d3.extent(measurements, function(d) { return d.temperature; }));
					    graphData.graph.append("path")
							.attr("class", "line")
							.attr("stroke", color)
							//.style("stroke-dasharray", ("10,2"))valueline
							.attr("d", valuelineTemp(measurements));
					}

					if (what == 2 || what == 3)
					{
						graphData.y1Range.domain(d3.extent(measurements, function(d) { return d.humidity; }));
						graphData.graph.append("path")
							.attr("class", "line")
							.attr("stroke", color)
							.style("stroke-dasharray", ("3,3"))
							.attr("d", valuelineHum(measurements));
					}
					
				    var t = graphData.graph.transition().duration(100);
				    t.select(".x.axis").call(graphData.xAxis);
				    t.select(".y.axisLeft").call(graphData.yAxisLeft);
				    t.select(".y.axisRight").call(graphData.yAxisRight);
				}

			}

			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			graphData = createGraphs();
			refreshGraphs();

			</script>

		    </div>
		    </form>

</body>
</html>
