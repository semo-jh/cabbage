/*
  ==============================================================================

    This file was auto-generated by the Jucer!

    It contains the basic outline for a simple desktop window.

  ==============================================================================
*/

#include "CabbageEditorWindow.h"


//==============================================================================
CabbageEditorWindow::CabbageEditorWindow()
    : DocumentWindow (T("Cabbage Csound Editor"),
                      Colours::black,
                      DocumentWindow::allButtons)
{
    centreWithSize (500, 400);
    setVisible (true);
	setResizable(true, true);
	contentComponent = new CsoundEditor ();
	contentComponent->addActionListener(this);
	contentComponent->setBounds(0, 0, getWidth(), getHeight());
	setContentOwned(contentComponent, true);


#if JUCE_MAC
setMacMainMenu (0);
#else
setMenuBar (contentComponent, 25);
#endif
}

CabbageEditorWindow::~CabbageEditorWindow()
{
}

void CabbageEditorWindow::closeButtonPressed()
{
   setVisible(false);
}

void CabbageEditorWindow::actionListenerCallback (const String& message){
	sendActionMessage(message);
}