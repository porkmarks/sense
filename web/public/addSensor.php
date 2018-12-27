<?php
	session_start();
	include('checklogin.php');
?>
<!DOCTYPE html>
<head><title>Add Gateway</title></head>
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
	    <div id ="ContentLeft">
    	    <?php
        		  include('settingsSideBar.php');
        	?>
    </div>
    	<div id="ContentRight">
			Add Sensor!!!!!
		</div>
	</div>


</body>
</html>
