<?php
	include('checklogin.php');
?>
<?php
session_start();
if (!isset($_SESSION["name"]))
{
	header('Location:login.php?error=Please login"');
}
?>
<!DOCTYPE html>
<meta charset="utf-8">
<head><title>index</title></head>
<link rel="stylesheet" href="//code.jquery.com/ui/1.11.4/themes/smoothness/jquery-ui.css">
<link href= "../css/main.css" rel="stylesheet" type ="text/css">
<script src="//code.jquery.com/jquery-1.10.2.js"></script>
<script src="//code.jquery.com/ui/1.11.4/jquery-ui.js"></script>


<body>
<div id="Holder">
<div id="TopHeader">
<nav id = "LeftNav">
<ol>
	<li>
		<h3><a href="http://localhost/sense/data/logout.php?logout=1" style="text-decoration: none">Logout</a></h3>
	</li>

	<li>
		<h3>
			<?php 
				echo"Hello, " . $_SESSION["name"] . ", nice to see you!";
			?>
		</h3>
	</li>
	
</ol>
</nav>
</div>

	

<div id="LogoDiv"></div>
<div id="NavBar">
	<nav id="MainNav">
		<ul>
			<li><a href = "http://localhost/sense/data/index.php" style="text-decoration: none">Data</a></li>
			<li><a href = "http://localhost/sense/settings/details.html" style="text-decoration: none">Settings</a></li>
		</ul>

	</nav>
</div>
<div id="Content">
	<div id="ContentLeft">
		<nav class ="SideMenu">
			<p>Data</p><br>

			<a href="http://localhost/sense/data/index.php" style="text-decoration: none">Overview</a><br>
			<a href="http://localhost/sense/data/graphs.php" style="text-decoration: none">Graphs</a><br>
			<a href="http://localhost/sense/data/analytics.html" style="text-decoration: none">Analytics</a><br>
			<a href="http://localhost/sense/data/alarms.html" style="text-decoration: none">Alarms</a></nav>
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
		
	<div id="ContentRight">

	</div>
</div>


</body>