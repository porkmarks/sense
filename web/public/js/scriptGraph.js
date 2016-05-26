

function plotGraph(graphData, fileDatas, beginDate, endDate, what)
{
	var parseDate = d3.time.format("%Y-%m-%d %H:%M:%S").parse;

	// Define the line
	var valuelineTemp = d3.svg.line()
	    .x(function(d) { return graphData.xRange(d.date); })
	    .y(function(d) { return graphData.y0Range(d.temperature); });

	var valuelineHum = d3.svg.line()
	    .x(function(d) { return graphData.xRange(d.date); })
	    .y(function(d) { return graphData.y1Range(d.humidity); });
	       

	var readData = function(plotIdx)
	{
		d3.tsv(fileDatas[plotIdx].fileName, function(data) 
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
		    	//console.log(data[i].date);
		    	if (data[i].date.getTime() >= beginDate.getTime() && data[i].date.getTime() <= endDate.getTime())
		    	{
		    		filteredData.push(data[i]);
		    	}

		    }
	    	

			graphData.xRange.domain(d3.extent(filteredData, function(d) { return d.date; }));
			if (what == 1 || what == 3)
			{
				graphData.y0Range.domain(d3.extent(filteredData, function(d) { return d.temperature; }));
			    graphData.graph.append("path")
					.attr("class", "line")
					.attr("stroke", fileDatas[plotIdx].color)
					//.style("stroke-dasharray", ("10,2"))valueline
					.attr("d", valuelineTemp(filteredData));
			}

			if (what == 2 || what == 3)
			{
				graphData.y1Range.domain(d3.extent(filteredData, function(d) { return d.humidity; }));
				graphData.graph.append("path")
					.attr("class", "line")
					.attr("stroke", fileDatas[plotIdx].color)
					.style("stroke-dasharray", ("3,3"))
					.attr("d", valuelineHum(filteredData));
			}
			
			// Add the valueline path.
			
			//console.log(plotIdx);

		    var t = graphData.graph.transition().duration(100);
		    //var y = t.selectAll(".y.axis");
			
		    t.select(".x.axis").call(graphData.xAxis);
		    t.select(".y.axisLeft").call(graphData.yAxisLeft);
		    t.select(".y.axisRight").call(graphData.yAxisRight);
		});
	};

	// Get the data
	for(var i = 0; i < fileDatas.length; i++)
	{
		readData(i);
	}



}

