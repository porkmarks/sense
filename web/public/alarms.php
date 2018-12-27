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
		
	<div id="Holder">
		<?php
			include('mainTopBar.php');
		?>
	</div>
    </header>


<div id="Content">
     <div id ="ContentLeft">
    	<?php
    		include('mainSideBar.php');
    	?>
    </div>
	<div id="ContentRight">
			Alarms!!!!!
	</div>
</div>


</body>
</html>
