<?php 
if (isset($_GET['logout']))
 {
   session_destroy();
   echo "<br> you are logged out successufuly!";
} 
   echo "<br/><a href='login.php'>login</a>";
 ?>