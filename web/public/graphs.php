<!DOCTYPE html>
<meta charset="utf-8">
<head><title>GRAFICE</title>
<script src="http://d3js.org/d3.v3.min.js" charset="utf-8"></script>
<link rel="stylesheet" href="//code.jquery.com/ui/1.11.4/themes/smoothness/jquery-ui.css">
<script src="//code.jquery.com/jquery-1.10.2.js"></script>
<script src="//code.jquery.com/ui/1.11.4/jquery-ui.js"></script>
<script src="scriptGraph.js"></script>
<script>

</script>
<style>
@import url(//fonts.googleapis.com/css?family=Open+Sans);
header {
    color:grey;
    text-align:left;
    padding:5px; 
    font-family: 'Open Sans', sans-serif;
    margin-left: 10px;
}
nav {
    line-height:30px;
    background-color:#eeeeee;
    margin-left: 10px;
    height:200px;
    width:80px;
    float:left;
    padding:5px; 
    font-family: 'Open Sans', sans-serif;
}
a:link {
    color: tomato;
}

a:visited {
    color: tomato;
}
nav a.current {
  
  color:steelblue;
}
section {
    width:400px;
    float:left;
    padding:10px; 
    font-family: 'Open Sans', sans-serif;
}
body { 
	font-family: 'Open Sans', sans-serif;
	width: 100%; 
	margin: 0 auto;
}

path {
    stroke-width: 1.5;
    fill: none;
}

.axis path,
.axis line {
    fill: none;
    stroke: grey;
    stroke-width: 1.2;
    shape-rendering: crispEdges;
}

#datePkr {
	position:fixed; top:75px; left: 110px;
	width: 1000px;
	height: 60px;
	margin-left: 20px;
	margin-right: 20px;
	margin-top: 20px;
	margin-bottom: 20px; 
	font-family: 'Open Sans', sans-serif;
}

.checkboxList {
	list-style-type:none;

}

.checkboxListScroll {
	float: right;
	position:relative;
	margin-left: 40px;
	margin-right: 80px;
	margin-top: 20px;
	margin-bottom: 20px; 
	width: 200px;
	height:800px;
	overflow: scroll;
	overflow-x: hidden;
	font-family: 'Open Sans', sans-serif;
}

#graphContainer {
    float: left;
    width: auto;
    height:340px;
    margin-top: 100px;
	margin-left: 20px;
	margin-right: 20px;
	font-family: 'Open Sans', sans-serif;

}
.itemList{
	font-size: 12px;
	margin-top: 4px;
	margin-bottom: 4px;
	font-family: 'Open Sans', sans-serif;
}
#checkboxes{
	font-family: 'Open Sans', sans-serif;
	float: left;
    width: 500px;
    height:30px;
    margin-top: 20px;
	margin-left: 40px;
	margin-right: 40px;
	font-family: 'Open Sans', sans-serif;
}
#option{
	position:fixed; top:75px; left: 700px;
	width: 600px;
	height: 30px;
	margin-left: 20px;
	margin-right: 20px;
	margin-top: 20px;
	margin-bottom: 20px;
	font-family: 'Open Sans', sans-serif;
}
#temp{
	font-size: 9px; 
	font-family: 'Open Sans', sans-serif;
}
#hum{
	font-family: 'Open Sans', sans-serif; 
	ont-size: 9px;
}
</style>
<header>
<h1><a href="http://localhost/sense/index.php"style="text-decoration: none">Sense</a></h1>
</header>
<body>
<nav>
Data<br>
<br>
<a href="http://localhost/sense/index.php" style="text-decoration: none">Overview</a><br>
<a href="http://localhost/sense/graphs.php" style="text-decoration: none">Graphs</a><br>
<a href="http://localhost/sense/analytics.html" style="text-decoration: none">Analytics</a><br>
<a href="http://localhost/sense/alarms.html" style="text-decoration: none">Alarms</a></nav>

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

<div id = "checkboxes"> 
    <label><input type="checkbox"  id = "temp" checked="true"> Temperature</label>
    <input type="checkbox" id = "hum" checked="true"> Humidity
</div>

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

var colors = ["#7A0000", "#0074D9", "#39CCCC", "#3D9970", "#2ECC40", "#3D0000", "#FF4136", "#854144b", "#FF3399", "#AAAAAA", "#B10DC9"];
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
	var graphWidth = windowWidth- 500- margin.left - margin.right;
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

</body>
</html>