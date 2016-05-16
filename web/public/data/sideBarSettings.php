<div id="ContentLeft">
	<nav class ="SideMenu">
		<p>Settings</p><br>

		<a href="details.php" style="text-decoration: none">My details<br>
		<a href="devicelist.php" style="text-decoration: none">Device list</a><br>
		<a href="addgateway.php" style="text-decoration: none">Add gateway</a><br>
		<a href="createAlarms.php" style="text-decoration: none">Create alarms</a>
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
</div>
