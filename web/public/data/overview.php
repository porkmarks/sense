<?php
	session_start();
	include('checklogin.php');
?>

<!DOCTYPE html>
<head><title>Overview</title></head>
<?php
	include('includes.php');
?>
<link href= "../css/overview.css" rel="stylesheet" type ="text/css"/>



<body>

	<div id="Holder">
		<?php
			include('topBar.php');
		?>
	</div>

	<div id="Content">
		<?php
			include('sideBar.php');
		?>
		<div id="ContentRight">
			<h2>Overview</h2>
			<div class="alert warning">
				<p class ="subtler">
				You have new
				<a href="alarms.php">alarm notifications</a>
				</p>
		</div>
	</div>

</body>
</html>