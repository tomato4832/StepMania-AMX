#include "global.h"
/*
-----------------------------------------------------------------------------
 Class: ScreenAward

 Desc: See header.

 Copyright (c) 2001-2002 by the person(s) listed below.  All rights reserved.
	Chris Danford
-----------------------------------------------------------------------------
*/

#include "ScreenAward.h"
#include "ScreenManager.h"
#include "AnnouncerManager.h"
#include "RageSounds.h"
#include "ThemeManager.h"
#include "GameState.h"
#include "SongManager.h"
#include "Steps.h"


#define NEXT_SCREEN		THEME->GetMetric (m_sName,"NextScreen")


ScreenAward::ScreenAward( CString sName ) : Screen( sName )
{
	bool bEitherPlayerHasAward = false;

	for( int p=0; p<NUM_PLAYERS; p++ )
	{
		if( !GAMESTATE->IsHumanPlayer(p) )
			continue;

		CString sAward;
		CString sDescription;
		if( !GAMESTATE->m_vLastPerDifficultyAwards[p].empty() )
		{
			PerDifficultyAward pda = GAMESTATE->m_vLastPerDifficultyAwards[p].front();
			GAMESTATE->m_vLastPerDifficultyAwards[p].pop_front();
			sAward = PerDifficultyAwardToString( pda );
			sDescription = 
				PerDifficultyAwardToThemedString( pda ) + "\n" +
				SONGMAN->GetDifficultyThemeName( GAMESTATE->m_pCurNotes[p]->GetDifficulty() );
		}
		else if( !GAMESTATE->m_vLastPeakComboAwards[p].empty() )
		{
			PeakComboAward pca = GAMESTATE->m_vLastPeakComboAwards[p].front();
			GAMESTATE->m_vLastPeakComboAwards[p].pop_front();
			sAward = PeakComboAwardToString( pca );
			sDescription = PeakComboAwardToThemedString( pca );
		}
		
		if( sAward.empty() )
			continue;	// skip this player

		bEitherPlayerHasAward = true;

		m_Received[p].Load( THEME->GetPathG(m_sName,"received") );
		m_Received[p]->SetName( ssprintf("ReceivedP%d",p+1) );
		SET_XY_AND_ON_COMMAND( m_Received[p] );
		this->AddChild( m_Received[p] );

		m_Trophy[p].Load( THEME->GetPathG(m_sName,"trophy "+sAward) );
		m_Trophy[p]->SetName( ssprintf("TrophyP%d",p+1) );
		SET_XY_AND_ON_COMMAND( m_Trophy[p] );
		this->AddChild( m_Trophy[p] );

		m_textDescription[p].LoadFromFont( THEME->GetPathF(m_sName,"description") );
		m_textDescription[p].SetName( ssprintf("DescriptionP%d",p+1) );
		m_textDescription[p].SetText( sDescription );
		SET_XY_AND_ON_COMMAND( m_textDescription[p] );
		this->AddChild( &m_textDescription[p] );
	}

	if( !bEitherPlayerHasAward )
	{
		HandleScreenMessage( SM_GoToNextScreen );
		return;
	}

	m_Menu.Load( m_sName );
	this->AddChild( &m_Menu );

	SOUND->PlayMusic( THEME->GetPathToS( m_sName + " music") );
}

void ScreenAward::DrawPrimitives()
{
	m_Menu.DrawBottomLayer();

	Screen::DrawPrimitives();

	m_Menu.DrawTopLayer();
}

void ScreenAward::Input( const DeviceInput& DeviceI, const InputEventType type, const GameInput &GameI, const MenuInput &MenuI, const StyleInput &StyleI )
{
//	LOG->Trace( "ScreenAward::Input()" );

	if( m_Menu.IsTransitioning() )
		return;

	Screen::Input( DeviceI, type, GameI, MenuI, StyleI );
}

void ScreenAward::HandleScreenMessage( const ScreenMessage SM )
{
	switch( SM )
	{
	case SM_MenuTimer:
		MenuStart( PLAYER_INVALID );
		break;
	case SM_GoToNextScreen:
		// TRICKY: Keep looping on this screen until all awards are shown
		bool bMoreAwardsLeft = false;
		for( int p=0; p<NUM_PLAYERS; p++ )
		{
			if( !GAMESTATE->IsHumanPlayer(p) )
				continue;

			if( !GAMESTATE->m_vLastPerDifficultyAwards[p].empty() ||
				!GAMESTATE->m_vLastPeakComboAwards[p].empty() )
			{
				bMoreAwardsLeft = true;
			}
		}
		if( bMoreAwardsLeft )
			SCREENMAN->SetNewScreen( m_sName );
		else
			SCREENMAN->SetNewScreen( NEXT_SCREEN );
		break;
	}
}

void ScreenAward::MenuStart( PlayerNumber pn )
{
	for( int p=0; p<NUM_PLAYERS; p++ )
	{
		if( !GAMESTATE->IsHumanPlayer(p) )
			continue;

		OFF_COMMAND( m_Received[p] );
		OFF_COMMAND( m_Trophy[p] );
		OFF_COMMAND( m_textDescription[p] );
	}

	m_Menu.StartTransitioning( SM_GoToNextScreen );
}