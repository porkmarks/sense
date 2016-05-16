<?php
	session_start();
	include('../data/checklogin.php');
?>
<!DOCTYPE html><head><title>My Details</title></head>
<?php
	
	include('../data/includes.php');
?>

<body>
	<header>
		<?php
			include('../data/topBar.php');
		?>
    </header>

<div id="Content">
		<?php
			include('../data/sideBarSettings.php');
		?>
		<div id="ContentRight">
			DETALII!!!!!
		</div>
	</div>


</body>
</html>