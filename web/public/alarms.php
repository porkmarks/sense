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
	<header>
		<?php
			include('mainTopBar.php');
		?>
    </header>

<div id="Content">
		<?php
			include('settingsSideBar.php');
		?>
		<div id="ContentRight">
			Alarms!!!!!
		</div>
	</div>


</body>
</html>
