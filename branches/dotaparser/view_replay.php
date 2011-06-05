<?php
/******************************************************************************
Last revision:
- Author: Seven
- Email: zabkar@gmail.com  (Subject CDP)
- Date: 16.8.2010 (1.4) 
******************************************************************************/
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Replay</title>
<link href="style_x.css" rel="stylesheet" type="text/css" />
<script type="text/javascript">
var rowToggle = true;

function toggleDisplayRow(id) {
    if( !this.rowToggle ) return;
    
    if(document.getElementById(id).style.display == 'none') {
        document.getElementById(id).style.display = 'block';
        document.getElementById('ih_'+id).innerHTML = 'Show less';
    }
    else {
        document.getElementById(id).style.display = 'none';
        document.getElementById('ih_'+id).innerHTML = 'Show more';
    }    
}

function toggleDisplay(id) {
    if(document.getElementById(id).style.display == 'none') {
        document.getElementById(id).style.display = 'block';
        document.getElementById('ih_'+id).innerHTML = 'Show less';
    }
    else {
        document.getElementById(id).style.display = 'none';
        document.getElementById('ih_'+id).innerHTML = 'Show more';
    }    
}

function toggleRowLink( bool ) {
    this.rowToggle = bool;  
}

function toggleDisplayedExtra(id, pid) {
    if(document.getElementById(id).style.display == 'none') {
        document.getElementById('player'+pid+'_skills').style.display = 'none';
        document.getElementById('player'+pid+'_items').style.display = 'none';
        document.getElementById('player'+pid+'_actions').style.display = 'none';

        document.getElementById(id).style.display = 'block';     
    }    
}
</script>
</head>

