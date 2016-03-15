<?php

session_start();

if ($_SESSION['loggedIn'] != "true") {

    header("Location: http://localhost/sense/data/login.php");

}
?>
