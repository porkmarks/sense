<?php
  session_start();
  include('../data/checklogin.php');
?>
<!DOCTYPE html><head><title>My Details</title></head>
<?php
  
  include('../data/includes.php');
?>

<body>
  <header>
    <?php
      include('../data/topBar.php');
    ?>
    </header>

<div id="Content">
    <?php
      include('../data/sideBarSettings.php');
    ?>
    <div id="ContentRight">
      <h2>Alarms</h2>

  <p class="subtle">Create a new alarm to get notificed when measurements
          passes the threshold value you specify.</p>

  <div class='row clear'>
      <div>
        <label for='alarmSensorList' class='mandatory'>Source</label>
      </div>
      <div>
        <select id='alarmSensorList' name='alarmSensorList' class='mandatory'>
        </select>
      </div>
  </div>


  <div class='row clear'>
    <div>
        <label for='direction' class='mandatory'>Direction</label>
    </div>
    <div>
      <select id='direction' name='direction' class='mandatory'>
        <option value='above'>Above</option>
        <option value='below'>Below</option>
      </select>
    </div>
  </div>

      
  <div class='row clear'>
    <div>
        <label for='value' class='mandatory number'>Value</label>
    </div>

    <div>
     <input type='text' id='value' name='value' value='' class='mandatory number'/>
    </div>
  </div>

      
  <p class="subtle">Optionally delay the alarm. E.g. enter "60" to trigger it only after one hour of measurements continuously exceeding the threshold.</p>

        
  <div class='row clear'>
    <div>
      <label for='delay' class='number'>For minutes</label>
    </div>
    <div>
      <input type='text' id='delay' name='delay' value='' class='number' />
    </div>
  </div>

      
  <div class="row right">
    <input type='submit' id='create' name='create' value='Create alarm'/>
  </div>

  <section>
  
    <h3>Alarms list</h3>
        
    <div class='alert info'>
      <p>These are your alarms.
      <a href='https://my.sensorist.com/data/alarms'>Click here</a> to view their notification history.
      </p>
    </div>

    
    <table>
      <thead>
        <tr>
          <th style='width: 50%;'>Source</th>
          <th style='width: 20%;'>Threshold</th>
          <th style='width: 10%;'>Delay</th>
          <th style='width: 10%;' class='center'>State</th>
          <th style='width: 10%;' class='center'>Delete</th>
        </tr>
      </thead>
    <tbody>
      <tr>
          <td>
            <span class='subtle'>Tempway:</span> Bathroom / Temperature
          </td>
          <td>Above 25 °C
          </td>
          <td>5
          </td>
          <td class='alarm out'>
          </td>

         <td class='center'>
             <input type='checkbox' id='alarms-1857' name='alarms[1857]' value='1' />
         </td>

      </tr>

      <tr>

        <td>
          <span class='subtle'>Tempway:</span> Kitchen / Temperature
        </td>
      
        <td>Above 25 °C</td>
        <td>5</td>
        <td class='alarm in'></td>

         <td class='center'>
             <input type='checkbox' id='alarms-1858' name='alarms[1858]' value='1' />

          </td>

      </tr>

      <tr>

        <td>
          <span class='subtle'>Tempway:</span> Office / Temperature
        </td>
      
        <td>Above 25 °C</td>
        <td>5</td>
        <td class='alarm out'></td>

        <td class='center'>
          <input type='checkbox' id='alarms-1859' name='alarms[1859]' value='1' />

        </td>

      </tr>

      <tr>

        <td>
          <span class='subtle'>Tempway:</span> Bedroom / Temperature
        </td>
      
        <td>Above 25 °C</td>
        <td>5</td>
        <td class='alarm out'></td>

        <td class='center'>
          <input type='checkbox' id='alarms-1860' name='alarms[1860]' value='1' />

        </td>

      </tr>

      <tr>
        <td>
          <span class='subtle'>Tempway:</span> Living / Temperature
        </td>
              
        <td>Above 25 °C</td>
        <td>5</td>
        <td class='alarm out'></td>

        <td class='center'>
          <input type='checkbox' id='alarms-1861' name='alarms[1861]' value='1' />
        </td>

      </tr>
    </tbody>
  </table>
<input type='submit' id='delete-899' name='delete[899]' value='Delete' class='img delete' />
</section>
    
</form>
   
 <script type="text/javascript">

var colors = ["#FF33AA", "#08B37F", "#22ADFF", "#FF4136", "#FF9900", "#3D0000", "#854144b", "#FF3399", "#AAAAAA", ];
var sensorFiles = ["living.tsv", "hall.tsv", "bedroom.tsv", "bathroom.tsv", "kitchen.tsv" ];

 
function populateAlarmSensorList(fileNames)
{ 
  var sensorList = document.getElementById("alarmSensorList");

  var createOptionGroup = function(idx)
  { 
    var fileName = fileNames[idx];
    var color = colors[idx % colors.length];

    var optgroup = document.createElement('optgroup');
    optgroup.label = fileName;
    optgroup.style.color = color;

    var option = document.createElement('option');
    option.value = 0;
    option.label = "Temperature";
    optgroup.appendChild(option);

    var option = document.createElement('option');
    option.value = 0;
    option.label = "Humidity";
    optgroup.appendChild(option);

    sensorList.appendChild(optgroup);
  };

  for (var idx = 0; idx < fileNames.length; idx++)
  { 
    createOptionGroup(idx);
  }
}

populateAlarmSensorList(sensorFiles);


</script>  

</body>
</html>