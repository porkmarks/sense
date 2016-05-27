<?php
	session_start();
	include('checklogin.php');
?>

<!DOCTYPE html>
<head><title>Alarms</title></head>
<?php
	include('includes.php');
?>


<body>

	<div id="Holder">
		<?php
			include('mainTopBar.php');
		?>
	</div>

	<div id="Content">
		<?php
			include('mainSideBar.php');
		?>
		<div id="ContentRight">
			alarms!!!!!
		</div>
	</div>

</body>
</html>
