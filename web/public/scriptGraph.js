

function plotGraph(fileNames, beginDate, endDate)
{
	var parseDate = d3.time.format("%d/%m/%Y").parse;

	// Set the dimensions of the canvas / graph
	var margin = {top: 30, right: 20, bottom: 30, left: 50},
	    width = 600 - margin.left - margin.right,
	    height = 270 - margin.top - margin.bottom;

	// Parse the date / time
	var parseDate = d3.time.format("%d/%m/%Y").parse;

	// Set the ranges
	var x = d3.time.scale().range([0, width]);
	var y = d3.scale.linear().range([height, 0]);

	// Define the axes
	var xAxis = d3.svg.axis().scale(x)
	    .orient("bottom").ticks(d3.time.days).tickFormat(d3.time.format('%d %b'));

	var yAxis = d3.svg.axis().scale(y)
	    .orient("left").ticks(5).tickFormat(function(d) { return d + "°C"; });

	// Define the line
	var valueline = d3.svg.line()
	    .x(function(d) { return x(d.date); })
	    .y(function(d) { return y(d.temperature); });
	    
	// Adds the svg canvas
	var svg = d3.select("body")
				.append("svg")
				.attr("width", width + margin.left + margin.right)
				.attr("height", height + margin.top + margin.bottom)
				.append("g")
				.attr("transform", "translate(" + margin.left + "," + margin.top + ")");

	
	// Get the data
	for(var i = 0; i < fileNames.length; i++)
	{
		d3.tsv(fileNames[i], function(data) 
		{	
			//parse data into correct type
		    data.forEach(function(d) 
		    {
		        d.idx = +d.idx;
		        d.date = parseDate(d.date);
		        d.temperature = +d.temperature;
		        d.humidity = +d.humidity;
		        
		    });
			//data filter for indice

		    var filteredData = [];
		    for (var i = 0; i < data.length; i++)
		    {
		    	console.log(data[i].date);
		    	if (data[i].date.getTime() >= beginDate.getTime() && data[i].date.getTime() <= endDate.getTime())
		    	{
		    		filteredData.push(data[i]);
		    	}
		    }
	    	
			x.domain(d3.extent(filteredData, function(d) { return d.date; }));
			y.domain([0, d3.max(filteredData, function(d) { return d.temperature; })]);
			// Add the valueline path.
			svg.append("path")
				.attr("class", "line")
				.attr("d", valueline(filteredData));
			// Add the X Axis

			    var t = svg.transition().duration(100);
						    t.select(".y.axis").call(yAxis);
						    t.select(".x.axis").call(xAxis);
						    t.select(".area").attr("d", area(values));
						    t.select(".line").attr("d", line(values));


		    
		});
	}

	svg.append("g")
		.attr("class", "x axis")
		.attr("transform", "translate(0," + height + ")")
		.call(xAxis);

	// Add the Y Axis
	svg.append("g")
	.attr("class", "y axis")
	.call(yAxis);




    

	
}