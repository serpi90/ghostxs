/*

Copyright [2008] [Trevor Hogan]

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#include "ghost.h"
#include "util.h"
#include "config.h"
#include "language.h"
#include "socket.h"
#include "ghostdb.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "game_base.h"
#include "game.h"
#include "stats.h"
#include "statsdota.h"
#include "statsw3mmd.h"

#include <cmath>
#include <string.h>
#include <time.h>

//
// sorting classes
//

class CGamePlayerSortAscByPing
{
public:
	bool operator( ) ( CGamePlayer *Player1, CGamePlayer *Player2 ) const

	{
		return Player1->GetPing( false ) < Player2->GetPing( false );

	}
};

class CGamePlayerSortDescByPing
{
public:
	bool operator( ) ( CGamePlayer *Player1, CGamePlayer *Player2 ) const

	{
		return Player1->GetPing( false ) > Player2->GetPing( false );

	}
};

//
// CGame
//

CGame :: CGame( CGHost *nGHost, CMap *nMap, CSaveGame *nSaveGame, uint16_t nHostPort, unsigned char nGameState, string nGameName, string nOwnerName, string nCreatorName, string nCreatorServer ) : CBaseGame( nGHost, nMap, nSaveGame, nHostPort, nGameState, nGameName, nOwnerName, nCreatorName, nCreatorServer ), m_DBBanLast( NULL ), m_Stats( NULL ) ,m_CallableGameAdd( NULL )
{
	m_DBGame = new CDBGame( 0, string( ), m_Map->GetMapPath( ), string( ), string( ), string( ), 0 );

	if( m_Map->GetMapType( ) == "w3mmd" )
		m_Stats = new CStatsW3MMD( this, m_Map->GetMapStatsW3MMDCategory( ) );
	else if( m_Map->GetMapType( ) == "dota" )
		m_Stats = new CStatsDOTA( this );
	trade_allowed = m_Map->GetTradeAllowed( );
}

CGame :: ~CGame( )
{
    	if( m_CallableGameAdd && m_CallableGameAdd->GetReady( ) )
    {
        if (m_GHost->m_GameIDReplays)
        {
            m_DatabaseID = m_CallableGameAdd->GetResult();
        }
        if ( m_CallableGameAdd->GetResult( ) > 0 )
        {
            CONSOLE_Print( "[GAME: " + m_GameName + "] saving player/stats data to database" );

            // store the CDBGamePlayers in the database

            for ( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); ++i )
                m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedGamePlayerAdd( m_CallableGameAdd->GetResult( ), (*i)->GetName( ), (*i)->GetIP( ), (*i)->GetSpoofed( ), (*i)->GetSpoofedRealm( ), (*i)->GetReserved( ), (*i)->GetLoadingTime( ), (*i)->GetLeft( ), (*i)->GetLeftReason( ), (*i)->GetTeam( ), (*i)->GetColour( ) ) );

            // store the stats in the database

            if ( m_Stats )
                m_Stats->Save( m_GHost, m_GHost->m_DB, m_CallableGameAdd->GetResult( ) );
        }
        else
            CONSOLE_Print( "[GAME: " + m_GameName + "] unable to save player/stats data to database" );

        m_GHost->m_DB->RecoverCallable( m_CallableGameAdd );
        delete m_CallableGameAdd;
        m_CallableGameAdd = NULL;
    }

    for ( vector<PairedBanCheck> :: iterator i = m_PairedBanChecks.begin( ); i != m_PairedBanChecks.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for ( vector<PairedBanAdd> :: iterator i = m_PairedBanAdds.begin( ); i != m_PairedBanAdds.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for ( vector<PairedGPSCheck> :: iterator i = m_PairedGPSChecks.begin( ); i != m_PairedGPSChecks.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for ( vector<PairedDPSCheck> :: iterator i = m_PairedDPSChecks.begin( ); i != m_PairedDPSChecks.end( ); ++i )
        m_GHost->m_Callables.push_back( i->second );

    for ( vector<CDBBan *> :: iterator i = m_DBBans.begin( ); i != m_DBBans.end( ); ++i )
        delete *i;

    delete m_DBGame;

    for ( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); ++i )
        delete *i;

    delete m_Stats;

    // it's a "bad thing" if m_CallableGameAdd is non NULL here
    // it means the game is being deleted after m_CallableGameAdd was created (the first step to saving the game data) but before the associated thread terminated
    // rather than failing horribly we choose to allow the thread to complete in the orphaned callables list but step 2 will never be completed
    // so this will create a game entry in the database without any gameplayers and/or DotA stats

    if ( m_CallableGameAdd )
    {
        CONSOLE_Print( "[GAME: " + m_GameName + "] game is being deleted before all game data was saved, game data has been lost" );
        m_GHost->m_Callables.push_back( m_CallableGameAdd );
    }
}

bool CGame :: Update( void *fd, void *send_fd )
{
    // update callables

    for ( vector<PairedBanCheck> :: iterator i = m_PairedBanChecks.begin( ); i != m_PairedBanChecks.end( ); )
    {
        if ( i->second->GetReady( ) )
        {
            CDBBan *Ban = i->second->GetResult( );

            if ( Ban )
                SendAllChat( m_GHost->m_Language->UserWasBannedOnByBecause( i->second->GetServer( ), i->second->GetUser( ), Ban->GetDate( ), Ban->GetAdmin( ), Ban->GetReason( ) ) );
            else
                SendAllChat( m_GHost->m_Language->UserIsNotBanned( i->second->GetServer( ), i->second->GetUser( ) ) );

            m_GHost->m_DB->RecoverCallable( i->second );
            delete i->second;
            i = m_PairedBanChecks.erase( i );
        }
        else
            ++i;
    }

    for ( vector<PairedBanAdd> :: iterator i = m_PairedBanAdds.begin( ); i != m_PairedBanAdds.end( ); )
    {
        if ( i->second->GetReady( ) )
        {
            if ( i->second->GetResult( ) )
            {
                for ( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); ++j )
                {
                    if ( (*j)->GetServer( ) == i->second->GetServer( ) )
                        (*j)->AddBan( i->second->GetUser( ), i->second->GetIP( ), i->second->GetGameName( ), i->second->GetAdmin( ), i->second->GetReason( ) );
                }

                //SendAllChat( m_GHost->m_Language->PlayerWasBannedByPlayer( i->second->GetServer( ), i->second->GetUser( ), i->first ) );
            }

            m_GHost->m_DB->RecoverCallable( i->second );
            delete i->second;
            i = m_PairedBanAdds.erase( i );
        }
        else
            ++i;
    }

    for ( vector<PairedGPSCheck> :: iterator i = m_PairedGPSChecks.begin( ); i != m_PairedGPSChecks.end( ); )
    {
        if ( i->second->GetReady( ) )
        {
            CDBGamePlayerSummary *GamePlayerSummary = i->second->GetResult( );

            if ( GamePlayerSummary )
            {
                if ( i->first.empty( ) )
                    SendAllChat( m_GHost->m_Language->HasPlayedGamesWithThisBot( i->second->GetName( ), GamePlayerSummary->GetFirstGameDateTime( ), GamePlayerSummary->GetLastGameDateTime( ), UTIL_ToString( GamePlayerSummary->GetTotalGames( ) ), UTIL_ToString( (float)GamePlayerSummary->GetAvgLoadingTime( ) / 1000, 2 ), UTIL_ToString( GamePlayerSummary->GetAvgLeftPercent( ) ) ) );
                else
                {
                    CGamePlayer *Player = GetPlayerFromName( i->first, true );

                    if ( Player )
                        SendChat( Player, m_GHost->m_Language->HasPlayedGamesWithThisBot( i->second->GetName( ), GamePlayerSummary->GetFirstGameDateTime( ), GamePlayerSummary->GetLastGameDateTime( ), UTIL_ToString( GamePlayerSummary->GetTotalGames( ) ), UTIL_ToString( (float)GamePlayerSummary->GetAvgLoadingTime( ) / 1000, 2 ), UTIL_ToString( GamePlayerSummary->GetAvgLeftPercent( ) ) ) );
                }
            }
            else
            {
                if ( i->first.empty( ) )
                    SendAllChat( m_GHost->m_Language->HasntPlayedGamesWithThisBot( i->second->GetName( ) ) );
                else
                {
                    CGamePlayer *Player = GetPlayerFromName( i->first, true );

                    if ( Player )
                        SendChat( Player, m_GHost->m_Language->HasntPlayedGamesWithThisBot( i->second->GetName( ) ) );
                }
            }

            m_GHost->m_DB->RecoverCallable( i->second );
            delete i->second;
            i = m_PairedGPSChecks.erase( i );
        }
        else
            ++i;
    }

    for ( vector<PairedDPSCheck> :: iterator i = m_PairedDPSChecks.begin( ); i != m_PairedDPSChecks.end( ); )
    {
        if ( i->second->GetReady( ) )
        {
            CDBDotAPlayerSummary *DotAPlayerSummary = i->second->GetResult( );

            if ( DotAPlayerSummary )
            {
                string Summary = m_GHost->m_Language->HasPlayedDotAGamesWithThisBot(	i->second->GetName( ),
                                 UTIL_ToString( DotAPlayerSummary->GetTotalGames( ) ),
                                 UTIL_ToString( DotAPlayerSummary->GetTotalWins( ) ),
                                 UTIL_ToString( DotAPlayerSummary->GetTotalLosses( ) ),
                                 UTIL_ToString( DotAPlayerSummary->GetTotalKills( ) ),
                                 UTIL_ToString( DotAPlayerSummary->GetTotalDeaths( ) ),
                                 UTIL_ToString( DotAPlayerSummary->GetTotalCreepKills( ) ),
                                 UTIL_ToString( DotAPlayerSummary->GetTotalCreepDenies( ) ),
                                 UTIL_ToString( DotAPlayerSummary->GetTotalAssists( ) ),
                                 UTIL_ToString( DotAPlayerSummary->GetTotalNeutralKills( ) ),
                                 UTIL_ToString( DotAPlayerSummary->GetTotalTowerKills( ) ),
                                 UTIL_ToString( DotAPlayerSummary->GetTotalRaxKills( ) ),
                                 UTIL_ToString( DotAPlayerSummary->GetTotalCourierKills( ) ),
                                 UTIL_ToString( DotAPlayerSummary->GetAvgKills( ), 2 ),
                                 UTIL_ToString( DotAPlayerSummary->GetAvgDeaths( ), 2 ),
                                 UTIL_ToString( DotAPlayerSummary->GetAvgCreepKills( ), 2 ),
                                 UTIL_ToString( DotAPlayerSummary->GetAvgCreepDenies( ), 2 ),
                                 UTIL_ToString( DotAPlayerSummary->GetAvgAssists( ), 2 ),
                                 UTIL_ToString( DotAPlayerSummary->GetAvgNeutralKills( ), 2 ),
                                 UTIL_ToString( DotAPlayerSummary->GetAvgTowerKills( ), 2 ),
                                 UTIL_ToString( DotAPlayerSummary->GetAvgRaxKills( ), 2 ),
                                 UTIL_ToString( DotAPlayerSummary->GetAvgCourierKills( ), 2 ) );

                if ( i->first.empty( ) )
                    SendAllChat( Summary );
                else
                {
                    CGamePlayer *Player = GetPlayerFromName( i->first, true );

                    if ( Player )
                        SendChat( Player, Summary );
                }
            }
            else
            {
                if ( i->first.empty( ) )
                    SendAllChat( m_GHost->m_Language->HasntPlayedDotAGamesWithThisBot( i->second->GetName( ) ) );
                else
                {
                    CGamePlayer *Player = GetPlayerFromName( i->first, true );

                    if ( Player )
                        SendChat( Player, m_GHost->m_Language->HasntPlayedDotAGamesWithThisBot( i->second->GetName( ) ) );
                }
            }

            m_GHost->m_DB->RecoverCallable( i->second );
            delete i->second;
            i = m_PairedDPSChecks.erase( i );
        }
        else
            ++i;
    }

    return CBaseGame :: Update( fd, send_fd );
}

void CGame :: EventPlayerDeleted( CGamePlayer *player )
{
    CBaseGame :: EventPlayerDeleted( player );

    // record everything we need to know about the player for storing in the database later
    // since we haven't stored the game yet (it's not over yet!) we can't link the gameplayer to the game
    // see the destructor for where these CDBGamePlayers are stored in the database
    // we could have inserted an incomplete record on creation and updated it later but this makes for a cleaner interface

    if ( m_GameLoading || m_GameLoaded )
    {
        // todotodo: since we store players that crash during loading it's possible that the stats classes could have no information on them
        // that could result in a DBGamePlayer without a corresponding DBDotAPlayer - just be aware of the possibility

        unsigned char SID = GetSIDFromPID( player->GetPID( ) );
        unsigned char Team = 255;
        unsigned char Colour = 255;

        if ( SID < m_Slots.size( ) )
        {
            Team = m_Slots[SID].GetTeam( );
            Colour = m_Slots[SID].GetColour( );
        }

        m_DBGamePlayers.push_back( new CDBGamePlayer( 0, 0, player->GetName( ), player->GetExternalIPString( ), player->GetSpoofed( ) ? 1 : 0, player->GetSpoofedRealm( ), player->GetReserved( ) ? 1 : 0, player->GetFinishedLoading( ) ? player->GetFinishedLoadingTicks( ) - m_StartedLoadingTicks : 0, m_GameTicks / 1000, player->GetLeftReason( ), Team, Colour ) );

        // also keep track of the last player to leave for the !banlast command

        for ( vector<CDBBan *> :: iterator i = m_DBBans.begin( ); i != m_DBBans.end( ); ++i )
        {
            if ( (*i)->GetName( ) == player->GetName( ) )
                m_DBBanLast = *i;
        }
    }
}

bool CGame :: EventPlayerAction( CGamePlayer *player, CIncomingAction *action )
{
    bool success = CBaseGame :: EventPlayerAction( player, action );

    // give the stats class a chance to process the action

    if ( success && m_Stats && m_Stats->ProcessAction( action ) && m_GameOverTime == 0 )
    {
        CONSOLE_Print( "[GAME: " + m_GameName + "] gameover timer started (stats class reported game over)" );
        SendEndMessage( );
        m_GameOverTime = GetTime( );
    }
	
	return success;
	
	if (m_GameEndCountDownStarted)
    {
		if (m_AutoEnded)
		{
			m_AutoEnd = false;
			m_AutoEnded = false;
		}
		m_GameEndCountDownStarted = false;
    }
    /* Anti Trade-Hack thanks to K[a]ne from CodeLain
     * http://www.codelain.com/forum/index.php?topic=17681.0;topicseen
     */
    BYTEARRAY *ActionData = action->GetAction( );

    if ( !trade_allowed && player && ActionData->size( ) >= 1 && m_GameLoaded )
    {
        if ( (*ActionData)[0] == 0x51 )
        {
            CONSOLE_Print( "[GAME: " + m_GameName + "] tradehack detected by [" + player->GetName( ) + "]" );
            SendAllChat( m_GHost->m_Language->TradeHackDetected( player->GetName( ) ) );
            m_PairedBanAdds.push_back( PairedBanAdd( string( ), m_GHost->m_DB->ThreadedBanAdd( player->GetJoinedRealm( ), player->GetName( ), player->GetExternalIPString( ), m_GameName, "AUTO BAN" , "[AUTOBAN] Tradehack detected" ) ) );
            player->SetDeleteMe( true );
            player->SetLeftReason( m_GHost->m_Language->WasKickedByPlayer( "Anti-tradehack" ) );
            player->SetLeftCode( PLAYERLEAVE_LOST );
        }
    }
}

