<?php

session_start();

if (isset($_SESSION['name']) == false)
{
    header("Location:login.php?error=Please login");
}

?>
