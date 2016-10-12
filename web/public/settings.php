<?php
	session_start();
	include('checklogin.php');
?>
<!DOCTYPE html>
<head><title>My Details</title></head>
<link href= "css/details.css" rel="stylesheet" type ="text/css"/>
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
			<div class="right">
    <section>
  <form id='details' action='changeDetails.php' method='POST' autocomplete='off'>
    <table class="mydetails">
      
      <h4>Your basic personal details.</h4>
      
        <tr>
	        <td valign="top">
				<label for='fname' class='mandatory'>First name</label>
			</td>
			<td valign="top">
				<input type='text' id='fname' name='fname' value='<?php echo $_SESSION["fname"] ?>' class='mandatory' />
			</td>
		</tr>

		
      	<tr>
	        <td valign="top">
				<label for='lname' class='mandatory'>Last name</label>
			</td>
			<td valign="top">
				<input type='text' id='lname' name='lname' value='<?php echo $_SESSION["lname"] ?>' class='mandatory' />
			</td>
		</tr>

		<tr>
			<td valign="top">
				<label for='company' class='mandatory'>Company</label>
			</td>
			<td valign="top">
				<input type='text' id='company' name='company' value='<?php echo $_SESSION["company"] ?>'/>
			</td>
		</tr>

      
      
	     <tr> 
	        <td valign="top">
				<label for='email' class='mandatory'>Email</label>
			</td>
			<td valign="top">
				<input type='email' id='email' name='email' value="<?php echo $_SESSION["email"] ?>" class='mandatory' />
			</td>
		</tr>
      
        <tr>
			<td valign="top">
				<label for='country_id' class='mandatory'>Country</label>
			</td>
			<td valign="top">

			<?php
				include('countriesList.php');
			?>
			</td>
		</tr>
      
        <tr>
        	<td valign="top">
				<label for='tz_id' class='mandatory'>Time zone</label>
			</td>
			<td valign="top">	
				<?php
					include('timezoneList.php');
				?>
			</td>
		</tr>	




     </table> 
    
     <h4>Units</h4>
 
     <h3>Measurement units.</h3>
     <table>
     <tr>
		<td valign="top">
			<label for='temp_unit_id' class='mandatory'>Temperature</label>
		</td>
			<td valign="top">
		<div>
			<select id='temp_unit_id' name='temp_unit_id' class='mandatory'>
				<option value='1' selected='selected'>Celsius (°C)</option>
				<option value='2'>Fahrenheit (°F)</option>
			</select>
		</div>
	</table> 	

    	
     
    <h4>Password</h4>
    <h3>Here you can change your password</h3>
    <table>
    <tr>
	    <td valign="top">
	    	<label for='password'>Password</label>
	    </td>
	    <td valign="top">	
	    	<input type='password' id='password' name='password' value='' />
	    </td>
    </tr>
    <tr>
	    <td valign="top">
	    	<label for='re_password'>Repeat password</label>
	    </td>
	    <td valign="top">	
	    	<input type='password' id='repassword' name='repassword' value='' />
	    </td>
	</tr>    
	<tr>
	    <td valign="top">	
	    	<input type='submit' id='submit' name='submit' value='Save' />
	    </td>
    </tr>
    </table>
     <h5>
			<?php
				$error = $_GET["error"];
				if (isset($error))
				{
					echo $error;
				}

			?>
	</h5>
  </form>
</section>
</body>
</html>
