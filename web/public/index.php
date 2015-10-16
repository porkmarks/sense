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
	body { font: 14px Arial; width 1600px; margin: 0 auto;}

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
		float:left;
		width: 900px;
		height: 200px;
		margin-left: 40px;
    	margin-right: 40px;
    	margin-top: 20px;
    	margin-bottom: 20px; 
	}

	.checkboxList {
    	list-style-type:none;
	}

	.checkboxListScroll {
		float: left;
		position:relative;
		margin-left: 40px;
    	margin-right: 40px;
    	margin-top: 20px;
    	margin-bottom: 20px; 
    	width: 200px;
    	height:200px;
    	overflow: scroll;
    	overflow-x: hidden;
	}

	#graphContainer {
	    float: left;
	    width: 1200px;
	    height:350px;
	    margin-top: 40px;
		margin-left: 40px;
    	margin-right: 40px;
    
	}
	.itemList{
		font-size: 14px;
		margin-top: 4px;
		margin-bottom: 4px;
	}
	#checkboxes{
		float: left;
	    width: 500px;
	    height:30px;
	    margin-top: 20px;
		margin-left: 40px;
    	margin-right: 40px;
	}
	#option{
		float: left;
		width: 600px;
		height: 30px;
		margin-left: 60px;
    	margin-right: 20px;
    	margin-top: 10px;
    	margin-bottom: 20px; 
	}
</style>
</head>
<body>

<div id="datePkr">
Begin Date: <input type="text" id="beginDate">
End Date: <input type="text" id="endDate">
</div>

<div class="checkboxListScroll">
<ul id="checkboxList" class="checkboxList"> </ul>
</div>
<div id ="graphContainer"></div>

<div id="option">
    <input name="DayButton" 
           type="button" 
           value="Day" 
           onclick="updateData()" />
    <input name="weekButton" 
           type="button" 
           value="Week" 
           onclick="updateData()" />

    <input name="MonthButton" 
           type="button" 
           value="Month" 
           onclick="updateData()" />

    <input name="SemesterButton" 
           type="button" 
           value="Semester" 
           onclick="updateData()" />

    <input name="YearButton" 
           type="button" 
           value="Year" 
           onclick="updateData()" />
</div>

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



var checkbTemp = document.getElementById("temp");
var checkbHum = document.getElementById("hum");

checkbTemp.onchange = function()
{
	refreshGraphs();
};
checkbHum.onchange = function()
{
	refreshGraphs();
};

var colors = ["#7A0000", "#0074D9", "#39CCCC", "#3D9970", "#2ECC40", "#3D0000", "#FF4136", "#854144b", "#FF3399", "#AAAAAA", "#B10DC9"];
var fileArray = ["living.tsv", "hall.tsv", "bedroom.tsv", "bathroom.tsv", "kitchen.tsv" ];

	// Set the dimensions of the canvas / graph
var margin = {top: 30, right: 60, bottom: 30, left: 60};
var width = 900 - margin.left - margin.right;
var height = 300 - margin.top - margin.bottom;

// Set the ranges
var x = d3.time.scale().range([0, width]);
var y0 = d3.scale.linear().range([height, 0]);
var y1 = d3.scale.linear().range([height, 0]);

// Define the axes
var xAxis = d3.svg.axis().scale(x)
    .orient("bottom").ticks(width/100).tickFormat(d3.time.format('%d %b %H:%M'));

var formatter = d3.format(".1f")
var yAxisLeft = d3.svg.axis().scale(y0)
    .orient("left").ticks(5).tickFormat(function(d) { return formatter(d) + "°C"; });
var yAxisRight = d3.svg.axis().scale(y1)
    .orient("right").ticks(5).tickFormat(function(d) { return formatter(d) + "%"; });

// Adds the svg canvas
var svg = d3.select("#graphContainer")
			.append("svg")
			.attr("width", width + margin.left + margin.right)
			.attr("height", height + margin.top + margin.bottom)
			.append("g")
			.attr("transform", "translate(" + margin.left + "," + margin.top + ")");
svg.append("g")
	.attr("class", "x axis")
	.attr("transform", "translate(0," + height + ")")
	.call(xAxis);

// Add the Y Axis
svg.append("g")
	.attr("class", "y axis axisLeft")
	.call(yAxisLeft);
svg.append("g")
	.attr("class", "y axis axisRight")
	.attr("transform", "translate(" + width + " ,0)")
	.call(yAxisRight);

var plotData = {
	x: x,
	y0: y0,
	y1: y1,
	graph: svg,
	xAxis: xAxis,
	yAxisLeft: yAxisLeft,
	yAxisRight: yAxisRight,
	width: width,
	height: height
}
var selectedObjects = [];


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
				selectedObjects.push({ fileName: fileName, color: color });
			}
			else
			{
				for (var i = 0; i < selectedObjects.length; i++)
				{
					if (selectedObjects[i].fileName == fileName)
					{
						selectedObjects.splice(i, 1);
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
		plotData.graph.select(".y.axisLeft").attr("visibility","visible");
	}
	else
	{
		plotData.graph.select(".y.axisLeft").attr("visibility","hidden");
	}
	if (checkbHum.checked == true)
	{
		filter += 2;
		plotData.graph.select(".y.axisRight").attr("visibility","visible");
	}
	else
	{
		plotData.graph.select(".y.axisRight").attr("visibility","hidden");
	}

	var parser = d3.time.format("%d/%m/%Y").parse;
	
	var beginDate = $("#beginDate").datepicker('getDate'); 
	var endDate = $("#endDate").datepicker('getDate'); 
	var min = endDate.getMinutes();
	endDate.setMinutes(min + 1439);

	d3.selectAll('.line').remove();

	if (beginDate != null && endDate != null && beginDate.getTime() < endDate.getTime())
	{
		plotGraph(plotData, selectedObjects, beginDate, endDate, filter)
	}
}

displayCheckboxes(fileArray);
refreshGraphs();
</script>

</body>
</html>