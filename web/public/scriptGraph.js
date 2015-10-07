

function plotGraph(plotData, fileDatas, beginDate, endDate)
{
	var parseDate = d3.time.format("%Y-%m-%d").parse;

	// Define the line
	var valuelineTemp = d3.svg.line()
	    .x(function(d) { return plotData.x(d.date); })
	    .y(function(d) { return plotData.y0(d.temperature); });

	var valuelineHum = d3.svg.line()
	    .x(function(d) { return plotData.x(d.date); })
	    .y(function(d) { return plotData.y1(d.humidity); });
	       

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
		        d.symbol  = fileDatas[plotIdx].fileName;
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
			plotData.y0.domain(d3.extent(filteredData, function(d) { return d.temperature; }));
			plotData.y1.domain(d3.extent(filteredData, function(d) { return d.humidity; }));
			// Add the valueline path.
			
			//console.log(plotIdx);

		    var t = plotData.graph.transition().duration(100);
		    t.select(".y.axis").call(plotData.y0Axis);
		    t.select(".x.axis").call(plotData.xAxis);

		    var s = plotData.graph.transition().duration(100);
		    s.select(".y.axis").call(plotData.y1Axis);
		    s.select(".x.axis").call(plotData.xAxis);
		
			
			
  			
		    plotData.graph.append("path")
				.attr("class", "line")
				.attr("stroke", fileDatas[plotIdx].color)
				//.style("stroke-dasharray", ("10,2"))valueline
				.attr("d", valuelineTemp(filteredData));

			 plotData.graph.append("path")
				.attr("class", "line")
				.attr("stroke", fileDatas[plotIdx].color)
				.style("stroke-dasharray", ("3,3"))
				.attr("d", valuelineHum(filteredData));
						    
		});
	};

	// Get the data
	for(var i = 0; i < fileDatas.length; i++)
	{
		readData(i);
	}



}

