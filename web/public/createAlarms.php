<!DOCTYPE html>
<head><title>Create Alarms</title></head>
<?php
    
    include('includes.php');

    include('checklogin.php');

    include('db.php');

    $db = mysqli_connect(DB_SERVER,DB_USERNAME,DB_PASSWORD, $_SESSION["sensorDB"]);
    if (mysqli_connect_errno())
    {
      echo "Failed to connect to MySQL: " . mysqli_connect_error();
    }
    $sql = "SELECT * from sensors";

    $result = mysqli_query($db, $sql);

    $sqlDisplay = "SELECT alerts.id as id, alerts.above as above, alerts.value as value, alerts.measurement as measurement, alerts.duration as duration, sensors.name as name FROM alerts
                   LEFT JOIN sensors
                   ON sensors.id = alerts.sensor_id 
                   ";
    $resultDisplay = mysqli_query($db, $sqlDisplay);

    $sqlAlarm = "SELECT a.id as alarm_id, m.sensor_id as sensor_id FROM `alerts` AS a 
              LEFT JOIN `measurements` AS m
              ON a.sensor_id = m.sensor_id OR a.sensor_id < 0
              WHERE ((a.above = 0 AND a.measurement = 0 AND m.temperature < a.value) OR
                    (a.above = 0 AND a.measurement = 1 AND m.humidity < a.value) OR
                    (a.above = 1 AND a.measurement = 0 AND m.temperature > a.value) OR
                    (a.above = 1 AND a.measurement = 1 AND m.humidity > a.value)) AND m.flags = 0;"
?>

<link href= "css/createAlarms.css" rel="stylesheet" type ="text/css"/>

<header>
    <?php
      include('mainTopBar.php');
    ?>
</header>
<body>

  <div id="Content">
    <?php
      include('settingsSideBar.php');
    ?>
    <div id="ContentRight">

    <form action ="createNewAlarm.php" method="post" id="createAlarmForm">    
        <p class="message">Create a new alarm to get notificed when measurements
                passes the threshold value you specify.</p>

        <table id ="alarmTable" width="400" border="0" cellpadding="6" cellspacing="6">
          <tr>
            <td style="font-weight: lighter"><div align="left"><label for="source">Source</label></div></td>
              <td>
              <select id='source' name="source">
                <optgroup label = "Any">
                    <option value = "any_t">Temperature</option>
                    <option value = "any_h">Humidity</option>
                </optgroup>

                <?php
                     while ($row = mysqli_fetch_array($result, MYSQLI_ASSOC))
                    {                     
                      echo "<optgroup label=\"".$row["name"]."\"><option value = \"".$row["id"]."\">Temperature</option>
                          <option value = \"-".$row["id"]."\">Humidity</option></opgroup>";
                    }
                ?>
               </select></td>
            </tr>
            <tr>
              <td style="font-weight: lighter"><div align="left"><label for="direction">Direction</label></div></td>
              <td>
                <select id='direction' name='direction' class='mandatory'>
                  <option value='1'>Above</option>
                  <option value='0'>Below</option>
                </select>
              </td>
            </tr>
            <tr>
              <td style="font-weight: lighter"><div align="left"><label for="value">Value</label></div></td>
              <td><div align="left"><input type='text' id='value' name='value' value='' /></div></td>
            </tr>
            <tr>
               <td style="font-weight: lighter"><div align="left"><label for='duration' class='number'>For minutes</label></div></td>
               <td><div align="left"><input type='text' id='duration' name='duration' value=''  /></div></td>
            </tr>
    
        </table>
        
        <p class="message1">Optionally delay the alarm. E.g. enter "60" to trigger it only after one hour of measurements continuously exceeding the threshold.</p>

       
          <input type='submit' id='create' name='create' value='Create alarm'  />

      </form>   
   
        <br>
      
        <h2>Alarms list</h2>
            
          <p class = "message">These are your alarms.
          <a href='data/alarms.php'>Click here</a> to view their notification history.
          </p>

      <form action ="deleteAlarms.php" method="post" id="deleteAlarmForm">   
        <table id="alarmsView";>
            <tr>
              <th style='width: 5%;' class='center'></th>
              <th style='width: 65%;'>Source</th>
              <th style='width: 15%;'>Duration</th>
              <th style='width: 15%;' class='center'>State</th>
              
            </tr>

        <?php
          while ($row = mysqli_fetch_array($resultDisplay, MYSQLI_ASSOC)){
        ?>
          
          <tr>
             <td bgcolor="#E9EDED" id ="deleteCheckbox" ><input name="checkbox[]" type="checkbox" align ='center' value="<?php echo $row['id']; ?>"></td>
             <td bgcolor="#E9EDED"><?php
                                        $above = ""; 
                                        if($row["above"] == 1)
                                        {
                                          $above = "above";
                                        }
                                        else
                                        {
                                          $above = "below";
                                        }

                                        $measurementType = "";
                                        if($row["measurement"] == 1)
                                        {
                                          $measurementType = "humidity";
                                        }
                                        else
                                        {
                                          $measurementType = "temperature";
                                        }

                                        echo $row["name"]." for any ".$measurementType." ".$above." ".$row["value"] ; ?></td>
              <td bgcolor="#E9EDED"><?php echo $row["duration"]; ?></td>
              <td bgcolor="#E9EDED"><?php echo $row["above"]; ?></td>
          </tr>
          
         
        <?php } ?> 

         <tr>
          <td><input id ="delete" name="delete" type="submit" value="Delete selected" onclick="return confirm('Are you sure you want to delete these alarms?');">
          </td>
        </tr>   
      </table>   
    
     </form> 

</body>