<body align="center">
<div class="wrapper" align="center">
<div class="replay">
<?php
  @require("reshine.php");
  $time_start = microtime(); 
  $colors = array();
  $names = array();
  
  $replaysDir = "replays";
  
  if (file_exists($replaysDir.'/'.$_GET['file'].'.txt')) {
        $txt_file = fopen($replaysDir.'/'.$_GET['file'].'.txt', 'r');
        
        $data = "";
        while( ($buff = fgets($txt_file)) != null) {
            $data .= $buff;    
        }
        
        $replay = unserialize($data);
        fclose($txt_file);    
  }
  
  else if (isset($_GET['file'])) {
        $replay = new replay($replaysDir.'/'.$_GET['file']);
        
        $txt_file = fopen($replaysDir.'/'.$_GET['file'].'.txt', 'a');
        flock($txt_file, 2);
        fputs($txt_file, serialize($replay));
        flock($txt_file, 3);
        fclose($txt_file);
  } 
  else {
        exit('No replay file given.');
        $error = 1;
  }
  
  $version = sprintf('%02d', $replay->header['major_v']);
  
  echo "<h2>".$replay->extra['title']."</h2>";
  
  echo ('<div class="content" style="width: 293px; float: left;">
        <b>Host: </b> '.(isset($replay->game['creator']) ? $replay->game['creator'] : "n/a").' 
    </div>
    <div class="content" style="width: 293px; float: right;">
    <b>Saver: </b> '.(isset($replay->game['saver_name']) ? $replay->game['saver_name'] : "n/a").' 
   
    </div>
    <br />
    <div class="content" style="width: 293px; float: left;">
        <b>Players: </b> '.(isset($replay->game['player_count']) ? $replay->game['player_count'] : "n/a").' 
    
    </div>
    <div class="content" style="width: 293px; float: right;">
        <b>Version: </b> 1.'.$version.'    
   
    </div>
    <br />    
    <div class="content" style="width: 293px; float: left;">
        <b>Length: </b> '.convert_time($replay->header['length']).'    
    
    </div>
    <div class="content" style="width: 293px; float: right;">
        <b>Observers: </b> '.$replay->game['observers'].'    
   
    </div>
    <div class="content" style="width: 590px;">
        <b>Map: </b> '.$replay->game['map'].'    
   
    </div>
    <div class="content" style="width: 590px;">
    <b>Winner: </b><span id=\'spoiler\' 
    style="border: solid 1px #CCC; padding: 0px 20px; background: #000; color: #000;"
    onmouseover="this.style.background=\'#FFF\'" 
    onmouseout="this.style.background=\'#000\'">
    '.$replay->extra['winner'].'
    </span>
    </div>
    <br />
    <div class="content" style="width: 590px;">
        <b>Download: </b> <a href="download.php?f='.$_GET['file'].'&fc='.$replay->extra['original_filename'].'" >'.$replay->extra['original_filename'].'</a>    
   
    </div>');
    
    

   echo ('
    <div style="display: '.($replay->bans_num > 0 ? 'block' : 'none').'" >
    <hr />
    <h2> Ban / Pick </h2>
    <div class="content" style="width: 586px;">
        <b>Bans:&nbsp; </b>');
   
   // Display Bans
   $i = 0;
   foreach($replay->bans as $hero) {
        $team = ($hero->extra == 0 ? "sentinel" : "scourge");
       
        echo '<img src="'.$hero->getArt().'" width="30px" height="30px" alt="Hero" title="'.$hero->getName().'" class="'.$team.'BanHero" />';
   
        $i++;
        if ( $i < count($replay->bans) )
            echo "-";
   
   }     
   
   echo ('</div>
    <br />
    <div class="content" style="width: 586px;">
        <b>Picks: </b>');
   
   // Display Picks 
   $i_pick = 0;
   foreach($replay->picks as $hero) {  
        $team = ($hero->extra == 0 ? "sentinel" : "scourge");
       
        echo '<img src="'.$hero->getArt().'" width="30px" height="30px" alt="Hero" title="'.$hero->getName().'" class="'.$team.'BanHero" />';
        
        // Output for CM Mode post 6.68 (3/2 ban split)
        if( isset($replay->game['dota_major']) && 
            isset($replay->game['dota_minor']) && 
            (($replay->game['dota_major'] == 6 && $replay->game['dota_minor'] >= 68) || $replay->game['dota_major'] > 6) 
          ) {
          if( count($replay->picks) != $i_pick+1 && ($i_pick >= 5 || $i_pick % 2 == 0)) {
            echo "-";    
          }
        }
        // Output for pre 6.68 CM Mode
        else {
            
            if( $i_pick % 2 == 0) {
                echo "-";    
            }
        }
        $i_pick++;     
   }
   echo "</div>";
   
   
   // Display Teams 
   for($i = 0; $i < 2; $i++) {
       $team = ( $i == 0 ? "Sentinel" : "Scourge" );
       
       echo '</div>
            
            <hr />
            <h2> '.$team.' </h2>
            <div class="content" style="width: 600px; padding-left: 0px; margin-left: 0px;">
                <table class="statsTable" border="0" cellpadding="0" cellspacing="0" width="100%">
                <tr class="tableHeader">
                    <td class="tableHeader" width="180px">Name </td>
                    <td class="tableHeader">Level </td>
                    <td class="tableHeader">APM </td>
                    <td class="tableHeader">K/D/A </td>
                    <td class="tableHeader">CS </td>
                    <td class="tableHeader">Extra </td>   
                </tr>';
       
       foreach($replay->teams[$i] as $pid=>$player) {
             
             // Convert 1.2 version to legacy (1.1) output
             if ( isset($replay->ActivatedHeroes) ) {
                 if ( $replay->stats[$player['dota_id']]->getHero() == false ) continue;
                                   
                 $t_heroName = $replay->stats[$player['dota_id']]->getHero()->getName();
                 
                 // Set level
                 $player['heroes'][$t_heroName]['level'] = $replay->stats[$player['dota_id']]->getHero()->getLevel();
                 
                 $t_heroSkills = $replay->stats[$player['dota_id']]->getHero()->getSkills();
                 
                 // Convert skill array to old format
                 foreach ( $t_heroSkills as $time => $skill ) {
                     $player['heroes'][$t_heroName]['abilities'][$time] = $skill;
                 }
                 
                 $player['heroes'][$t_heroName]['data'] = $replay->stats[$player['dota_id']]->getHero()->getData();
             }
           
             // Get player's hero
             foreach($player['heroes'] as $name=>$hero) {
                 
                if( $name == "order" || !isset($hero['level'])) continue; 
                
                
                 
                if( $name != "Common" ) {    
                    // Merge common skills and atribute stats with Hero's skills
                    if(isset($player['heroes']['Common']) ) {
                        
                        $hero['level'] += $player['heroes']['Common']['level'];
                        $hero['abilities'] = array_merge($hero['abilities'], $player['heroes']['Common']['abilities']);
                    }
                    if ( $hero['level'] > 25 ) {
                        $hero['level'] = 25;    
                    }
                    @ksort($hero['abilities']);
                    $p_hero = $hero;
                
                    break;    
                }
             }
           
             
             
             echo ('<tr class="table'.$team.'" onclick="javascript:toggleDisplayRow(\'player'.$player['player_id'].'\');">
                <td class="table'.$team.'">');
             echo '<img src="'.$p_hero['data']->getArt().'" width="25px" height="25px"';
             echo ' alt="'.$p_hero['data']->getName().'" title="'.$p_hero['data']->getName().'"';
             echo (' style="vertical-align: middle;"/> <span class="playerName" style="font-weight: bolder;">'.$player['name'].' </span></td>
                <td class="table'.$team.'">'.$p_hero['level'].' </td>
                <td class="table'.$team.'">'.round( (60 * 1000 * $player['apm']) / ($player['time'])).' </td>');
             
             
             if(isset($replay->stats[$player['dota_id']])) {
                $stats = $replay->stats[$player['dota_id']];
                echo ('<td class="table'.$team.'">'.$stats->HeroKills.'/'.$stats->Deaths.'/'.$stats->Assists.' </td>
                    <td class="table'.$team.'">'.$stats->CreepKills.'/'.$stats->CreepDenies.'/'.$stats->Neutrals.' </td>');
                    
             }
             else {
                 echo ('<td class="table'.$team.'"> N/A </td>
                    <td class="table'.$team.'"> N/A </td>' );
                                                
             }
             echo ('<td class="table'.$team.'"><a onmouseover="javascript:toggleRowLink(false);" onmouseout="javascript:toggleRowLink(true);" href="javascript:toggleDisplay(\'player'.$player['player_id'].'\');"><div id="ih_player'.$player['player_id'].'">Show more</div></a></td>    
                </tr>');
            
            // Start extra info display
            echo ('<tr class="tableExtra">
                    <td colspan="6" class="tableExtra">
                        <div width="100%" style="display: none;" id="player'.$player['player_id'].'" >');
           
           
            // Display Inventory
            echo  '<div class="inventory" style="float: left;">';
            for($j = 0; $j < 6; $j++) {
                $art = ( isset($stats->Inventory[$j]) && is_object($stats->Inventory[$j]) ) ?  $stats->Inventory[$j]->getArt() : "images/BTNEmpty.gif";
                $name = ( isset($stats->Inventory[$j]) && is_object($stats->Inventory[$j]) ) ?  $stats->Inventory[$j]->getName() : "Empty"; 
                
                
                echo '<img src="'.$art.'" width="48px" height="48px" alt="'.$name.'" title="'.$name.'" />';
            }
            echo '</div>';
            
            // Handle player left events
            if(isset($player['time'])) {
                $playerLeaveTime = convert_time($player['time']);
            }
            else {
                $playerLeaveTime = convert_time($replay->header['length']);
            }
            if(isset($player['leave_result'])) {
                $leaveResult = $player['leave_result'];
            }
            else {
                $leaveResult = "Finished";
            }
            
            // Display other
            echo  ('<div class="extraOther" style="float: right;">
                        <div class="displaySelect">
                            Left at '.$playerLeaveTime.', reason: '.$leaveResult.'.');
            if(isset($replay->stats[$player['dota_id']]->AA_Total) && isset($replay->stats[$player['dota_id']]->AA_Hits) && $replay->stats[$player['dota_id']]->AA_Total > 0 ) {
                echo ("<br />Arrow accuracy: ".round((($replay->stats[$player['dota_id']]->AA_Hits / $replay->stats[$player['dota_id']]->AA_Total)*100))."% 
                        (".$replay->stats[$player['dota_id']]->AA_Hits."/".$replay->stats[$player['dota_id']]->AA_Total.")");
            }
            if(isset($replay->stats[$player['dota_id']]->HA_Total) && isset($replay->stats[$player['dota_id']]->HA_Hits) && $replay->stats[$player['dota_id']]->HA_Total > 0 ) {
                echo ("<br />Hook accuracy: ".round((($replay->stats[$player['dota_id']]->HA_Hits / $replay->stats[$player['dota_id']]->HA_Total)*100))."% 
                        (".$replay->stats[$player['dota_id']]->HA_Hits."/".$replay->stats[$player['dota_id']]->HA_Total.")");
            }
                                
           echo    ('         </div>
                        <div class="displaySelect">
                            <a href="javascript:toggleDisplayedExtra(\'player'.$player['player_id'].'_skills\',\''.$player['player_id'].'\');">Skills</a> | 
                            <a href="javascript:toggleDisplayedExtra(\'player'.$player['player_id'].'_items\',\''.$player['player_id'].'\');">Items</a> |
                            <a href="javascript:toggleDisplayedExtra(\'player'.$player['player_id'].'_actions\',\''.$player['player_id'].'\');">Actions</a>
                        </div>');
            
            // Display skills
            echo '<div id="player'.$player['player_id'].'_skills" class="extraInfo" >';
            
            $i_skill = 0;
            unset($a_level);
            foreach ($p_hero['abilities'] as $time=>$ability) {
                $i_skill++;
                if ($i_skill > 25 ) break;
                  
                if(!isset($a_level[$ability->getName()])) {
                      $a_level[$ability->getName()] = 1;
                  }
                  else {
                      $a_level[$ability->getName()]++;
                  }
                  echo ('<b>'.($i_skill < 10 ? "&nbsp;" : "").$i_skill.' </b><img style="vertical-align: middle;" src="'.$ability->getArt().'" width="18px" height="18px" /> &#160;<b>'.convert_time($time).'</b>&#160; '.$ability->getName().' '.$a_level[$ability->getName()].'<br />');
            }
            echo '</div>';
            
            
            // Display items
            echo '<div id="player'.$player['player_id'].'_items" class="extraInfo" style="display: none;">';
            
            foreach ($player['items'] as $time=>$item) {
                if (is_object($item) && $item->getName() != "Select Hero" ) {
                  echo ('<img style="vertical-align: middle;" src="'.$item->getArt().'" alt="Item" title="'.$item->getName().'" width="18px" height="18px" /> &#160;<b>'.$time.'</b>&#160; '.$item->getName().'<br />');
                }
            }
            echo '</div>';
            
            // Display actions
            echo '<div id="player'.$player['player_id'].'_actions" class="extraInfo" style="display: none;">';
            
            if (isset($player['actions_details'])) {
                  ksort($player['actions_details']);
                  
                  $px_per_action = 400 / $player['apm'];
                  
                  foreach ($player['actions_details'] as $name=>$info) {
                    echo ('<b>'.$name.' '.$info.'</b><div class="graph" style="width: '.round($info * $px_per_action).'px;"></div><br />');
                  }
            }
            echo '</div>'; 
           
           
           echo '</div>';
           // End extra info display
           echo   ('</div>
                     </td>
                    </tr>');
            
            
            // Remember colors for Chat display.
            if ($player['color']) {
                $colors[$player['player_id']] = $player['color'];
                $names[$player['player_id']] = $player['name'];
            }    
       }
       echo "</table>";
   }
    
    
   
   /* Display Description */
   echo '</table>
    </div>
    <hr />
    <h2> Description </h2>
    <div class="content" style="width: 586px;">
       '.$replay->extra['text'].'
    </div>';
   
   
   /* Display Chat */ 
   echo '<hr />
    <h2> Chat </h2>
    <div class="chatWindow">';
    
   
   foreach ($replay->chat as $content) {
        $prev_time = $content['time'];
        
        echo('('.convert_time($content['time']));
        
        if (isset($content['mode']) && isset($colors[$content['mode']]) && isset($names[$content['mode']])) {
          if (is_int($content['mode'])) {
            echo(' / '.($content['mode'] == 0x01 ? "Allied" : ""));
          }
          else {
            echo(' / '.$content['mode']);
          }
        }
        
        echo(') ');
        
        if (isset($content['player_id'])) {
          // no color for observers
          if (isset($colors[$content['player_id']])) {
            echo('<span style="font-weight: bold;" class="'.$colors[$content['player_id']].'">'.$content['player_name'].'</span>: ');
          } 
          else {
            echo('<span style="font-weight: bold;" class class="observer">'.$content['player_name'].'</span>: ');
          }
        }

        echo(htmlspecialchars($content['text'], ENT_COMPAT, 'UTF-8').'<br />');
  }
  
  echo '</div>';  
   
  
  
   $time_end = microtime();
   $temp = explode(' ', $time_start.' '.$time_end);
   $duration = sprintf('%.8f',($temp[2] + $temp[3]) - ( $temp[0] + $temp[1]));
   
   echo "<div align='center'> Copyleft &copy; CTS - Dotaparser - Loaded in: ".$duration." sec";
  
?>
</div>
</div>

</body>
</html>
