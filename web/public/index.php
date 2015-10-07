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
	    stroke: red;
	    stroke-width: 1.5;
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
	    width: 900px;
	    height:900px;
	    margin-top: 40px;
		margin-left: 40px;
    	margin-right: 40px;
    
	}
	.itemList{
		font-size: 14px;
		margin-top: 4px;
		margin-bottom: 4px;
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


<script>

$('#beginDate')
	.datepicker({
		//showButtonPanel: true,
		dateFormat: "dd/mm/yy",
		defaultDate: -12,
		onSelect: function(date) {
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
		onSelect: function(date) {
			refreshGraphs();

	    }
	})
	.datepicker('setDate', new Date());



var colors = ["#7A0000", "#0074D9", "#39CCCC", "#3D9970", "#2ECC40", "#3D0000", "#FF4136", "#854144b", "#FF3399", "#AAAAAA", "#B10DC9"];
var fileArray = ["living.tsv", "hall.tsv", "bedroom.tsv", "bedroom.tsv", "bedroom.tsv", "bedroom.tsv", "bedroom.tsv", "bedroom.tsv", "bedroom.tsv", "bedroom.tsv", "bedroom.tsv"];

	// Set the dimensions of the canvas / graph
var margin = {top: 30, right: 20, bottom: 30, left: 50};
var width = 900 - margin.left - margin.right;
var height = 300 - margin.top - margin.bottom;

// Set the ranges
var x = d3.time.scale().range([0, width]);
var y0 = d3.scale.linear().range([height, 0]);
var y1 = d3.scale.linear().range([height, 0]);

// Define the axes
var xAxis = d3.svg.axis().scale(x)
    .orient("bottom").ticks(width/100).tickFormat(d3.time.format('%d %b'));

var y0Axis = d3.svg.axis().scale(y0)
    .orient("left").ticks(5).tickFormat(function(d) { return d + "°C"; });
var y1Axis = d3.svg.axis().scale(y1)
    .orient("right").ticks(5).tickFormat(function(d) { return d + "%"; });

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
	.attr("class", "y axis")
	.call(y0Axis);
svg.append("g")
	.attr("class", "y axis")
	.call(y1Axis);

var plotData = {
	x: x,
	y0: y0,
	y1:y1,
	graph: svg,
	xAxis: xAxis,
	y0Axis: y0Axis,
	y1Axis:y1Axis,
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
	var parser = d3.time.format("%d/%m/%Y").parse;
	
	var beginDate = $("#beginDate").datepicker('getDate'); 
	var endDate = $("#endDate").datepicker('getDate'); 

	d3.selectAll('.line').remove();

	if (beginDate != null && endDate != null && beginDate.getTime() < endDate.getTime())
	{
		plotGraph(plotData, selectedObjects, beginDate, endDate)
	}
}



displayCheckboxes(fileArray);
refreshGraphs();
</script>
</body>
</html>