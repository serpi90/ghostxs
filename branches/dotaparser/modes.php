<?php
/******************************************************************************
Last revision:
- Author: Seven
- Email: zabkar@gmail.com  (Subject CDP)
- Date: 1.10.2012 (1.4.5)
******************************************************************************/

class GenericMode {
    protected $shortName;
    protected $fullName;
    
    function GenericMode() {
        
    }
    
    function getShortName() {
        return $this->shortName;
    }
    
    function getFullName() {
        return $this->fullName;
    }
    
}

/**
* Desc: Captain Draft
* Short: CD
*/
class ModeCD extends GenericMode {
    private $bansPerTeam = 2;
    private $heroPool;
    private $heroBans;
    private $heroPicks;
    
    function ModeCD() {
        $this->heroPool = array();
        $this->heroBans = array();
        $this->heroPicks = array();
        $this->shortName = "cd";
        $this->fullName = "Captain's draft";
    }
    
    public function getBansPerTeam() {
        return $this->bansPerTeam;
    }
    
    /**
    * Collecting bans
    * @param Entity $hero 
    */
    function addHeroToBans( $hero ) {
        $this->heroBans[] = $hero;
    }
    
    /**
    * Collecting picks
    * @param Entity $hero 
    */
    function addHeroToPicks( $hero ) {
        $this->heroPicks[] = $hero;
    }
    
    /**
    *  Used to gather hero pool information
    * @param Entity $hero 
    */
    function addHeroToPool( $hero ) {
        $this->heroPool[] = $hero;
    }
    
    /**
    * Returns current amount of picked heroes
    * @returns int Num of picks
    */
    function getNumPicked() {
        return count($this->heroPicks);
    }
    
    function getBans() {
        return $this->heroBans;
    }
    
    function getPicks() {
        return $this->heroPicks;
    }
}

/**
* Desc: Captain Mode
* Short: CM
*/
class ModeCM extends GenericMode {
    private $bansPerTeam = 2;
    private $heroBans;
    private $heroPicks;
    
    /**
    * Constructor
    * 
    */
    function ModeCM() {
        $this->heroBans = array();
        $this->heroPicks = array();
        $this->shortName = "cm";
        $this->fullName = "Captain's mode";
    }

	/**
	 * Get bans per team
	 *
	 * @param int $version - dota version
	 *
	 * @return Number of hero bans per team
	 */
    public function getBansPerTeam($version = 675) {
	    if($version < 668) {
		    return 4;
	    }
	    else {
		    // If we're dealing with versions post 6.68 bans are split either 3/2 or 2/3
		    // So initially bansPerTeam are set to either 3 or 2 for phase one.
		    // Eventually bansPerTeam are set to 5 indicating we're in phase 2, in that case we return 5 as bans per team.
		    if(5 !== $this->bansPerTeam) {
			    // First phase bans
				if($version > 668 && $version < 675) {
					return 3;
				}
			    else {
				    // 6.75+
				    return 2;
			    }
		    }
		    else {
			    // Return second phase bans per team
			    return 5;
		    }
	    }
    }
    
    /**
    * Collecting bans
    * @param Entity $hero 
    */
    function addHeroToBans( $hero ) {
        $this->heroBans[] = $hero;
    }
    
    /**
    * Returns current amount of banned heroes
    * @returns int Num of bans
    */
    function getNumBanned() {
        return count($this->heroBans);
    }
    
    /**
    * Collecting picks
    * @param Entity $hero 
    */
    function addHeroToPicks( $hero ) {
        $this->heroPicks[] = $hero;
    }
    
    /**
    * Returns current amount of picked heroes
    * @returns int Num of picks
    */
    function getNumPicked() {
        return count($this->heroPicks);
    }
    
    /**
    * Get an array of bans
    * @returns Array of Hero Entity type bans
    */
    function getBans() {
        return $this->heroBans;
    }
    
    /**
    * Get an array of picks
    * @returns Array of Hero Entity type picks
    */
    function getPicks() {
        return $this->heroPicks;
    }
    
    /**
    *
    * Returns TRUE if number of banned heroes equals or exceeds the set bansPerTeam
    * @returns Boolean True - First Ban Phase Complete or False
    */
    function banPhaseComplete($version = 675) {
        if(count($this->heroBans) >= ($this->getBansPerTeam($version) * 2) ) {
            return true;
        }
        return false;
    }
    
    /**
    * Sets the amount of bans per team
    * 
    * @param mixed $num Bans per team
    */
    function setBansPerTeam($num) {
        $this->bansPerTeam = $num;
    }
    
}