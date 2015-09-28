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
	body { font: 14px Arial;}

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
</style>
</head>
<body>

<div style="float:left","margin: 20px">Begin Date: <input type="text" id="beginDate"></div>
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

</script>
<div style="float:left","margin: 30px">End Date: <input type="text" id="endDate"></div>
<script>
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



</script>


<script>
	// Set the dimensions of the canvas / graph
var margin = {top: 30, right: 20, bottom: 30, left: 50};
var width = 800 - margin.left - margin.right;
var height = 300 - margin.top - margin.bottom;

// Set the ranges
var x = d3.time.scale().range([0, width]);
var y = d3.scale.linear().range([height, 0]);

// Define the axes
var xAxis = d3.svg.axis().scale(x)
    .orient("bottom").ticks(d3.time.days).tickFormat(d3.time.format('%d %b'));

var yAxis = d3.svg.axis().scale(y)
    .orient("left").ticks(5).tickFormat(function(d) { return d + "°C"; });
	// Adds the svg canvas
var svg = d3.select("body")
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
	.call(yAxis);

var plotData = {
	x: x,
	y: y,
	graph: svg,
	xAxis: xAxis,
	yAxis: yAxis,
	width: width,
	height: height
}


function refreshGraphs()
{
	var parser = d3.time.format("%d/%m/%Y").parse;
	var fileArray = ["living.tsv", "hall.tsv", "bedroom.tsv"];
	var beginDate = $("#beginDate").datepicker('getDate'); 
	var endDate = $("#endDate").datepicker('getDate'); 

	d3.selectAll('.line').remove();

	if (beginDate != null && endDate != null && beginDate.getTime() < endDate.getTime())
	{
		plotGraph( plotData, fileArray, beginDate, endDate)
	}
}

refreshGraphs();

</script>
<li>
<label><input type="checkbox" name="checkbox" value="value">Hall</label>
<label><input type="checkbox" name="checkbox" value="value">Bedroom</label>
<label><input type="checkbox" name="checkbox" value="value">Living</label>

<button type='button' onclick='checkAll()'>Check</button>
<button type='button' onclick='uncheckAll()'>Uncheck</button>
</li>
<script>
function checkAll() {
    d3.selectAll('input').property('checked', true);
}
function uncheckAll() {
    d3.selectAll('input').property('checked', false);
}
</script>

<div style="float:right; margin:10px">
</div>
</body>
</html>