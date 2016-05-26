
<!DOCTYPE html>
<html lang="en">
<head>
<link href ="css/layout_register.css" rel="stylesheet" type = "text/css" />
<link href ="css/menu.css" rel="stylesheet" type = "text/css" />

	<meta http-equiv="content-type" content="text/html; charset=utf-8">

	<title>Tempr-Register</title>

</head>

<body>
<div id="Holder">
<div id="Header"></div>
<div id="NavBar">
	<nav>
		<ul>
			<li><a href="login.php">Login</a></li>
			<li><a href="register.php">Register</a></li>
			<li><a href="#">Forgot Password</a></li>
		</ul>

	</nav>
</div>
<div id="Content">
	<div id="ContentLeft">
		<h2></h2><br/>
		<br/>
		<h2>
			<?php
				$error = $_GET["error"];
				if (isset($error))
				{
					echo $error;
				}

			?>
		</h2>
	</div>
	<div id="ContentRight">
		<form action ="validateregister.php" method="post" id="RegisterForm">
		<fieldset>
			<table width="400" border="0" cellpadding="10" cellspacing="10">
			<tr>
				<td style="font-weight: bold"><div align="right"><label for="name">Name</label></div></td>
				<td><input name="name" type="text" class="StyleTxtField" size="25" required /></td>
			</tr>
			<tr>
				<td style="font-weight: bold"><div align="right"><label for="email">Email</label></div></td>
				<td><input name="email" type="email" class="StyleTxtField" size="25" required /></td>
			</tr>
			<tr>
				<td height="23" style="font-weight: bold"><div align="right"><label for="password">Password</label></div></td>
				<td><input name="password" type="password" class="StyleTxtField" size="25" required /></td>
			</tr>
			<tr>
				<td height="23" <div align="right"></td>
			    <td> <input type="submit" name="submit" value="Register!" class="StyleTxtField" />
               </td>
			</tr>
            </table>
		</fieldset>
		</form>
	</div>
</div>
<div id="Footer"></div>
</div>




</body>



</html>


