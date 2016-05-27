<div id="ContentLeft">
	<nav class ="SideMenu">
		<p>Settings</p><br>

		<a href="settings.php" style="text-decoration: none">My Details<br>
		<a href="sensorSettings.php" style="text-decoration: none">Sensor Settings</a><br>
		<a href="addSensor.php" style="text-decoration: none">Add Sensor</a><br>
		<a href="alarms.php" style="text-decoration: none">Alarms</a>
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
