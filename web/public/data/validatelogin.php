
<?php
include("db.php");

if (isset($_POST["submit"]))
{
	$db = mysqli_connect(DB_SERVER,DB_USERNAME,DB_PASSWORD,DB_DATABASE);
	if (mysqli_connect_errno())
	{
		echo "Failed to connect to MySQL: " . mysqli_connect_error();
	}

	$email = $_POST["email"];
	$password = md5($_POST["password"]);
	
	$sql = "SELECT * FROM Users WHERE email='$email' AND password = '$password';";
	$result = mysqli_query($db, $sql);
	$row = mysqli_fetch_array($result, MYSQLI_ASSOC);

	
	if (mysqli_num_rows($result) == 1)
	{
		session_start();
		$_SESSION["name"] = $row["name"];
		$_SESSION["email"] = $row["email"];
		$_SESSION["sensorDB"] = $row["sensor_database"];

        $location = "Location: overview.php";
    }
    else
    {
        $location = 'Location: login.php?error=Sorry...Password/user combination not good..."';
    }

    mysqli_free_result($result);
    unset($result);

	mysqli_close($db);
    unset($db);

	header($location);
}

?>

