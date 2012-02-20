<?php
/******************************************************************************
Last revision:
- Author: Seven
- Email: zabkar@gmail.com  (Subject DotaParser)
- Date: 7.7.2009 
******************************************************************************/
?>
<!DOCTYPE html>
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Replay upload</title>
<link href="style_x.css" rel="stylesheet" type="text/css" media="screen" />
</head>

<body>
<div class="wrapper">
<div class="replay">
<h2> Upload replay </h2> 
<?php
$print_info = false;
define("MAX_UPLOAD_SIZE", 3000000);

// Upload a file
if(isset($_POST['uploadReplay'])) {
    if(!isset($_FILES['replay_file']) || !isset($_POST['replay_title']) || !isset($_POST['replay_winner']) || !isset($_POST['replay_text'])) {
        echo 'Error: Make sure you\'ve filled out all the fields.';
    }
    else {
       $title = htmlspecialchars(trim($_POST['replay_title']));
       $winner = htmlspecialchars(trim($_POST['replay_winner']));
       $text = htmlspecialchars(trim($_POST['replay_text'])); 

       // Check that we have a file
       $replayUploaded = false;
       $replayFile = "";
       
       if(( !empty($title) && !empty($winner) &&
            !empty($_FILES["replay_file"])) && ($_FILES['replay_file']['error'] == 0)) {
          //Check if the file is JPEG image and it's size is less than 350Kb
          $filename = basename($_FILES['replay_file']['name']);
          $ext = substr($filename, strrpos($filename, '.') + 1);
          $uniqueID = time();
          
          
          if (($ext == "w3g") && $_FILES["replay_file"]["size"] < MAX_UPLOAD_SIZE) {
             //Determine the path to which we want to save this file
              $newname = dirname(__FILE__).'/replays/'.$uniqueID.'.'.$ext;
              //Check if the file with the same name is already exists on the server
              if (!file_exists($newname)) {
                //Attempt to move the uploaded file to it's new place
                if ((move_uploaded_file($_FILES['replay_file']['tmp_name'], $newname))) {
                   $replayFile = $uniqueID.'.'.$ext;
                   $replayUploaded = true;
                } 
                else {
                   print_message("Error: A problem occurred during file upload!");
                }
              } 
              else {
                 print_message("Error: File ".$_FILES["replay_file"]["name"]." already exists");
              }
          } 
          else {
             print_message("Error: Only .w3g replays under 3 MB are accepted for upload");
          }
        } 
        else {
            print_message("Error: Make sure you've filled out all the fields");
        }

        // If the replay was uploadead successfully, process it
        if( $replayUploaded ) {
            @require("reshine.php");
            
            $replay = new replay('replays/'.$replayFile);
            
            $replay->extra['title'] = $title;
            
            /* Determine the winner 
             * If the uploader chose "Automatic" then check if the parser was able to determine a winner,
             * otherwise the winner is set to "Unknown"
             * Alternatively the uploader can set the winner manually
             */
            if("Automatic" != $winner) {
                $replay->extra['winner'] = ( $winner == "Sentinel" ? "Sentinel" : "Scourge" );
            }
            else if(isset($replay->extra['parsed_winner'])) {
                $replay->extra['winner'] = $replay->extra['parsed_winner'];
            } 
            else {
                $replay->extra['winner'] = "Unknown";
            }
            
            $replay->extra['text'] = $text;
            $replay->extra['original_filename'] = $filename;
            
 
            $txt_file = fopen('replays/'.$replayFile.'.txt', 'a');

            flock($txt_file, 2);
            fputs($txt_file, serialize($replay));
            flock($txt_file, 3);
            fclose($txt_file);
            
            if ( $replay->extra['parsed'] == false ) {
                // Replay not parsed
            }
            else {
                // Replay saved, display the link.
                print_message('Replay uploaded successfully. <a href="view_replay.php?file='.$replayFile.'" alt="View replay" > View details </a>');
                $print_info = true;    
            }
            
            
        }
        
        
    }
}

function print_message($msg) {
    echo '<div style="padding-left: 10px; padding-bottom: 10px;" >';
    echo $msg;
    echo '</div>';
}    
?>

    <div class="content" style="width: 99%;">

        <form enctype="multipart/form-data" action="upload_replay.php" method="post">
        <fieldset>
          <label for="replay_title" >Title*: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</label><input name="replay_title" id="replay_title" type="input" />
          <br />
          <label for="replay_winner" >Winner: &nbsp;&nbsp;&nbsp;&nbsp;</label>
            <select name="replay_winner" id="replay_winner"  />
                <option value="Automatic">Automatic </option>
                <option value="Sentinel">Sentinel </option>
                <option value="Scourge">Scourge </option>
            </select>
          <br />
          <label for="replay_text" style="vertical-align: top;" >Description: </label>
          <textarea name="replay_text" id="replay_text" cols="65"></textarea>
          <br />
          <input type="hidden" name="MAX_FILE_SIZE" id="'.MAX_UPLOAD_SIZE.'" value="3000000" />
          <label for="replay_file" >File*: </label><input name="replay_file" id="replay_file" type="file" />
          <input type="submit" value="Upload" name="uploadReplay" />
        </fieldset>
      </form>    
        
    </div>
<?php
    if( $print_info ) {   
        echo ('<div class="content" style="width: 99%;">
            <h2> Compact replay info </h2>
            <textarea style="width: 99%; height: 500px;">
<div class="wrapper">
<div class="replay">
<h2 class="replay_h2">'.$title.'</h2>
    <div class="replay_content " style="text-align: center;">');
    $replay->print_team_heroes(0);
      
    echo   ('<div class="replay_content " style="padding-left: 0px; text-align: center;">
            <a href="download.php?f='.$replayFile.'&fc='.$replay->extra['original_filename'].'" style="display: inline;"> &raquo;&nbsp;&nbsp; Download  &laquo; </a> 
            <a href="view_replay.php?file='.$replayFile.'" target="_blank" style="display: inline;">  &raquo; More Info &laquo; </a>    
        </div>');
    $replay->print_team_heroes(1); 
    echo   ('</div>                             
</div>
</div>
            </textarea>
        </div>');
    }
?>      
</div>

</div>

</body>
</html>

