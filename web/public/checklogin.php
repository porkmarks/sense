<?php

session_start();

if (isset($_SESSION['fname']) == false)
{
    header("Location:login.php?error=Please login");
}

?>
