<?php
	session_start();
	include('checklogin.php');
?>
<!DOCTYPE html>
<head><title>GRAFICE</title>
<?php

	include('includes.php');
?>
<body>
<header>
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
			
			<div id="datePkr">
			Begin Date: <input type="text" id="beginDate">
			End Date: <input type="text" id="endDate">
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
				<ul id="checkboxList" class="checkboxList"> </ul>
			</div>
			<div id ="graphContainer"></div>


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
			<div class = "container">
			<div id = "checkboxes"> 
			   <label><input type="checkbox"  id = "temp" checked="true"  > Temperature</label>
			   <label><input type="checkbox"  id = "hum" checked="true"   > Humidity</label>
			</div>
			<button type="button" class="btn btn-link">Download csv</button>

			<script>

			$('#beginDate')
				.datepicker({
					//showButtonPanel: true,
					dateFormat: "dd/mm/yy",
					defaultDate: -12,
					onSelect: function(dateStr) {
						refreshGraphs();
				    }
				})
				.datepicker('setDate', -7); 

			$('#endDate')
				.datepicker
				({
					//showButtonPanel: true,
					dateFormat: "dd/mm/yy",
					defaultDate: new Date(),
					onSelect: function(dateStr) {
						refreshGraphs();
				    }
				})
				.datepicker('setDate', new Date());

			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			// Globals

			var graphData = {}; //will be filled later

			var colors = ["#FF33AA", "#08B37F", "#22ADFF", "#FF4136", "#FF9900", "#3D0000", "#854144b", "#FF3399", "#AAAAAA", ];
			var sensorFiles = ["living.tsv", "hall.tsv", "bedroom.tsv", "bathroom.tsv", "kitchen.tsv" ];

			var selectedPlots = [];

			var checkbTemp = document.getElementById("temp");
			var checkbHum = document.getElementById("hum");


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
				graphData =createGraphs();
				refreshGraphs();
			};


			function updateDay()
			{			
				$("#endDate").datepicker('setDate', new Date());
				var date = $("#endDate").datepicker('getDate'); 
				var days = date.getDate() -1;
				date.setDate(days);
				$("#beginDate").datepicker('setDate', date);
				refreshGraphs();
			}

			function updateMonth()
			{
				$("#endDate").datepicker('setDate', new Date());
				var date = $("#endDate").datepicker('getDate'); 
				var month = date.getMonth() - 1;
				date.setMonth(month);
				$("#beginDate").datepicker('setDate', date);
				refreshGraphs();
			}
			function updateWeek()
			{			
				$("#endDate").datepicker('setDate', new Date());
				var date = $("#endDate").datepicker('getDate'); 
				var days = date.getDate() -7;
				date.setDate(days);
				$("#beginDate").datepicker('setDate', date);
				refreshGraphs();

			}
			function updateSemester() 
			{
				$("#endDate").datepicker('setDate', new Date());
				var date = $("#endDate").datepicker('getDate'); 
				var month = date.getMonth() - 6;
				date.setMonth(month);
				$("#beginDate").datepicker('setDate', date);
				refreshGraphs();
			}
			function updateYear() 
			{
				$("#endDate").datepicker('setDate', new Date());
				var date = $("#endDate").datepicker('getDate'); 
				var month = date.getMonth() - 12;
				date.setMonth(month);
				$("#beginDate").datepicker('setDate', date);
				refreshGraphs();
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

			function displayCheckboxes(fileNames)
			{	
				var checkboxList = document.getElementById("checkboxList");

				var createCheckbox = function(idx)
				{	
					var fileName = fileNames[idx];
					var color = colors[idx];

					var cb = document.createElement('input');
					cb.type = "checkbox";
					cb.name = fileName;
					cb.value = "value";



					var label = document.createElement('label')
					label.htmlFor = "id";
					label.appendChild(document.createTextNode(fileName.substr(0, fileName.length-4)));
					label.style.color = color;

					label.onclick = function()
					{
						cb.checked = !cb.checked;
						cb.onchange();
					};

					cb.onchange = function()
					{
						if (cb.checked == true || label.onClick == true)
						{
							selectedPlots.push({ fileName: fileName, color: color });
						}
						else
						{
							for (var i = 0; i < selectedPlots.length; i++)
							{
								if (selectedPlots[i].fileName == fileName)
								{
									selectedPlots.splice(i, 1);
									break;
								}
							}
						}
						
						refreshGraphs();
					};

					var item = document.createElement('li');
					item.className = "itemList";

					item.appendChild(cb);
					item.appendChild(label);

					checkboxList.appendChild(item);
				};

				for (var idx = 0; idx < fileNames.length; idx++)
				{	
					createCheckbox(idx);
				}
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
					plotGraph(graphData, selectedPlots, beginDate, endDate, filter)
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

			/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			graphData = createGraphs();
			displayCheckboxes(sensorFiles);
			refreshGraphs();

			</script>

					</div>
				</div>




</body>
</html>
