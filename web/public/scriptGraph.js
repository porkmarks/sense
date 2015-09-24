

function plotGraph(plotData, fileNames, beginDate, endDate)
{
	var parseDate = d3.time.format("%Y-%m-%d").parse;

	// Define the line
	var valueline = d3.svg.line()
	    .x(function(d) { return plotData.x(d.date); })
	    .y(function(d) { return plotData.y(d.temperature); });
	    


	var readData = function(plotIdx)
	{
		d3.tsv(fileNames[plotIdx], function(data) 
		{	
			//parse data into correct type
		    data.forEach(function(d) 
		    {
		        d.idx = +d.idx;
		        d.date = parseDate(d.date);
		        d.temperature = +d.temperature;
		        d.humidity = +d.humidity;
		        d.symbol  = fileNames[i];
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
	    	

			plotData.x.domain(d3.extent(filteredData, function(d) { return d.date; }));
			plotData.y.domain(d3.extent(filteredData, function(d) { return d.temperature; }));
			// Add the valueline path.
			
			//console.log(plotIdx);

		    var t = plotData.graph.transition().duration(100);
		    t.select(".y.axis").call(plotData.yAxis);
		    t.select(".x.axis").call(plotData.xAxis);
		
			
			var colors = ["#ff9900", "#109618", "#990099", "#0099c6", "#dd4477", "#66aa00", "#b82e2e", "#316395", "#994499", "#22aa99", "#aaaa11", "#6633cc", "#e67300", "#8b0707", "#651067", "#329262", "#5574a6", "#3b3eac"];
  			
		    plotData.graph.append("path")
				.attr("class", "line")
				.attr("stroke", colors[plotIdx])
				//.style("stroke-dasharray", ("10,2"))
				.attr("d", valueline(filteredData));
					    
		});
	};

	// Get the data
	for(var i = 0; i < fileNames.length; i++)
	{
		readData(i);
	}



}

