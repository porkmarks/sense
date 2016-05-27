<div id="TopHeader">
	<nav id = "LeftNav">
	<ol>
		<li>
			<h3><a href="logout.php" style="text-decoration: none">Logout</a></h3>
		</li>

		<li>
			<h3>
				<?php 
					echo"Hello, " . $_SESSION["name"] . ", nice to see you!";
				?>
			</h3>
		</li>
		
	</ol>
	</nav>
</div>

<div id="LogoDiv"></div>

<div id="NavBar">
	<nav id="MainNav">
		<ul>
			<li><a href = "overview.php" style="text-decoration: none">Data</a></li>
			<li><a href = "settings.php" style="text-decoration: none">Settings</a></li>
		</ul>
		<script>
		$(function(){
				$('a').each(function() {
				if ($(this).prop('href') == window.location.href) {
 			      $(this).addClass('current');
                  }
				});
		});
	</script>
	</nav>
</div>

