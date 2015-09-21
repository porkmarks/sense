<!DOCTYPE html>
<meta charset="utf-8">
<head><title>GRAFICE</title>
<script src="http://d3js.org/d3.v3.min.js" charset="utf-8"></script>
<link rel="stylesheet" href="//code.jquery.com/ui/1.11.4/themes/smoothness/jquery-ui.css">
<script src="//code.jquery.com/jquery-1.10.2.js"></script>
<script src="//code.jquery.com/ui/1.11.4/jquery-ui.js"></script>
<script src="scriptGraph.js"></script>
<script>
function refreshGraphs()
{
	var parser = d3.time.format("%d/%m/%Y").parse;
	var fileArray = ["data.tsv", "data1.tsv", "data2.tsv"];
	var beginDate = $("#beginDate").datepicker('getDate'); 
	var endDate = $("#endDate").datepicker('getDate'); 

	if (beginDate != null && endDate != null && beginDate.getTime() < endDate.getTime())
	{
		plotGraph(fileArray, beginDate, endDate)
	}
}
</script>
<style>
	body { font: 14px Arial;}

	path { 
	    stroke: steelblue;
	    stroke-width: 3;
	    fill: none;
	}

	.axis path,
	.axis line {
	    fill: none;
	    stroke: red;
	    stroke-width: 2;
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

refreshGraphs();

</script>
<div style="float:right; margin:10px">
<script>

</script>
</div>
</body>
</html>