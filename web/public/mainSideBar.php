	<nav class ="SideMenu">

		<a href="overview.php" style="text-decoration: none">Overview</a><br>
		<a href="graphs.php" style="text-decoration: none">Graphs</a><br>
		<a href="analytics.php" style="text-decoration: none">Analytics</a><br>
		<a href="alarms.php" style="text-decoration: none">Alarms</a></nav>
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
	</nav>