bool CGame :: EventPlayerBotCommand( CGamePlayer *player, string command, string payload )
{
    bool HideCommand = CBaseGame :: EventPlayerBotCommand( player, command, payload );
    bool ping_done = false;

    // todotodo: don't be lazy

    string User = player->GetName( );
    string Command = command;
    string Payload = payload;

    bool AdminCheck = false;

    for ( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
    {
        if ( (*i)->GetServer( ) == player->GetSpoofedRealm( ) && (*i)->IsAdmin( User ) )
        {
            AdminCheck = true;
            break;
        }
    }

    bool RootAdminCheck = false;

    for ( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
    {
        if ( (*i)->GetServer( ) == player->GetSpoofedRealm( ) && (*i)->IsRootAdmin( User ) )
        {
            RootAdminCheck = true;
            break;
        }
    }

	bool CommandExecuted = false;
	bool NonLockableCommandExecuted = true;
	
    if ( player->GetSpoofed( ) && ( AdminCheck || RootAdminCheck || IsOwner( User ) ) )
    {
		CommandExecuted = true;
		
        CONSOLE_Print( "[GAME: " + m_GameName + "] admin [" + User + "] sent command [" + Command + "] with payload [" + Payload + "]" );
        
		//
		// !ADMINCHAT by Zephyrix improved by Metal_Koola
		//
		if ( Command == "ac" && !Payload.empty( ) )
		{
			for ( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
			{
				if ((*i)->GetSpoofed( ))
				{
					bool IsAdmin = false;
					string Name = (*i)->GetName( );
					for ( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); ++j )
					{
						if ( (*j)->GetServer( ) == player->GetSpoofedRealm( ) && ( (*j)->IsRootAdmin( Name ) || (*j)->IsAdmin( Name ) ) )
						{
							IsAdmin = true;
							break;
						}
					}

					if ( IsAdmin )
						SendChat( player->GetPID( ), (*i)->GetPID( ), "[ACHAT]-[" + User + "]: " + Payload );
				}
			}
			HideCommand = true;
		}
		else
		{
			CommandExecuted = false;
			NonLockableCommandExecuted = false;
		}
		if ( !CommandExecuted && (!m_Locked || RootAdminCheck || IsOwner( User ) ) )
        {
			CommandExecuted = true; // Goes to false if no command is executed. ( final else )
            /**************************
            * LOCKABLE ADMIN COMMANDS *
            ***************************/
            
            //
            // !ABORT (abort countdown, this includes start, end and autoend countdown)
            // !A
            //

            // we use "!a" as an alias for abort because you don't have much time to abort the countdown so it's useful for the abort command to be easy to type

            if ( Command == "abort" || Command == "a" )
            {
				if ( m_CountDownStarted && !m_GameLoading && !m_GameLoaded ) // abort start
				{
					SendAllChat( m_GHost->m_Language->CountDownAborted( ) );
					m_CountDownStarted = false;
				}
				else if ( m_GameEndCountDownStarted ) // abort end or autoend
				{
					if ( m_AutoEnded )
					{
						m_AutoEnd = false;
						m_AutoEnded = false;
					}
					CONSOLE_Print( "[GAME: " + m_GameName + "] game end aborted." );
					SendAllChat( m_GHost->m_Language->AdminStoppedEndCountdown( ) );
					m_GameEndCountDownStarted = false;
				}
            }

			/*/
			// !Rate
			//

			else if( Command == "rate" )
			{
			uint32_t elo;
			elo = player->GetScore();

			SendChat(player, "Your ELO is " + UTIL_ToString( elo ));
			}
			//
			// !Balance
			//

			else if ( ( Command == "bal" || Command == "balance" ) && !m_GameLoaded )
			{
				BalanceSlots( );
			}

			//
			// !Team Ratio
			//

			else if( Command == "teamratio" || Command == "tr" )
			{
				unsigned char playerSID, team;
				double team1score, team2score;
				uint32_t team2;
				uint32_t team1;

				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				
				{
					playerSID = GetSIDFromPID( (*i)->GetPID( ) );
					team = m_Slots[playerSID].GetTeam( );

					if( team == 0 )
					{
						team1score += (*i)->GetScore( );
						team1 = team1score;
					}
					else
					{
						team2score += (*i)->GetScore( );
						team2 = team2score;
					}
				}

				SendAllChat ( "Team Ratio: Sentinel == " + UTIL_ToString(team1) + " vs Scourge == " + UTIL_ToString(team2) );
			}*/

			//
			// !SGP
			//

			if( ( Command == "showgproxy" || Command == "sgp" ) )
			{
				string UsingGProxy;
				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				{
					if( (*i)->GetGProxy( ) )
					{
						if( UsingGProxy.empty( ) )
							UsingGProxy = (*i)->GetName( );
						else
							UsingGProxy += ", " + (*i)->GetName( );
					}
				}
				if( Payload.empty( ) )
				{
					if( UsingGProxy.empty( ) )
						SendAllChat( "No one is using GProxy++" );
					else
						SendAllChat( "Players using GProxy++: " + UsingGProxy );
				}
				else SendChat( player, "The command is .showgproxy or .sgp" );
			}

			//
			// SameIp
			//

			else if( Command == "sameip" || Command == "sip" )
			{
				SendAllChat( "Multiple IP address usage result" ); //TODO-TODO Language
				SendAllChat( "=====================================" );
						bool found = false;
				vector<bool> CheckedIp(m_Players.size(), false);
				string result;
				string s;
						for (size_t i = 0; i < m_Players.size(); i++) {
					if (CheckedIp[i])
						continue;
					CheckedIp[i] = true;
					s = m_Players[i]->GetName();
					bool found2 = false;
					for (size_t j = 0; j < m_Players.size(); j++) {
						if (!CheckedIp[j] && m_Players[i]->GetExternalIPString() == m_Players[j]->GetExternalIPString()) {
							s += ", " + m_Players[j]->GetName();
							CheckedIp[j] = true;
							found2 = true;
						}
					}
					if (found2) {
						result += "(" + s + ") ";
						found = true;
					}
				}
				if (found) {
					SendAllChat(result);
				} else {
					SendAllChat("No matches.");
				}
			}
			//
			// !NORESERVER !NR
			//
			else if ( (Command == "nr" || Command == "noreserved") && Payload.empty() && !m_GameLoaded )
			{
				m_ReserveAdmins = !m_ReserveAdmins;
				if ( m_ReserveAdmins == true )
					SendAllChat ("Reservando slots para administradores"); //TODO-TODO Language
				else
					SendAllChat ("Sin reservas de slots para administradores");
			}

			
			//
			// !BANS
			//

			else if ( Command == "bans" && !m_GameLoaded )
			{
				if ( Payload.empty() )
					m_KickBanned = !m_KickBanned;
				else if ( Payload == "on" )
					m_KickBanned = true;
				else if ( Payload == "off" )
					m_KickBanned = false;

				if ( m_KickBanned == true )
					SendAllChat ("Banned players kick enabled"); //TODO-TODO Language
				else
					SendAllChat ("Banned players kick disabled");
			}

			//
			// !Downloads
			//

			else if ( (Command == "downloads" || Command == "dls") && !m_GameLoaded )
			{
				if ( RootAdminCheck )
				{
					bool error = false;
					if( Payload.empty() )
					{
						if( m_Downloads == 0 )
							m_Downloads = 1;
						else
							m_Downloads = 0;
					}
					else
					{
						uint32_t dl = UTIL_ToUInt32( Payload );
						if ( dl < 0 || dl > 2 )
							error = true;
						else
							m_Downloads = dl;
					}

					if( !error )
					{
						if ( m_Downloads == 0 )
						{
							SendAllChat ( m_GHost->m_Language->MapDownloadsDisabled( ) );
							m_Downloads = 0;
						}
						else if ( m_Downloads == 1 )
						{
							SendAllChat ( m_GHost->m_Language->MapDownloadsEnabled( ) );
							m_Downloads = 1;
						}
					}
				}
				else
					SendAllChat ( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ) );
			}
			
            //
            // !ADDBAN
            // !BAN
            //

            else if ( ( Command == "addban" || Command == "ban" || Command == "b" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
            {
                // extract the victim and the reason
                // e.g. "Varlock leaver after dying" -> victim: "Varlock", reason: "leaver after dying"

                string Victim;
                string Reason;
                stringstream SS;
                SS << Payload;
                SS >> Victim;

                if ( !SS.eof( ) )
                {
                    getline( SS, Reason );
                    string :: size_type Start = Reason.find_first_not_of( " " );

                    if ( Start != string :: npos )
                        Reason = Reason.substr( Start );
                }

				if( (m_GHost->m_RequireBanReason) && Reason.empty( ) )
				{
					SendAllChat( m_GHost->m_Language->RequireBanReason( Victim ) );
				}
                else if ( m_GameLoaded )
                {
                    string VictimLower = Victim;
                    transform( VictimLower.begin( ), VictimLower.end( ), VictimLower.begin( ), (int(*)(int))tolower );
                    uint32_t Matches = 0;
                    CDBBan *LastMatch = NULL;

                    // try to match each player with the passed string (e.g. "Varlock" would be matched with "lock")
                    // we use the m_DBBans vector for this in case the player already left and thus isn't in the m_Players vector anymore

                    for ( vector<CDBBan *> :: iterator i = m_DBBans.begin( ); i != m_DBBans.end( ); ++i )
                    {
                        string TestName = (*i)->GetName( );
                        transform( TestName.begin( ), TestName.end( ), TestName.begin( ), (int(*)(int))tolower );

                        if ( TestName.find( VictimLower ) != string :: npos )
                        {
                            Matches++;
                            LastMatch = *i;

                            // if the name matches exactly stop any further matching

                            if ( TestName == VictimLower )
                            {
                                Matches = 1;
                                break;
                            }
                        }
                    }

                    if ( Matches == 0 )
                        SendAllChat( m_GHost->m_Language->UnableToBanNoMatchesFound( Victim ) );
                    else if ( Matches == 1 )
                    {
                        bool isAdmin = false;
						string server;
                        for ( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
                        {
                            if ( LastMatch->GetServer( ) == (*j)->GetServer( ) )
                            {
                                if ( (*j)->IsAdmin( LastMatch->GetName( ) ) || (*j)->IsRootAdmin( LastMatch->GetName( ) ) )
                                    isAdmin = true;
									server = (*j)->GetServerAlias( );
                                break;
                            }

                        }
                        if ( isAdmin )
                            SendChat( player, m_GHost->m_Language->ErrorBanningAdmin( ) );
                        else
                            m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( LastMatch->GetServer( ), LastMatch->GetName( ), LastMatch->GetIP( ), m_GameName, User, Reason ) ) );
							SendAllChat( m_GHost->m_Language->PlayerWasBannedByPlayer( server, LastMatch->GetName( ), User ) );
							SendAllChat ("Ban Reason: " + Reason);
                    }
                    else
                        SendAllChat( m_GHost->m_Language->UnableToBanFoundMoreThanOneMatch( Victim ) );
                }
                else
                {
                    CGamePlayer *LastMatch = NULL;
                    uint32_t Matches = GetPlayerFromNamePartial( Victim, &LastMatch );

                    if ( Matches == 0 )
                        SendAllChat( m_GHost->m_Language->UnableToBanNoMatchesFound( Victim ) );
                    else if ( Matches == 1 )
                    {
                        bool isAdmin = false;
						string server;
                        for ( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
                        {
                            if ( LastMatch->GetJoinedRealm( ) == (*j)->GetServer( ) )
                            {
                                if ( (*j)->IsAdmin( LastMatch->GetName( ) ) || (*j)->IsRootAdmin( LastMatch->GetName( ) ) )
                                    isAdmin = true;
									server = (*j)->GetServerAlias( );
                                break;
                            }

                        }
                        if ( isAdmin )
                            SendChat( player, m_GHost->m_Language->ErrorBanningAdmin( ) );
                        else
                            m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( LastMatch->GetJoinedRealm( ), LastMatch->GetName( ), LastMatch->GetExternalIPString( ), m_GameName, User, Reason ) ) );
							SendAllChat( m_GHost->m_Language->PlayerWasBannedByPlayer( server, LastMatch->GetName( ), User ) );
							SendAllChat ("Ban Reason: " + Reason);
                    }
                    else
                        SendAllChat( m_GHost->m_Language->UnableToBanFoundMoreThanOneMatch( Victim ) );
                }
            }

            //
            // !ANNOUNCE
            //

            else if ( ( Command == "announce" || Command == "ann" ) && !m_CountDownStarted )
            {
                if ( Payload.empty( ) || Payload == "off" )
                {
                    SendAllChat( m_GHost->m_Language->AnnounceMessageDisabled( ) );
                    SetAnnounce( 0, string( ) );
                }
                else
                {
                    // extract the interval and the message
                    // e.g. "30 hello everyone" -> interval: "30", message: "hello everyone"

                    uint32_t Interval;
                    string Message;
                    stringstream SS;
                    SS << Payload;
                    SS >> Interval;

                    if ( SS.fail( ) || Interval == 0 )
                        CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to announce command" );
                    else
                    {
                        if ( SS.eof( ) )
                            CONSOLE_Print( "[GAME: " + m_GameName + "] missing input #2 to announce command" );
                        else
                        {
                            getline( SS, Message );
                            string :: size_type Start = Message.find_first_not_of( " " );

                            if ( Start != string :: npos )
                                Message = Message.substr( Start );


                            SendAllChat( m_GHost->m_Language->AnnounceMessageEnabled( ) );
                            SetAnnounce( Interval, Message );
                        }
                    }
                }
            }

            //
            // !AUTOSAVE
            //

            else if ( Command == "autosave" )
            {
                if ( Payload == "on" )
                {
                    SendAllChat( m_GHost->m_Language->AutoSaveEnabled( ) );
                    m_AutoSave = true;
                }
                else if ( Payload == "off" )
                {
                    SendAllChat( m_GHost->m_Language->AutoSaveDisabled( ) );
                    m_AutoSave = false;
                }
            }

            //
            // !AUTOSTART
            //

            else if ( Command == "autostart" && !m_CountDownStarted )
            {
                if ( Payload.empty( ) || Payload == "off" )
                {
                    SendAllChat( m_GHost->m_Language->AutoStartDisabled( ) );
                    m_AutoStartPlayers = 0;
                }
                else
                {
                    uint32_t AutoStartPlayers = UTIL_ToUInt32( Payload );

                    if ( AutoStartPlayers != 0 )
                    {
                        SendAllChat( m_GHost->m_Language->AutoStartEnabled( UTIL_ToString( AutoStartPlayers ) ) );
                        m_AutoStartPlayers = AutoStartPlayers;
                    }
                }
            }

            //
            // !BANLAST !BL
            //

            else if ( ( Command == "banlast" || Command == "bl" ) && m_GameLoaded && !m_GHost->m_BNETs.empty( ) && m_DBBanLast )
            {
                bool isAdmin = false;
				string server;
                for ( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
                {
                    if ( m_DBBanLast->GetServer( ) == (*j)->GetServer( ) )
                    {
						server = (*j)->GetServerAlias( );
                        if ( (*j)->IsAdmin( m_DBBanLast->GetName( ) ) || (*j)->IsRootAdmin( m_DBBanLast->GetName( ) ) )
                            isAdmin = true;
                        break;
                    }

                }
                if ( isAdmin )
				{
					SendChat( player, m_GHost->m_Language->ErrorBanningAdmin( ) );
				}
				else if( (m_GHost->m_RequireBanReason) && Payload.empty( ) )
				{
					SendAllChat( m_GHost->m_Language->RequireBanReason( User ) );
				}
                else
				{
					m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( m_DBBanLast->GetServer( ), m_DBBanLast->GetName( ), m_DBBanLast->GetIP( ), m_GameName, User, Payload ) ) );
					SendAllChat( m_GHost->m_Language->PlayerWasBannedByPlayer( server, m_DBBanLast->GetName( ), User ) );
					SendAllChat ("Ban Reason: " + Payload);
				}
            }

            //
            // !CHECK
            //

            else if ( Command == "check" )
            {
                if ( !Payload.empty( ) )
                {
                    CGamePlayer *LastMatch = NULL;
                    uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

                    if ( Matches == 0 )
                        SendAllChat( m_GHost->m_Language->UnableToCheckPlayerNoMatchesFound( Payload ) );
                    else if ( Matches == 1 )
                    {
                        bool LastMatchAdminCheck = false;

                        for ( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
                        {
                            if ( (*i)->GetServer( ) == LastMatch->GetSpoofedRealm( ) && (*i)->IsAdmin( LastMatch->GetName( ) ) )
                            {
                                LastMatchAdminCheck = true;
                                break;
                            }
                        }

                        bool LastMatchRootAdminCheck = false;

                        for ( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
                        {
                            if ( (*i)->GetServer( ) == LastMatch->GetSpoofedRealm( ) && (*i)->IsRootAdmin( LastMatch->GetName( ) ) )
                            {
                                LastMatchRootAdminCheck = true;
                                break;
                            }
                        }

                        SendAllChat( m_GHost->m_Language->CheckedPlayer( LastMatch->GetName( ), LastMatch->GetNumPings( ) > 0 ? UTIL_ToString( LastMatch->GetPing( m_GHost->m_LCPings ) ) + "ms" : "N/A", m_GHost->m_DBLocal->FromCheck( UTIL_ByteArrayToUInt32( LastMatch->GetExternalIP( ), true ) ), LastMatchAdminCheck || LastMatchRootAdminCheck ? "Yes" : "No", IsOwner( LastMatch->GetName( ) ) ? "Yes" : "No", LastMatch->GetSpoofed( ) ? "Yes" : "No", LastMatch->GetSpoofedRealm( ).empty( ) ? "N/A" : LastMatch->GetSpoofedRealm( ), LastMatch->GetReserved( ) ? "Yes" : "No" ) );
                    }
                    else
                        SendAllChat( m_GHost->m_Language->UnableToCheckPlayerFoundMoreThanOneMatch( Payload ) );
                }
                else
                    SendAllChat( m_GHost->m_Language->CheckedPlayer( User, player->GetNumPings( ) > 0 ? UTIL_ToString( player->GetPing( m_GHost->m_LCPings ) ) + "ms" : "N/A", m_GHost->m_DBLocal->FromCheck( UTIL_ByteArrayToUInt32( player->GetExternalIP( ), true ) ), AdminCheck || RootAdminCheck ? "Yes" : "No", IsOwner( User ) ? "Yes" : "No", player->GetSpoofed( ) ? "Yes" : "No", player->GetSpoofedRealm( ).empty( ) ? "N/A" : player->GetSpoofedRealm( ), player->GetReserved( ) ? "Yes" : "No" ) );
            }

            //
            // !CHECKBAN
            //

            else if ( ( Command == "checkban" || Command == "cb" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
            {
                for ( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
                    m_PairedBanChecks.push_back( PairedBanCheck( User, m_GHost->m_DB->ThreadedBanCheck( (*i)->GetServer( ), Payload, string( ) ) ) );
            }

            //
            // !CLEARHCL
            //

            else if ( Command == "clearhcl" && !m_CountDownStarted )
            {
                m_HCLCommandString.clear( );
                SendAllChat( m_GHost->m_Language->ClearingHCL( ) );
            }

            //
            // !CLOSE (close slot)
            //

            else if ( ( Command == "close" || Command == "c" ) && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
            {
                // close as many slots as specified, e.g. "5 10" closes slots 5 and 10

                stringstream SS;
                SS << Payload;

                while ( !SS.eof( ) )
                {
                    uint32_t SID;
                    SS >> SID;

                    if ( SS.fail( ) )
                    {
                        CONSOLE_Print( "[GAME: " + m_GameName + "] bad input to close command" );
                        break;
                    }
                    else
                        CloseSlot( (unsigned char)( SID - 1 ), true );
                }
            }

            //
            // !CLOSEALL
            //

            else if ( ( Command == "closeall" || Command == "ca" ) && !m_GameLoading && !m_GameLoaded )
                CloseAllSlots( );

            //
            // !COMP (computer slot)
            //

            else if ( Command == "comp" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded && !m_SaveGame )
            {
                // extract the slot and the skill
                // e.g. "1 2" -> slot: "1", skill: "2"

                uint32_t Slot;
                uint32_t Skill = 1;
                stringstream SS;
                SS << Payload;
                SS >> Slot;

                if ( SS.fail( ) )
                    CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to comp command" );
                else
                {
                    if ( !SS.eof( ) )
                        SS >> Skill;

                    if ( SS.fail( ) )
                        CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #2 to comp command" );
                    else
                        ComputerSlot( (unsigned char)( Slot - 1 ), (unsigned char)Skill, true );
                }
            }

            //
            // !COMPCOLOUR (computer colour change)
            //

            else if ( Command == "compcolour" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded && !m_SaveGame )
            {
                // extract the slot and the colour
                // e.g. "1 2" -> slot: "1", colour: "2"

                uint32_t Slot;
                uint32_t Colour;
                stringstream SS;
                SS << Payload;
                SS >> Slot;

                if ( SS.fail( ) )
                    CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to compcolour command" );
                else
                {
                    if ( SS.eof( ) )
                        CONSOLE_Print( "[GAME: " + m_GameName + "] missing input #2 to compcolour command" );
                    else
                    {
                        SS >> Colour;

                        if ( SS.fail( ) )
                            CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #2 to compcolour command" );
                        else
                        {
                            unsigned char SID = (unsigned char)( Slot - 1 );

                            if ( !( m_Map->GetMapOptions( ) & MAPOPT_FIXEDPLAYERSETTINGS ) && Colour < 12 && SID < m_Slots.size( ) )
                            {
                                if ( m_Slots[SID].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetComputer( ) == 1 )
                                    ColourSlot( SID, Colour );
                            }
                        }
                    }
                }
            }

            //
            // !COMPHANDICAP (computer handicap change)
            //

            else if ( Command == "comphandicap" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded && !m_SaveGame )
            {
                // extract the slot and the handicap
                // e.g. "1 50" -> slot: "1", handicap: "50"

                uint32_t Slot;
                uint32_t Handicap;
                stringstream SS;
                SS << Payload;
                SS >> Slot;

                if ( SS.fail( ) )
                    CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to comphandicap command" );
                else
                {
                    if ( SS.eof( ) )
                        CONSOLE_Print( "[GAME: " + m_GameName + "] missing input #2 to comphandicap command" );
                    else
                    {
                        SS >> Handicap;

                        if ( SS.fail( ) )
                            CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #2 to comphandicap command" );
                        else
                        {
                            unsigned char SID = (unsigned char)( Slot - 1 );

                            if ( !( m_Map->GetMapOptions( ) & MAPOPT_FIXEDPLAYERSETTINGS ) && ( Handicap == 50 || Handicap == 60 || Handicap == 70 || Handicap == 80 || Handicap == 90 || Handicap == 100 ) && SID < m_Slots.size( ) )
                            {
                                if ( m_Slots[SID].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetComputer( ) == 1 )
                                {
                                    m_Slots[SID].SetHandicap( (unsigned char)Handicap );
                                    SendAllSlotInfo( );
                                }
                            }
                        }
                    }
                }
            }

            //
            // !COMPRACE (computer race change)
            //

            else if ( Command == "comprace" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded && !m_SaveGame )
            {
                // extract the slot and the race
                // e.g. "1 human" -> slot: "1", race: "human"

                uint32_t Slot;
                string Race;
                stringstream SS;
                SS << Payload;
                SS >> Slot;

                if ( SS.fail( ) )
                    CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to comprace command" );
                else
                {
                    if ( SS.eof( ) )
                        CONSOLE_Print( "[GAME: " + m_GameName + "] missing input #2 to comprace command" );
                    else
                    {
                        getline( SS, Race );
                        string :: size_type Start = Race.find_first_not_of( " " );

                        if ( Start != string :: npos )
                            Race = Race.substr( Start );

                        transform( Race.begin( ), Race.end( ), Race.begin( ), (int(*)(int))tolower );
                        unsigned char SID = (unsigned char)( Slot - 1 );

                        if ( !( m_Map->GetMapOptions( ) & MAPOPT_FIXEDPLAYERSETTINGS ) && !( m_Map->GetMapFlags( ) & MAPFLAG_RANDOMRACES ) && SID < m_Slots.size( ) )
                        {
                            if ( m_Slots[SID].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetComputer( ) == 1 )
                            {
                                if ( Race == "human" )
                                {
                                    m_Slots[SID].SetRace( SLOTRACE_HUMAN | SLOTRACE_SELECTABLE );
                                    SendAllSlotInfo( );
                                }
                                else if ( Race == "orc" )
                                {
                                    m_Slots[SID].SetRace( SLOTRACE_ORC | SLOTRACE_SELECTABLE );
                                    SendAllSlotInfo( );
                                }
                                else if ( Race == "night elf" )
                                {
                                    m_Slots[SID].SetRace( SLOTRACE_NIGHTELF | SLOTRACE_SELECTABLE );
                                    SendAllSlotInfo( );
                                }
                                else if ( Race == "undead" )
                                {
                                    m_Slots[SID].SetRace( SLOTRACE_UNDEAD | SLOTRACE_SELECTABLE );
                                    SendAllSlotInfo( );
                                }
                                else if ( Race == "random" )
                                {
                                    m_Slots[SID].SetRace( SLOTRACE_RANDOM | SLOTRACE_SELECTABLE );
                                    SendAllSlotInfo( );
                                }
                                else
                                    CONSOLE_Print( "[GAME: " + m_GameName + "] unknown race [" + Race + "] sent to comprace command" );
                            }
                        }
                    }
                }
            }

            //
            // !COMPTEAM (computer team change)
            //

            else if ( Command == "compteam" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded && !m_SaveGame )
            {
                // extract the slot and the team
                // e.g. "1 2" -> slot: "1", team: "2"

                uint32_t Slot;
                uint32_t Team;
                stringstream SS;
                SS << Payload;
                SS >> Slot;

                if ( SS.fail( ) )
                    CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to compteam command" );
                else
                {
                    if ( SS.eof( ) )
                        CONSOLE_Print( "[GAME: " + m_GameName + "] missing input #2 to compteam command" );
                    else
                    {
                        SS >> Team;

                        if ( SS.fail( ) )
                            CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #2 to compteam command" );
                        else
                        {
                            unsigned char SID = (unsigned char)( Slot - 1 );

                            if ( !( m_Map->GetMapOptions( ) & MAPOPT_FIXEDPLAYERSETTINGS ) && Team < 12 && SID < m_Slots.size( ) )
                            {
                                if ( m_Slots[SID].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetComputer( ) == 1 )
                                {
                                    m_Slots[SID].SetTeam( (unsigned char)( Team - 1 ) );
                                    SendAllSlotInfo( );
                                }
                            }
                        }
                    }
                }
            }

            //
            // !DBSTATUS
            //

            else if ( Command == "dbstatus" )
                SendAllChat( m_GHost->m_DB->GetStatus( ) );

            //
            // !DOWNLOAD
            // !DL
            //

            else if ( ( Command == "download" || Command == "dl" ) && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
            {
                CGamePlayer *LastMatch = NULL;
                uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

                if ( Matches == 0 )
                    SendAllChat( m_GHost->m_Language->UnableToStartDownloadNoMatchesFound( Payload ) );
                else if ( Matches == 1 )
                {
                    if ( !LastMatch->GetDownloadStarted( ) && !LastMatch->GetDownloadFinished( ) )
                    {
                        unsigned char SID = GetSIDFromPID( LastMatch->GetPID( ) );

                        if ( SID < m_Slots.size( ) && m_Slots[SID].GetDownloadStatus( ) != 100 )
                        {
                            // inform the client that we are willing to send the map

                            CONSOLE_Print( "[GAME: " + m_GameName + "] map download started for player [" + LastMatch->GetName( ) + "]" );
                            Send( LastMatch, m_Protocol->SEND_W3GS_STARTDOWNLOAD( GetHostPID( ) ) );
                            LastMatch->SetDownloadAllowed( true );
                            LastMatch->SetDownloadStarted( true );
                            LastMatch->SetStartedDownloadingTicks( GetTicks( ) );
                        }
                    }
                }
                else
                    SendAllChat( m_GHost->m_Language->UnableToStartDownloadFoundMoreThanOneMatch( Payload ) );
            }

            //
            // !DROP
            //

            else if ( Command == "drop" && m_GameLoaded )
                StopLaggers( "lagged out (dropped by admin)" );

			//
            // !END
            //

            else if ( Command == "end" && m_GameLoaded && !m_GameEndCountDownStarted && m_GameOverTime == 0 )
            {
                CONSOLE_Print( "[GAME: " + m_GameName + "] is over (admin ended game)" );
                SendAllChat( m_GHost->m_Language->GameWillEndInSeconds( 5 ) );
                m_GameEndCountDownStarted = true;
                m_GameEndCountDownCounter = 5;
                m_GameEndLastCountDownTicks = GetTicks();
            }

            //
            // !ENDN
            //

            else if ( Command == "endn" && m_GameLoaded && m_GameOverTime == 0 )
            {
                CONSOLE_Print( "[GAME: " + m_GameName + "] is over (admin ended game)" );
                StopPlayers( "was disconnected ("+User+" ended game)" );
            }

            //
            // !FAKEPLAYER
            //

            else if ( ( Command == "fakeplayer" || Command == "fk" ) && !m_CountDownStarted )
            {
                if ( m_FakePlayerPID == 255 )
                    CreateFakePlayer( );
                else
                    DeleteFakePlayer( );
            }

            //
            // !FPPAUSE
            //

            else if ( Command == "fppause" && m_FakePlayerPID != 255 && m_GameLoaded )
            {
                BYTEARRAY CRC;
                BYTEARRAY Action;
                Action.push_back( 1 );
                m_Actions.push( new CIncomingAction( m_FakePlayerPID, CRC, Action ) );
            }

            //
            // !FPRESUME
            //

            else if ( Command == "fpresume" && m_FakePlayerPID != 255 && m_GameLoaded )
            {
                BYTEARRAY CRC;
                BYTEARRAY Action;
                Action.push_back( 2 );
                m_Actions.push( new CIncomingAction( m_FakePlayerPID, CRC, Action ) );
            }

            //
            // !FROM
            //

            else if ( Command == "from" || Command == "f" )
            {
                string Froms;

                for ( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
                {
                    // we reverse the byte order on the IP because it's stored in network byte order

                    Froms += (*i)->GetNameTerminated( );
                    Froms += ": (";
                    Froms += m_GHost->m_DBLocal->FromCheck( UTIL_ByteArrayToUInt32( (*i)->GetExternalIP( ), true ) );
                    Froms += ")";

                    if ( i != m_Players.end( ) - 1 )
                        Froms += ", ";

                    if ( ( m_GameLoading || m_GameLoaded ) && Froms.size( ) > 100 )
                    {
                        // cut the text into multiple lines ingame

                        SendAllChat( Froms );
                        Froms.clear( );
                    }
                }

                if ( !Froms.empty( ) )
                    SendAllChat( Froms );
            }

            //
            // !HCL
            //

            else if ( Command == "hcl" && !m_CountDownStarted )
            {
                if ( !Payload.empty( ) )
                {
                    if ( Payload.size( ) <= m_Slots.size( ) )
                    {
                        string HCLChars = "abcdefghijklmnopqrstuvwxyz0123456789 -=,.";

                        if ( Payload.find_first_not_of( HCLChars ) == string :: npos )
                        {
                            m_HCLCommandString = Payload;
                            SendAllChat( m_GHost->m_Language->SettingHCL( m_HCLCommandString ) );
                        }
                        else
                            SendAllChat( m_GHost->m_Language->UnableToSetHCLInvalid( ) );
                    }
                    else
                        SendAllChat( m_GHost->m_Language->UnableToSetHCLTooLong( ) );
                }
                else
                    SendAllChat( m_GHost->m_Language->TheHCLIs( m_HCLCommandString ) );
            }

            //
            // !HOLD (hold a slot for someone)
            //

            else if ( Command == "hold" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
            {
                // hold as many players as specified, e.g. "Varlock Kilranin" holds players "Varlock" and "Kilranin"

                stringstream SS;
                SS << Payload;

                while ( !SS.eof( ) )
                {
                    string HoldName;
                    SS >> HoldName;

                    if ( SS.fail( ) )
                    {
                        CONSOLE_Print( "[GAME: " + m_GameName + "] bad input to hold command" );
                        break;
                    }
                    else
                    {
                        SendAllChat( m_GHost->m_Language->AddedPlayerToTheHoldList( HoldName ) );
                        AddToReserved( HoldName );
                    }
                }
            }

            //
            // !KICK (kick a player)
            //

            else if ( ( Command == "kick" || Command == "k" ) && !Payload.empty( ) )
            {
                CGamePlayer *LastMatch = NULL;
                uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

                if ( Matches == 0 )
                    SendAllChat( m_GHost->m_Language->UnableToKickNoMatchesFound( Payload ) );
                else if ( Matches == 1 )
                {
                    LastMatch->SetDeleteMe( true );
                    LastMatch->SetLeftReason( m_GHost->m_Language->WasKickedByPlayer( User ) );

                    if ( !m_GameLoading && !m_GameLoaded )
                        LastMatch->SetLeftCode( PLAYERLEAVE_LOBBY );
                    else
                        LastMatch->SetLeftCode( PLAYERLEAVE_LOST );

                    if ( !m_GameLoading && !m_GameLoaded )
                        OpenSlot( GetSIDFromPID( LastMatch->GetPID( ) ), false );
                }
                else
                    SendAllChat( m_GHost->m_Language->UnableToKickFoundMoreThanOneMatch( Payload ) );
            }

            //
            // !LATENCY (set game latency)
            //

            else if ( Command == "latency" || Command == "dr" )
            {
                if ( Payload.empty( ) )
                    SendAllChat( m_GHost->m_Language->LatencyIs( UTIL_ToString( m_Latency ) ) );
                else
                {
                    m_Latency = UTIL_ToUInt32( Payload );

                    if ( m_Latency <= 20 )
                    {
                        m_Latency = 20;
                        SendAllChat( m_GHost->m_Language->SettingLatencyToMinimum( "20" ) );
                    }
                    else if ( m_Latency >= 500 )
                    {
                        m_Latency = 500;
                        SendAllChat( m_GHost->m_Language->SettingLatencyToMaximum( "500" ) );
                    }
                    else
                        SendAllChat( m_GHost->m_Language->SettingLatencyTo( UTIL_ToString( m_Latency ) ) );
                }
            }

            //
            // !LOCK
            //

            else if ( Command == "lock" && ( RootAdminCheck || IsOwner( User ) ) )
            {
                SendAllChat( m_GHost->m_Language->GameLocked( ) );
                m_Locked = true;
            }

            //
            // !MESSAGES
            //

            else if ( Command == "messages" )
            {
                if ( Payload == "on" )
                {
                    SendAllChat( m_GHost->m_Language->LocalAdminMessagesEnabled( ) );
                    m_LocalAdminMessages = true;
                }
                else if ( Payload == "off" )
                {
                    SendAllChat( m_GHost->m_Language->LocalAdminMessagesDisabled( ) );
                    m_LocalAdminMessages = false;
                }
            }

            //
            // !MUTE
            //

            else if ( Command == "mute" || Command == "m" )
            {
                CGamePlayer *LastMatch = NULL;
                uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

                if ( Matches == 0 )
                    SendAllChat( m_GHost->m_Language->UnableToMuteNoMatchesFound( Payload ) );
                else if ( Matches == 1 )
                {
                    SendAllChat( m_GHost->m_Language->MutedPlayer( LastMatch->GetName( ), User ) );
                    LastMatch->SetMuted( true );
                }
                else
                    SendAllChat( m_GHost->m_Language->UnableToMuteFoundMoreThanOneMatch( Payload ) );
            }

            //
            // !MUTEALL
            //

            else if ( ( Command == "muteall" || Command == "ma") && m_GameLoaded )
            {
				if ( Payload.empty( ) )
				{
					SendAllChat ( m_GHost->m_Language->GlobalChatMuted( ) );
					m_MuteAll = true;
				}
				else
				{
					CGamePlayer *LastMatch = NULL;
					uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

					if ( Matches == 0 )
					{
						SendAllChat( m_GHost->m_Language->UnableToMutePlayersAllNoMatchesFound( Payload ) );
					}
					else if ( Matches == 1 )
					{
						SendAllChat( m_GHost->m_Language->MutedPlayersAll( LastMatch->GetName( ), User ) );
						LastMatch->SetAllMuted( true );
					}
					else
					{
						SendAllChat( m_GHost->m_Language->UnableToMutePlayersAllFoundMoreThanOneMatch( Payload ) );
					}
				}
            }

            //
            // !ONLY GhostXS
            //

            else if ( Command == "only" && !m_GameLoading && !m_GameLoaded )
            {
                if ( Payload.empty( ) )
                {
                    SendAllChat( m_GHost->m_Language->CountryCheckDisabled( ) );
                    m_Countries_Allow = false;
                    m_Countries_Allowed = "";
                }
                else
                {
                    m_Countries_Allow = true;
                    m_Countries_Allowed = Payload;
                    transform( m_Countries_Allowed.begin( ), m_Countries_Allowed.end( ), m_Countries_Allowed.begin( ), (int(*)(int))toupper );
                    SendAllChat( m_GHost->m_Language->CountryCheckEnabled( m_Countries_Allowed) );
                    m_Countries_Allowed = m_Countries_Allowed + " ??";

                    for ( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
                    {
                        string From = m_GHost->m_DBLocal->FromCheck( UTIL_ByteArrayToUInt32( (*i)->GetExternalIP( ), true ) );
                        transform( From.begin( ), From.end( ), From.begin( ), (int(*)(int))toupper );

                        bool isAdmin = IsOwner((*i)->GetName( ));
                        for ( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
                        {
                            if ( (*j)->IsAdmin((*i)->GetName( ) ) || (*j)->IsRootAdmin( (*i)->GetName( ) ) )
                            {
                                isAdmin = true;
                                break;
                            }
                        }

                        if (IsReserved ((*i)->GetName()))

                            isAdmin = true;

                        if ( !isAdmin && (*i)->GetName( )!=User && m_Countries_Allowed.find(From)==string :: npos )
                        {
                            SendAllChat( m_GHost->m_Language->AutokickingPlayerForDeniedCountry( (*i)->GetName( ), From ) );
                            (*i)->SetDeleteMe( true );
                            (*i)->SetLeftReason( "was autokicked," + From + " not on the allowed countries list");
                            (*i)->SetLeftCode( PLAYERLEAVE_LOBBY );
                            OpenSlot( GetSIDFromPID( (*i)->GetPID( ) ), false );
                        }
                    }
                }
            }

            //
            // !OPEN (open slot)
            //

            else if ( ( Command == "open" || Command == "o" ) && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
            {
                // open as many slots as specified, e.g. "5 10" opens slots 5 and 10

                stringstream SS;
                SS << Payload;

                while ( !SS.eof( ) )
                {
                    uint32_t SID;
                    SS >> SID;

                    if ( SS.fail( ) )
                    {
                        CONSOLE_Print( "[GAME: " + m_GameName + "] bad input to open command" );
                        break;
                    }
                    else
                        OpenSlot( (unsigned char)( SID - 1 ), true );
                }
            }

            //
            // !OPENALL
            //

            else if ( ( Command == "openall" || Command == "oa" ) && !m_GameLoading && !m_GameLoaded )
                OpenAllSlots( );

            //
            // !OWNER (set game owner)
            //

            else if ( Command == "owner" )
            {
                if ( RootAdminCheck || IsOwner( User ) || !GetPlayerFromName( m_OwnerName, false ) )
                {
                    if ( !Payload.empty( ) )
                    {
                        string PlayerName = Payload; 
						CGamePlayer *LastMatch = NULL;
						uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );
						if (Matches == 1)
						{
							PlayerName = LastMatch->GetName();
							SendAllChat( m_GHost->m_Language->SettingGameOwnerTo( PlayerName ) );
							m_OwnerName = PlayerName;
						}
						else if ( Matches == 0 )
						{
							SendAllChat( m_GHost->m_Language->SettingGameOwnerTo( PlayerName ) );
							m_OwnerName = PlayerName;
						}
						else
						{
							SendAllChat( m_GHost->m_Language->UnableToTransferOwnershipFoundMoreThanOneMatch( ) );
						}
						
                    }
                    else
                    {
                        SendAllChat( m_GHost->m_Language->SettingGameOwnerTo( User ) );
                        m_OwnerName = User;
                    }
                }
                else
                    SendAllChat( m_GHost->m_Language->UnableToSetGameOwner( m_OwnerName ) );
            }

            //
            // !IPS
            //

            else if ( Command == "ips" )
            {
                string Froms;

                for ( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
                {
                    // we reverse the byte order on the IP because it's stored in network byte order

                    Froms += (*i)->GetName( );
                    Froms += ": (";
                    Froms += (*i)->GetExternalIPString( );
                    Froms += ")";

                    if ( i != m_Players.end( ) - 1 )
                        Froms += ", ";
                }

                SendAllChat( Froms );
            }

            //
            // !PING
            // !P
            //

            else if ( Command == "ping" || Command == "p" )
            {
                ping_done = true;
                // kick players with ping higher than payload if payload isn't empty
                // we only do this if the game hasn't started since we don't want to kick players from a game in progress

                uint32_t Kicked = 0;
                uint32_t KickPing = 0;
                string Pings;

                if ( !m_GameLoading && !m_GameLoaded && !Payload.empty( ) )
                    KickPing = UTIL_ToUInt32( Payload );

                if ( !Payload.empty() )
                {
                    CGamePlayer *LastMatch = NULL;
                    uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );

                    if ( Matches == 0 )
                        CONSOLE_Print("No matches");

                    else if ( Matches == 1 )
                    {
                        Pings = LastMatch->GetName( );
                        Pings +=": ";
                        if ( LastMatch->GetNumPings( ) > 0 )
                        {
                            Pings += UTIL_ToString( LastMatch->GetPing( m_GHost->m_LCPings ) );
                            Pings +=" ms";
                        }
                        else
                            Pings += "N/A";

                        Pings += " (";
                        Pings += ")";
                        SendAllChat(Pings);
						return HideCommand;
                    }
                    else
                        CONSOLE_Print("Found more than one match");
                }

                if ( !m_GameLoading && !m_GameLoaded && !Payload.empty( ) )
                    KickPing = UTIL_ToUInt32( Payload );

                // copy the m_Players vector so we can sort by descending ping so it's easier to find players with high pings

                vector<CGamePlayer *> SortedPlayers = m_Players;
                sort( SortedPlayers.begin( ), SortedPlayers.end( ), CGamePlayerSortDescByPing( ) );

                for ( vector<CGamePlayer *> :: iterator i = SortedPlayers.begin( ); i != SortedPlayers.end( ); ++i )
                {
                    Pings += (*i)->GetNameTerminated( );
                    Pings += ": ";

                    if ( (*i)->GetNumPings( ) > 0 )
                    {
                        Pings += UTIL_ToString( (*i)->GetPing( m_GHost->m_LCPings ) );

                        if ( !m_GameLoading && !m_GameLoaded && !(*i)->GetReserved( ) && KickPing > 0 && (*i)->GetPing( m_GHost->m_LCPings ) > KickPing )
                        {
                            (*i)->SetDeleteMe( true );
                            (*i)->SetLeftReason( "was kicked for excessive ping " + UTIL_ToString( (*i)->GetPing( m_GHost->m_LCPings ) ) + " > " + UTIL_ToString( KickPing ) );
                            (*i)->SetLeftCode( PLAYERLEAVE_LOBBY );
                            OpenSlot( GetSIDFromPID( (*i)->GetPID( ) ), false );
                            Kicked++;
                        }

                        Pings += "ms";
                    }
                    else
                        Pings += "N/A";

                    if ( i != SortedPlayers.end( ) - 1 )
                        Pings += ", ";

                    if ( ( m_GameLoading || m_GameLoaded ) && Pings.size( ) > 100 )
                    {
                        // cut the text into multiple lines ingame

                        SendAllChat( Pings );
                        Pings.clear( );
                    }
                }

                if ( !Pings.empty( ) )
                    SendAllChat( Pings );

                if ( Kicked > 0 )
                    SendAllChat( m_GHost->m_Language->KickingPlayersWithPingsGreaterThan( UTIL_ToString( Kicked ), UTIL_ToString( KickPing ) ) );
            }

            //
            // !PRIV (rehost as private game)
            //

            else if ( Command == "priv" && !Payload.empty( ) && !m_CountDownStarted && !m_SaveGame )
            {
                if ( Payload.length() < 31 )
                {
                    CONSOLE_Print( "[GAME: " + m_GameName + "] trying to rehost as private game [" + Payload + "]" );
                    SendAllChat( m_GHost->m_Language->TryingToRehostAsPrivateGame( Payload ) );
                    m_GameState = GAME_PRIVATE;
                    m_LastGameName = m_GameName;
                    m_GameName = Payload;
                    m_HostCounter = m_GHost->m_HostCounter++;
                    m_RefreshError = false;
                    m_RefreshRehosted = true;

                    for ( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
                    {
                        // unqueue any existing game refreshes because we're going to assume the next successful game refresh indicates that the rehost worked
                        // this ignores the fact that it's possible a game refresh was just sent and no response has been received yet
                        // we assume this won't happen very often since the only downside is a potential false positive

                        (*i)->UnqueueGameRefreshes( );
                        (*i)->QueueGameUncreate( );
                        (*i)->QueueEnterChat( );

                        // we need to send the game creation message now because private games are not refreshed

                        (*i)->QueueGameCreate( m_GameState, m_GameName, string( ), m_Map, NULL, m_HostCounter );

                        if ( (*i)->GetPasswordHashType( ) != "pvpgn" )
                            (*i)->QueueEnterChat( );
                    }

                    m_CreationTime = GetTime( );
                    m_LastRefreshTime = GetTime( );
                }
                else
                    SendAllChat( m_GHost->m_Language->UnableToCreateGameNameTooLong( Payload ) );
            }

            //
            // !PUB (rehost as public game)
            //

            else if ( Command == "pub" && !Payload.empty( ) && !m_CountDownStarted && !m_SaveGame )
            {
                if ( Payload.length() < 31 )
                {
                    CONSOLE_Print( "[GAME: " + m_GameName + "] trying to rehost as public game [" + Payload + "]" );
                    SendAllChat( m_GHost->m_Language->TryingToRehostAsPublicGame( Payload ) );
                    m_GameState = GAME_PUBLIC;
                    m_LastGameName = m_GameName;
                    m_GameName = Payload;
                    m_HostCounter = m_GHost->m_HostCounter++;
                    m_RefreshError = false;
                    m_RefreshRehosted = true;

                    for ( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
                    {
                        // unqueue any existing game refreshes because we're going to assume the next successful game refresh indicates that the rehost worked
                        // this ignores the fact that it's possible a game refresh was just sent and no response has been received yet
                        // we assume this won't happen very often since the only downside is a potential false positive

                        (*i)->UnqueueGameRefreshes( );
                        (*i)->QueueGameUncreate( );
                        (*i)->QueueEnterChat( );

                        // the game creation message will be sent on the next refresh
                    }

                    m_CreationTime = GetTime( );
                    m_LastRefreshTime = GetTime( );
                }
                else
                    SendAllChat( m_GHost->m_Language->UnableToCreateGameNameTooLong( Payload ) );
            }
            //
            // !REFRESH (turn on or off refresh messages)
            //

            else if ( ( Command == "refresh" || Command == "r" ) && !m_CountDownStarted )
            {
                if ( Payload.empty() )
                {
                    m_RefreshMessages = !m_RefreshMessages;
                    if (m_RefreshMessages == true )
                        SendAllChat (m_GHost->m_Language->RefreshMessagesEnabled() );
                    else
                        SendAllChat (m_GHost->m_Language->RefreshMessagesDisabled() );
                }
                else if ( Payload == "on" )
                {
                    SendAllChat( m_GHost->m_Language->RefreshMessagesEnabled( ) );
                    m_RefreshMessages = true;
                }
                else if ( Payload == "off" )
                {
                    SendAllChat( m_GHost->m_Language->RefreshMessagesDisabled( ) );
                    m_RefreshMessages = false;
                }
            }

            //
            // !SENDLAN
            //

            else if ( Command == "sendlan" && !Payload.empty( ) && !m_CountDownStarted )
            {
                // extract the ip and the port
                // e.g. "1.2.3.4 6112" -> ip: "1.2.3.4", port: "6112"

                string IP;
                uint32_t Port = 6112;
                stringstream SS;
                SS << Payload;
                SS >> IP;

                if ( !SS.eof( ) )
                    SS >> Port;

                if ( SS.fail( ) )
                    CONSOLE_Print( "[GAME: " + m_GameName + "] bad inputs to sendlan command" );
                else
                {
                    // construct a fixed host counter which will be used to identify players from this "realm" (i.e. LAN)
                    // the fixed host counter's 4 most significant bits will contain a 4 bit ID (0-15)
                    // the rest of the fixed host counter will contain the 28 least significant bits of the actual host counter
                    // since we're destroying 4 bits of information here the actual host counter should not be greater than 2^28 which is a reasonable assumption
                    // when a player joins a game we can obtain the ID from the received host counter
                    // note: LAN broadcasts use an ID of 0, battle.net refreshes use an ID of 1-10, the rest are unused

                    uint32_t FixedHostCounter = m_HostCounter & 0x0FFFFFFF;

                    // we send 12 for SlotsTotal because this determines how many PID's Warcraft 3 allocates
                    // we need to make sure Warcraft 3 allocates at least SlotsTotal + 1 but at most 12 PID's
                    // this is because we need an extra PID for the virtual host player (but we always delete the virtual host player when the 12th person joins)
                    // however, we can't send 13 for SlotsTotal because this causes Warcraft 3 to crash when sharing control of units
                    // nor can we send SlotsTotal because then Warcraft 3 crashes when playing maps with less than 12 PID's (because of the virtual host player taking an extra PID)
                    // we also send 12 for SlotsOpen because Warcraft 3 assumes there's always at least one player in the game (the host)
                    // so if we try to send accurate numbers it'll always be off by one and results in Warcraft 3 assuming the game is full when it still needs one more player
                    // the easiest solution is to simply send 12 for both so the game will always show up as (1/12) players

                    if ( m_SaveGame )
                    {
                        // note: the PrivateGame flag is not set when broadcasting to LAN (as you might expect)

                        uint32_t MapGameType = MAPGAMETYPE_SAVEDGAME;
                        BYTEARRAY MapWidth;
                        MapWidth.push_back( 0 );
                        MapWidth.push_back( 0 );
                        BYTEARRAY MapHeight;
                        MapHeight.push_back( 0 );
                        MapHeight.push_back( 0 );
                        m_GHost->m_UDPSocket->SendTo( IP, Port, m_Protocol->SEND_W3GS_GAMEINFO( m_GHost->m_TFT, m_GHost->m_LANWar3Version, UTIL_CreateByteArray( MapGameType, false ), m_Map->GetMapGameFlags( ), MapWidth, MapHeight, m_GameName, "Varlock", GetTime( ) - m_CreationTime, "Save\\Multiplayer\\" + m_SaveGame->GetFileNameNoPath( ), m_SaveGame->GetMagicNumber( ), 12, 12, m_HostPort, FixedHostCounter, m_EntryKey ) );
                    }
                    else
                    {
                        // note: the PrivateGame flag is not set when broadcasting to LAN (as you might expect)
                        // note: we do not use m_Map->GetMapGameType because none of the filters are set when broadcasting to LAN (also as you might expect)

                        uint32_t MapGameType = MAPGAMETYPE_UNKNOWN0;
                        m_GHost->m_UDPSocket->SendTo( IP, Port, m_Protocol->SEND_W3GS_GAMEINFO( m_GHost->m_TFT, m_GHost->m_LANWar3Version, UTIL_CreateByteArray( MapGameType, false ), m_Map->GetMapGameFlags( ), m_Map->GetMapWidth( ), m_Map->GetMapHeight( ), m_GameName, "Varlock", GetTime( ) - m_CreationTime, m_Map->GetMapPath( ), m_Map->GetMapCRC( ), 12, 12, m_HostPort, FixedHostCounter, m_EntryKey ) );
                    }
                }
            }
            			
			//
			// !Servers
			// !sv
			// !realm
			//
			
			else if( Command == "servers" || Command == "sv" || Command == "realm" )
			{
				string Froms;
				string Froms2;
				string SNL;
				string SN;
				bool samerealm=true;
				if( !Payload.empty( ) )
				{
					CGamePlayer *LastMatch = NULL;
					uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );

					if( Matches == 0 )
						CONSOLE_Print("No matches");

					else if( Matches == 1 )
					{
						Froms = LastMatch->GetName( );
						Froms += ": (";
						SN = LastMatch->GetJoinedRealm( );
						if( SN.empty( ) )
							SN = "LAN";
						else
							for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); ++j )
							{
								if( (*j)->GetServer( ) == SN )
								{
									SN = (*j)->GetServerAlias( );
									break;
								}
							}
						Froms += SN;
						Froms += ")";
						SendAllChat(Froms);
					}
					else
						CONSOLE_Print("Found more than one match");
				}
				else
				{
					for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
					{
						// we reverse the byte order on the IP because it's stored in network byte order

						Froms2 += (*i)->GetName( );
						Froms += (*i)->GetName( );
						Froms += ": (";
						SN = (*i)->GetJoinedRealm( );
						if( SN.empty( ) )
							SN = "LAN";
						else
							for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); ++j )
							{
								if( (*j)->GetServer( ) == SN )
								{
									SN = (*j)->GetServerAlias( );
									break;
								}
							}
						Froms += SN;
						Froms += ")";

						if (SNL=="")
							SNL=SN;
						else if (SN!=SNL)
							samerealm=false;

						if( i != m_Players.end( ) - 1 )
						{
							Froms += ", ";
							Froms2 += ", ";
						}
					}
					Froms2 += " "+ m_GHost->m_Language->PlayersAreFromServer( SNL );

					if (samerealm)
						SendAllChat( Froms2 );
					else
						SendAllChat( Froms );
				}
			}

            //
            // !SP
            //

            else if ( Command == "sp" && !m_CountDownStarted )
            {
                SendAllChat( m_GHost->m_Language->ShufflingPlayers( ) );
                ShuffleSlots( );
            }

            //
            // !START
            //

            else if ( Command == "start" && !m_CountDownStarted )
            {
                // if the player sent "!start force" skip the checks and start the countdown
                // otherwise check that the game is ready to start

                if ( Payload == "force" )
                    StartCountDown( true );
                else
                {
                    if ( GetTicks( ) - m_LastPlayerLeaveTicks >= 2000 )
                        StartCountDown( false );
                    else
                        SendAllChat( m_GHost->m_Language->CountDownAbortedSomeoneLeftRecently( ) );
                }
            }

            //
            // !STARTN
            //

            else if ( Command == "startn" && !m_CountDownStarted )
            {
				bool spoofed;
				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				{
					if( (*i)->GetSpoofed( ) )
					{
						spoofed = (*i)->GetSpoofed( );
					}
				}	
				if ( GetTicks( ) - m_LastPlayerLeaveTicks < 1000 )
				{
					SendAllChat( m_GHost->m_Language->CountDownAbortedSomeoneLeftRecently( ) );
				}
				else if ( spoofed )
				{
					StartCountDown( true );
					m_CountDownCounter = 0;
				}
				else
				{
					SendAllChat( "All players must be spoofchecked");
				}
            }

            //
            // !SWAP (swap slots)
            //

            else if ( Command == "swap" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
            {
                uint32_t SID1;
                uint32_t SID2;
                stringstream SS;
                SS << Payload;
                SS >> SID1;

                if ( SS.fail( ) )
                    CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to swap command" );
                else
                {
                    if ( SS.eof( ) )
                        CONSOLE_Print( "[GAME: " + m_GameName + "] missing input #2 to swap command" );
                    else
                    {
                        SS >> SID2;

                        if ( SS.fail( ) )
                            CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #2 to swap command" );
                        else
                            SwapSlots( (unsigned char)( SID1 - 1 ), (unsigned char)( SID2 - 1 ) );
                    }
                }
            }

            //
            // !SYNCLIMIT
            //

            else if ( Command == "synclimit" || Command == "s" )
            {
                if ( Payload.empty( ) )
                    SendAllChat( m_GHost->m_Language->SyncLimitIs( UTIL_ToString( m_SyncLimit ) ) );
                else
                {
                    m_SyncLimit = UTIL_ToUInt32( Payload );

                    if ( m_SyncLimit <= 10 || m_SyncLimit >= 10000 )
                    {
                        m_SyncLimit = 10000;
                        SendAllChat( m_GHost->m_Language->SettingSyncLimitToMaximum( "10000" ) );
                    }
                    else
                        SendAllChat( m_GHost->m_Language->SettingSyncLimitTo( UTIL_ToString( m_SyncLimit ) ) );
                }
            }

            //
            // !UNHOST
            //

            else if ( Command == "unhost" && !m_CountDownStarted )
                m_Exiting = true;

            //
            // !UNLOCK
            //

            else if ( Command == "unlock" && ( RootAdminCheck || IsOwner( User ) ) )
            {
                SendAllChat( m_GHost->m_Language->GameUnlocked( ) );
                m_Locked = false;
            }

            //
            // !UNMUTE
            //

            else if ( Command == "unmute" || Command == "um" )
            {
                CGamePlayer *LastMatch = NULL;
                uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

                if ( Matches == 0 )
                    SendAllChat( m_GHost->m_Language->UnableToMuteNoMatchesFound( Payload ) );
                else if ( Matches == 1 )
                {
                    SendAllChat( m_GHost->m_Language->UnmutedPlayer( LastMatch->GetName( ), User ) );
                    LastMatch->SetMuted( false );
                }
                else
                    SendAllChat( m_GHost->m_Language->UnableToMuteFoundMoreThanOneMatch( Payload ) );
            }

            //
            // !UNMUTEALL
            //

            else if ( ( Command == "unmuteall" || Command == "uma" ) && m_GameLoaded )
            {
				if (Payload.empty ( ) )
				{
					SendAllChat ( m_GHost->m_Language->GlobalChatUnmuted( ) );
					m_MuteAll = false;
				}
				else
				{
					CGamePlayer *LastMatch = NULL;
					uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );
					if ( Matches == 0 )
						SendAllChat( m_GHost->m_Language->UnableToMutePlayersAllNoMatchesFound( Payload ) );
					else if ( Matches == 1 )
					{
					SendAllChat( m_GHost->m_Language->UnmutedPlayersAll( LastMatch->GetName( ), User ) );
					LastMatch->SetAllMuted( false );
					}
					else
					SendAllChat( m_GHost->m_Language->UnableToMutePlayersAllFoundMoreThanOneMatch( Payload ) );
				}
            }



            //
            // !VERBOSE
            // !VB
            //

            else if ( Command == "verbose" || Command == "vb" )
            {
                m_GHost->m_Verbose = !m_GHost->m_Verbose;
                if (m_GHost->m_Verbose)
                    SendAllChat( "Verbose ON" );
                else
                    SendAllChat( "Verbose OFF" );
            }

            //
            // !VIRTUALHOST
            //

            else if ( Command == "virtualhost" && !Payload.empty( ) && Payload.size( ) <= 15 && !m_CountDownStarted )
            {
                DeleteVirtualHost( );
                m_VirtualHostName = Payload;
            }

            //
            // !VOTECANCEL
            //

            else if ( Command == "votecancel" && !m_KickVotePlayer.empty( ) )
            {
                SendAllChat( m_GHost->m_Language->VoteKickCancelled( m_KickVotePlayer ) );
                m_KickVotePlayer.clear( );
                m_StartedKickVoteTime = 0;
			}
			else
			{
				CommandExecuted = false;
			}
        }
        else if ( CommandExecuted && !NonLockableCommandExecuted )
        {
            CONSOLE_Print( "[GAME: " + m_GameName + "] admin command ignored, the game is locked" );
            SendChat( player, m_GHost->m_Language->TheGameIsLocked( ) );
        }
    }
    else
    {
        if ( !player->GetSpoofed( ) )
            CONSOLE_Print( "[GAME: " + m_GameName + "] non-spoofchecked user [" + User + "] sent command [" + Command + "] with payload [" + Payload + "]" );
        else
            CONSOLE_Print( "[GAME: " + m_GameName + "] non-admin [" + User + "] sent command [" + Command + "] with payload [" + Payload + "]" );
    }

    /*********************
    * NON ADMIN COMMANDS *
    *********************/
	if ( !CommandExecuted )
	{
		CommandExecuted = true;
		//
		// !CHECKME
		//
		if ( Command == "checkme" )
			SendChat( player, m_GHost->m_Language->CheckedPlayer( User, player->GetNumPings( ) > 0 ? UTIL_ToString( player->GetPing( m_GHost->m_LCPings ) ) + "ms" : "N/A", m_GHost->m_DBLocal->FromCheck( UTIL_ByteArrayToUInt32( player->GetExternalIP( ), true ) ), AdminCheck || RootAdminCheck ? "Yes" : "No", IsOwner( User ) ? "Yes" : "No", player->GetSpoofed( ) ? "Yes" : "No", player->GetSpoofedRealm( ).empty( ) ? "N/A" : player->GetSpoofedRealm( ), player->GetReserved( ) ? "Yes" : "No" ) );

		//
		// !GN !GAMENAME
		//

		else if ( Command == "gn" || Command == "gamename" )
			SendAllChat( "Game Name: [ " +  m_GameName + " ]");

		//
		// !PING !P
		//

		else if ( ( Command == "ping" || Command == "p" ) && !ping_done )
			SendChat( player, player->GetNumPings( ) > 0 ? UTIL_ToString( player->GetPing( m_GHost->m_LCPings ) ) + "ms" : "N/A" );

		//
		// !ROLL
		//

		else if ( Command == "roll" )
		{
			int RandomNumber;
			int max = Payload.empty()? 100 : UTIL_ToUInt32( Payload );
			srand((unsigned)time(0));
			RandomNumber = (rand()%(max-1))+1;
			SendAllChat(User + " rolled "+UTIL_ToString(RandomNumber) + " of " + UTIL_ToString( max ) );
		}

		//
		// !STATS
		//

		else if ( Command == "stats" && GetTime( ) - player->GetStatsSentTime( ) >= 5 )
		{
			string StatsUser = User;

			if ( !Payload.empty( ) )
				StatsUser = Payload;

			if ( player->GetSpoofed( ) && ( AdminCheck || RootAdminCheck || IsOwner( User ) ) )
				m_PairedGPSChecks.push_back( PairedGPSCheck( string( ), m_GHost->m_DB->ThreadedGamePlayerSummaryCheck( StatsUser ) ) );
			else
				m_PairedGPSChecks.push_back( PairedGPSCheck( User, m_GHost->m_DB->ThreadedGamePlayerSummaryCheck( StatsUser ) ) );

			player->SetStatsSentTime( GetTime( ) );
		}

		//
		// !STATSDOTA
		// !SD
		//

		else if ( (Command == "statsdota" || Command == "sd") && GetTime( ) - player->GetStatsDotASentTime( ) >= 5 )
		{
			string StatsUser = User;

			if ( !Payload.empty( ) )
				StatsUser = Payload;

			if ( player->GetSpoofed( ) && ( AdminCheck || RootAdminCheck || IsOwner( User ) ) )
				m_PairedDPSChecks.push_back( PairedDPSCheck( string( ), m_GHost->m_DB->ThreadedDotAPlayerSummaryCheck( StatsUser ) ) );
			else
				m_PairedDPSChecks.push_back( PairedDPSCheck( User, m_GHost->m_DB->ThreadedDotAPlayerSummaryCheck( StatsUser ) ) );

			player->SetStatsDotASentTime( GetTime( ) );
		}

		//
		// !VERSION !V
		//

		else if ( Command == "version" || Command == "v" )
		{
			if ( player->GetSpoofed( ) && ( AdminCheck || RootAdminCheck || IsOwner( User ) ) )
				SendChat( player, m_GHost->m_Language->VersionAdmin( m_GHost->m_Version ) );
			else
				SendChat( player, m_GHost->m_Language->VersionNotAdmin( m_GHost->m_Version ) );
		}

		//
		// !VOTEKICK
		//

		else if ( Command == "votekick" && m_GHost->m_VoteKickAllowed && !Payload.empty( ) )
		{
			if ( !m_KickVotePlayer.empty( ) )
				SendChat( player, m_GHost->m_Language->UnableToVoteKickAlreadyInProgress( ) );
			else if ( m_Players.size( ) == 2 )
				SendChat( player, m_GHost->m_Language->UnableToVoteKickNotEnoughPlayers( ) );
			else
			{
				CGamePlayer *LastMatch = NULL;
				uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

				if ( Matches == 0 )
					SendChat( player, m_GHost->m_Language->UnableToVoteKickNoMatchesFound( Payload ) );
				else if ( Matches == 1 )
				{
					if ( LastMatch->GetReserved( ) )
						SendChat( player, m_GHost->m_Language->UnableToVoteKickPlayerIsReserved( LastMatch->GetName( ) ) );
					else
					{
						m_KickVotePlayer = LastMatch->GetName( );
						m_StartedKickVoteTime = GetTime( );

						for ( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
							(*i)->SetKickVote( false );

						player->SetKickVote( true );
						CONSOLE_Print( "[GAME: " + m_GameName + "] votekick against player [" + m_KickVotePlayer + "] started by player [" + User + "]" );
						SendAllChat( m_GHost->m_Language->StartedVoteKick( LastMatch->GetName( ), User, UTIL_ToString( (uint32_t)ceil( ( GetNumHumanPlayers( ) - 1 ) * (float)m_GHost->m_VoteKickPercentage / 100 ) - 1 ) ) );
						SendAllChat( m_GHost->m_Language->TypeYesToVote( string( 1, m_GHost->m_CommandTrigger ) ) );
					}
				}
				else
					SendChat( player, m_GHost->m_Language->UnableToVoteKickFoundMoreThanOneMatch( Payload ) );
			}
		}

		//
		// !YES
		//

		else if ( Command == "yes" && !m_KickVotePlayer.empty( ) && player->GetName( ) != m_KickVotePlayer && !player->GetKickVote( ) )
		{
			player->SetKickVote( true );
			uint32_t VotesNeeded = (uint32_t)ceil( ( GetNumHumanPlayers( ) - 1 ) * (float)m_GHost->m_VoteKickPercentage / 100 );
			uint32_t Votes = 0;

			for ( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
			{
				if ( (*i)->GetKickVote( ) )
					++Votes;
			}

			if ( Votes >= VotesNeeded )
			{
				CGamePlayer *Victim = GetPlayerFromName( m_KickVotePlayer, true );

				if ( Victim )
				{
					Victim->SetDeleteMe( true );
					Victim->SetLeftReason( m_GHost->m_Language->WasKickedByVote( ) );

					if ( !m_GameLoading && !m_GameLoaded )
						Victim->SetLeftCode( PLAYERLEAVE_LOBBY );
					else
						Victim->SetLeftCode( PLAYERLEAVE_LOST );

					if ( !m_GameLoading && !m_GameLoaded )
						OpenSlot( GetSIDFromPID( Victim->GetPID( ) ), false );

					CONSOLE_Print( "[GAME: " + m_GameName + "] votekick against player [" + m_KickVotePlayer + "] passed with " + UTIL_ToString( Votes ) + "/" + UTIL_ToString( GetNumHumanPlayers( ) ) + " votes" );
					SendAllChat( m_GHost->m_Language->VoteKickPassed( m_KickVotePlayer ) );
				}
				else
					SendAllChat( m_GHost->m_Language->ErrorVoteKickingPlayer( m_KickVotePlayer ) );

				m_KickVotePlayer.clear( );
				m_StartedKickVoteTime = 0;
			}
			else
				SendAllChat( m_GHost->m_Language->VoteKickAcceptedNeedMoreVotes( m_KickVotePlayer, User, UTIL_ToString( VotesNeeded - Votes ) ) );
		}
		else
		{
			CommandExecuted = false;
		}
	}
	
    return ( HideCommand && CommandExecuted );
}

void CGame :: EventGameStarted( )
{
    CBaseGame :: EventGameStarted( );

    // record everything we need to ban each player in case we decide to do so later
    // this is because when a player leaves the game an admin might want to ban that player
    // but since the player has already left the game we don't have access to their information anymore
    // so we create a "potential ban" for each player and only store it in the database if requested to by an admin

    for ( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
        m_DBBans.push_back( new CDBBan( (*i)->GetJoinedRealm( ), (*i)->GetName( ), (*i)->GetExternalIPString( ), string( ), string( ), string( ), string( ) ) );
}

bool CGame :: IsGameDataSaved( )
{
    return m_CallableGameAdd && m_CallableGameAdd->GetReady( );
}

void CGame :: SaveGameData( )
{
    CONSOLE_Print( "[GAME: " + m_GameName + "] saving game data to database" );
    m_CallableGameAdd = m_GHost->m_DB->ThreadedGameAdd( m_GHost->m_BNETs.size( ) == 1 ? m_GHost->m_BNETs[0]->GetServer( ) : string( ), m_DBGame->GetMap( ), m_GameName, m_OwnerName, m_GameTicks / 1000, m_GameState, m_CreatorName, m_CreatorServer );
}
