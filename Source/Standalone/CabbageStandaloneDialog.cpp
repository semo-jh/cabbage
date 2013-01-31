/*
  Copyright (C) 2009 Rory Walsh

  Cabbage is free software; you can redistribute it
  and/or modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.   

  Cabbage is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with Csound; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307 USA
*/

#include "CabbageStandaloneDialog.h"


#define MAXBYTES 16777216

//==============================================================================
//  Somewhere in the codebase of your plugin, you need to implement this function
//  and make it create an instance of the filter subclass that you're building.
extern CabbagePluginAudioProcessor* JUCE_CALLTYPE createCabbagePluginFilter(String inputfile, bool guiOnOff);


//==============================================================================
StandaloneFilterWindow::StandaloneFilterWindow (const String& title,
                                                const Colour& backgroundColour)
    : DocumentWindow (title, backgroundColour,
                      DocumentWindow::minimiseButton
                       | DocumentWindow::closeButton),
      optionsButton ("options"), 
	  isGUIOn(false), 
	  pipeOpenedOk(false), 
	  AudioEnabled(true), 
	  isAFileOpen(false),
	  standaloneMode(false)
{
	
	String defaultCSDFile = File(File::getSpecialLocation(File::currentExecutableFile)).withFileExtension(".csd").getFullPathName();
	consoleMessages = "";
	cabbageDance = 0;
	setTitleBarButtonsRequired (DocumentWindow::minimiseButton | DocumentWindow::closeButton, false);
    Component::addAndMakeVisible (&optionsButton);
    optionsButton.addListener (this);
	timerRunning = false;
	yAxis = 0;
	optionsButton.setTriggeredOnMouseDown (true);
	bool alwaysontop = getPreference(appProperties, "SetAlwaysOnTop");
	setAlwaysOnTop(alwaysontop);
	this->setResizable(false, false);

	lookAndFeel = new CabbageLookAndFeel();
	this->setLookAndFeel(lookAndFeel);

	oldLookAndFeel = new LookAndFeel();
// MOD - Stefano Bonetti 
#ifdef Cabbage_Named_Pipe 
	ipConnection = new socketConnection(*this);
	pipeOpenedOk = ipConnection->createPipe(String("cabbage"));
	if(pipeOpenedOk) Logger::writeToLog(String("Namedpipe created ..."));
#endif 
// MOD - End

	JUCE_TRY
    {
        filter = createCabbagePluginFilter("", false);
		filter->addChangeListener(this);
		filter->addActionListener(this);
		filter->sendChangeMessage();
		filter->createGUI("");		
    }
    JUCE_CATCH_ALL

    if (filter == nullptr)
    {
        jassertfalse    // Your filter didn't create correctly! In a standalone app that's not too great.
        JUCEApplication::quit();
    }

    filter->setPlayConfigDetails (JucePlugin_MaxNumInputChannels, 
                                          JucePlugin_MaxNumOutputChannels,
                                          44100, 512);

    PropertySet* const globalSettings = getGlobalSettings();

    deviceManager = new AudioDeviceManager();
	deviceManager->addAudioCallback (&player);
	deviceManager->addMidiInputCallback (String::empty, &player);

	ScopedPointer<XmlElement> savedState;
	if (globalSettings != nullptr)
    savedState = globalSettings->getXmlValue ("audioSetup");

    deviceManager->initialise (filter->getNumInputChannels(),
                               filter->getNumOutputChannels(), 0, false);
	deviceManager->closeAudioDevice();
	

	int runningCabbageIO = getPreference(appProperties, "UseCabbageIO");
    if(runningCabbageIO){	
		player.setProcessor (filter);    
		if (globalSettings != nullptr)
		{
			MemoryBlock data;

			if (data.fromBase64Encoding (globalSettings->getValue ("filterState"))
				 && data.getSize() > 0)
			{
				filter->setStateInformation (data.getData(), data.getSize());
			}
		}

		
	}
	else deviceManager->closeAudioDevice();

	setContentOwned (filter->createEditorIfNeeded(), true);

    const int x = globalSettings->getIntValue ("windowX", -100);
    const int y = globalSettings->getIntValue ("windowY", -100);

    if (x != -100 && y != -100)
        setBoundsConstrained (Rectangle<int> (x, y, getWidth(), getHeight()));
    else
		centreWithSize (getWidth(), getHeight());
		
	setPreference(appProperties, "AutoUpdate",0);
	
	//opens a default file that matches the name of the current executable
	//this can be used to create more 'standalone' like apps
	if(File(defaultCSDFile).existsAsFile()){
		standaloneMode = true;
		openFile(defaultCSDFile);
	}
}
//==============================================================================
// Destructor
//==============================================================================
StandaloneFilterWindow::~StandaloneFilterWindow()
{

#ifdef Cabbage_Named_Pipe 
// MOD - Stefano Bonetti
	if(ipConnection){ 
	ipConnection->disconnect();
	}
// MOD - End
#endif

    PropertySet* const globalSettings = getGlobalSettings();

    if (globalSettings != nullptr)
    {
        globalSettings->setValue ("windowX", getX());
        globalSettings->setValue ("windowY", getY());

        if (deviceManager != nullptr)
        {
            ScopedPointer<XmlElement> xml (deviceManager->createStateXml());
            globalSettings->setValue ("audioSetup", xml);
        }
    }

    deviceManager->removeMidiInputCallback (String::empty, &player);
    deviceManager->removeAudioCallback (&player);
    deviceManager = nullptr;

    if (globalSettings != nullptr && filter != nullptr)
    {
        MemoryBlock data;
        filter->getStateInformation (data);

        globalSettings->setValue ("filterState", data.toBase64Encoding());
    }
	filter->suspendProcessing(true);
    deleteFilter();
}

//==============================================================================
// sends messages to WinXound
//==============================================================================
void StandaloneFilterWindow::sendMessageToWinXound(String messageType, String message)
{
if(pipeOpenedOk){
String text = messageType+String("|")+message;
MemoryBlock messageData (text.toUTF8(), text.getNumBytesAsUTF8());
pipeOpenedOk = ipConnection->sendMessage(messageData);
}
}

void StandaloneFilterWindow::sendMessageToWinXound(String messageType, int value)
{
if(pipeOpenedOk){
String text = messageType+String("|")+String(value);
MemoryBlock messageData (text.toUTF8(), text.getNumBytesAsUTF8());
pipeOpenedOk = ipConnection->sendMessage(messageData);
}
}

//==============================================================================
// insane Cabbage dancing....
//==============================================================================
void StandaloneFilterWindow::timerCallback()
{   
	int64 diskTime = csdFile.getLastModificationTime().toMilliseconds();
	int64 tempTime = lastSaveTime.toMilliseconds();
	if(diskTime>tempTime){
		resetFilter();
		lastSaveTime = csdFile.getLastModificationTime();
	}
	//cout << csdFile.getLastModificationTime().toString(true, true, false, false);
	if(cabbageDance){
	float moveY = sin(yAxis*2*3.14*20/100); 
	float moveX = sin(yAxis*2*3.14*10/100); 
	yAxis+=1;
	this->setTopLeftPosition(this->getScreenX()+(moveX*5), this->getScreenY()+(moveY*10));
	}
}

//==============================================================================
// action Callback - updates instrument according to changes in source code
//==============================================================================
void StandaloneFilterWindow::actionListenerCallback (const String& message){
	if(message == "GUI Update"){
		if(cabbageCsoundEditor){
		cabbageCsoundEditor->setCsoundFile(csdFile);
		cabbageCsoundEditor->csoundEditor->highlightLine(filter->getCurrentLineText());
		cabbageCsoundEditor->csoundEditor->grabKeyboardFocus();
		}
	}

	else if(message == "GUI Updated, controls added"){
	filter->createGUI(csdFile.loadFileAsString());
	setGuiEnabled(true);
	filter->setGuiEnabled(true);
	setCurrentLine(filter->getCurrentLine()+1);
	}
	
	else if(message.equalsIgnoreCase("fileSaved")){
	saveFile();
	}

	else if(message.contains("fileOpen")){
	openFile("");
	}

	else if(message.contains("fileSaveAs")){
	saveFileAs();
	}

	else if(message.contains("fileExportSynth")){
	exportPlugin(String("VSTi"), false);
	}

	else if(message.contains("fileExportEffect")){
	exportPlugin(String("VST"), false);
	}

	else if(message.contains("fileUpdateGUI")){
		filter->createGUI(cabbageCsoundEditor->getCurrentText());
		csdFile.replaceWithText(cabbageCsoundEditor->getCurrentText()); 
	if(cabbageCsoundEditor){
	cabbageCsoundEditor->setName(csdFile.getFullPathName());
	
	if(cabbageCsoundEditor->isVisible())
		cabbageCsoundEditor->csoundEditor->textEditor->grabKeyboardFocus();
	}
	}

#ifdef Cabbage_Build_Standalone
	else if(message.contains("MENU COMMAND: manual update instrument"))
		resetFilter();

	else if(message.contains("MENU COMMAND: open instrument"))
		openFile("");

	else if(message.contains("MENU COMMAND: manual update GUI"))
		filter->createGUI(csdFile.loadFileAsString());
	
	else if(message.contains("MENU COMMAND: suspend audio"))
        if(AudioEnabled){
			filter->suspendProcessing(true);
			AudioEnabled = false;
		}
		else{
			AudioEnabled = true;
			filter->suspendProcessing(false);
		}
else{}
#endif

}
//==============================================================================

//==============================================================================
// listener Callback - updates WinXound compiler output with Cabbage messages
//==============================================================================
void StandaloneFilterWindow::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
String text = "";

for(int i=0;i<filter->getDebugMessageArray().size();i++)
      {
          if(filter->getDebugMessageArray().getReference(i).length()>0)
          {
              text += String(filter->getDebugMessageArray().getReference(i).toUTF8());
			 
          }
		   
      }
consoleMessages = consoleMessages+text+"\n";
filter->clearDebugMessageArray();
if(cabbageCsoundEditor){
cabbageCsoundEditor->setCsoundOutputText(consoleMessages+"\n");
consoleMessages="";

}


}
//==============================================================================
// Delete filter
//==============================================================================
void StandaloneFilterWindow::deleteFilter()
{
	player.setProcessor (nullptr);

    if (filter != nullptr && getContentComponent() != nullptr)
    {
        filter->editorBeingDeleted (dynamic_cast <AudioProcessorEditor*> (getContentComponent()));
        clearContentComponent();
    }

    filter = nullptr;
}

//==============================================================================
// Reset filter
//==============================================================================
void StandaloneFilterWindow::resetFilter()
{
//first we check that the audio device is up and running ok




	deleteFilter();
	deviceManager->closeAudioDevice();
	filter = createCabbagePluginFilter(csdFile.getFullPathName(), false);
	filter->suspendProcessing(true);
	filter->addChangeListener(this);
	filter->addActionListener(this);
	filter->sendChangeMessage();
	filter->createGUI(csdFile.loadFileAsString());
	
	String test = filter->getPluginName();
	setName(filter->getPluginName());

	int runningCabbageProcess = getPreference(appProperties, "UseCabbageIO");
    
	setContentOwned (filter->createEditorIfNeeded(), true);
    if(runningCabbageProcess){
		if (filter != nullptr)
		{
			if (deviceManager != nullptr){
				player.setProcessor (filter);
				deviceManager->restartLastAudioDevice();
			}
		}
		filter->suspendProcessing(false);
	}
	else{
		filter->performEntireScore();
	}


    PropertySet* const globalSettings = getGlobalSettings();

    if (globalSettings != nullptr)
        globalSettings->removeValue ("filterState");
	
#ifdef Cabbage_Named_Pipe
	//notify WinXound that Cabbage is set up and ready for action
	sendMessageToWinXound(String("CABBAGE_LOADED"), "");
#endif

	if(cabbageCsoundEditor){
	cabbageCsoundEditor->setName(csdFile.getFullPathName());
	if(cabbageCsoundEditor->isVisible())
		cabbageCsoundEditor->csoundEditor->textEditor->grabKeyboardFocus();
	}

}

//==============================================================================
void StandaloneFilterWindow::saveState()
{
    PropertySet* const globalSettings = getGlobalSettings();

    FileChooser fc (TRANS("Save current state"),
                    globalSettings != nullptr ? File (globalSettings->getValue ("lastStateFile"))
                                              : File::nonexistent);

    if (fc.browseForFileToSave (true))
    {
        MemoryBlock data;
        filter->getStateInformation (data);

        if (! fc.getResult().replaceWithData (data.getData(), data.getSize()))
        {
            AlertWindow::showMessageBox (AlertWindow::WarningIcon,
                                         TRANS("Error whilst saving"),
                                         TRANS("Couldn't write to the specified file!"));
        }
    }
}

void StandaloneFilterWindow::loadState()
{
    PropertySet* const globalSettings = getGlobalSettings();

    FileChooser fc (TRANS("Load a saved state"),
                    globalSettings != nullptr ? File (globalSettings->getValue ("lastStateFile"))
                                              : File::nonexistent);

    if (fc.browseForFileToOpen())
    {
        MemoryBlock data;

        if (fc.getResult().loadFileAsData (data))
        {
            filter->setStateInformation (data.getData(), data.getSize());
        }
        else
        {
            AlertWindow::showMessageBox (AlertWindow::WarningIcon,
                                         TRANS("Error whilst loading"),
                                         TRANS("Couldn't read from the specified file!"));
        }
    }
}

//==============================================================================
PropertySet* StandaloneFilterWindow::getGlobalSettings()
{
    /* If you want this class to store the plugin's settings, you can set up an
       ApplicationProperties object and use this method as it is, or override this
       method to return your own custom PropertySet.

       If using this method without changing it, you'll probably need to call
       ApplicationProperties::setStorageParameters() in your plugin's constructor to
       tell it where to save the file.
    */
	getPreference(appProperties, "htmlHelp", "some directory");
	return appProperties->getUserSettings();
}

void StandaloneFilterWindow::showAudioSettingsDialog()
{
//int runningCabbageProcess = appProperties->getUserSettings()->getValue("UseCabbageIO", var(0)).getFloatValue();
//if(!runningCabbageProcess){
//	showMessage("Cabbage is currently letting Csound access the audio and MIDI inputs/outputs. If you wish to use Cabbage IO please enable it from the preferences.", &this->getLookAndFeel());
//	return;
//}
const int numIns = filter->getNumInputChannels() <= 0 ? JucePlugin_MaxNumInputChannels : filter->getNumInputChannels();
const int numOuts = filter->getNumOutputChannels() <= 0 ? JucePlugin_MaxNumOutputChannels : filter->getNumOutputChannels();

    AudioDeviceSelectorComponent selectorComp (*deviceManager,
                                               numIns, numIns, numOuts, numOuts,
                                               true, false, true, false);

    selectorComp.setSize (400, 250);
	setAlwaysOnTop(false);
	selectorComp.setLookAndFeel(lookAndFeel);
	Colour col(44, 44, 44);
	DialogWindow::showModalDialog(TRANS("Audio Settings"), &selectorComp, this, col, true, false, false);
	bool alwaysontop = getPreference(appProperties, "SetAlwaysOnTop");
	setAlwaysOnTop(alwaysontop);

}
//==============================================================================
void StandaloneFilterWindow::closeButtonPressed()
{
	/*
if(cabbageCsoundEditor){
	cabbageCsoundEditor->removeAllActionListeners(); 
	cabbageCsoundEditor->removeFromDesktop();
	cabbageCsoundEditor->closeButtonPressed();
}
Time::waitForMillisecondCounter(1000);
*/
JUCEApplication::quit();
}

void StandaloneFilterWindow::resized()
{
    DocumentWindow::resized();
    optionsButton.setBounds (8, 6, 60, getTitleBarHeight() - 8);
}


//==============================================================================
// Button clicked method
//==============================================================================
void StandaloneFilterWindow::buttonClicked (Button*)
{
    if (filter == nullptr)
        return;

	String test;
    PopupMenu m;
	PopupMenu subMenu;
	m.setLookAndFeel(lookAndFeel);
	subMenu.setLookAndFeel(lookAndFeel);

	if(!standaloneMode){
		m.addItem(1, String("Open Cabbage Instrument | Ctrl+o"));
		subMenu.addItem(30, String("Effect"));
		subMenu.addItem(31, String("Instrument"));
		m.addSubMenu(String("New Cabbage..."), subMenu);

		m.addItem(2, String("View Source Editor"));
		m.addSeparator();
	}
	if(AudioEnabled)
	m.addItem(400, "Audio Enabled | Ctrl+m", true, true); 
	else
	m.addItem(400, "Audio Enabled | Ctrl+m", true, false); 
    m.addItem(4, TRANS("Audio Settings..."));
    m.addSeparator();
	if(!standaloneMode){	
		m.addItem(100, String("Toggle Edit-mode"));
		m.addSeparator();
		subMenu.clear();
		subMenu.addItem(15, TRANS("Plugin Synth"));
		subMenu.addItem(16, TRANS("Plugin Effect"));
		m.addSubMenu(TRANS("Export..."), subMenu);
#ifndef MACOSX
		subMenu.clear();
		subMenu.addItem(5, TRANS("Plugin Synth"));
		subMenu.addItem(6, TRANS("Plugin Effect"));
		m.addSubMenu(TRANS("Export As..."), subMenu);
#endif
		m.addSeparator();
	}

	if(!standaloneMode){
		m.addItem(8, String("Rebuild Instrument | Ctrl+b"));
		m.addItem(9, String("Rebuild GUI | Ctrl+u"));
	/*
	if(filter->getMidiDebug())
    m.addItem(9, TRANS("Show MIDI Debug Information"), true, true);
	else
	m.addItem(9, TRANS("Show MIDI Debug Information"));
	*/
		subMenu.clear();
		subMenu.addItem(11, TRANS("Effects"));
		subMenu.addItem(12, TRANS("Synths"));
		m.addSubMenu(TRANS("Batch Convert"), subMenu);
		m.addSeparator();
		//m.addItem(2000, "Test me");

/*
	m.addSeparator();
	if(cabbageDance)
	m.addItem(99, String("Cabbage Dance"), true, true);
	else
	m.addItem(99, String("Cabbage Dance"));
*/
		subMenu.clear();
		int alwaysontop = getPreference(appProperties, "SetAlwaysOnTop"); 
		if(alwaysontop)
		subMenu.addItem(7, String("Always on Top"), true, true);
		else
		subMenu.addItem(7, String("Always on Top"), true, false);
		//preferences....
		int pluginInfo = getPreference(appProperties, "DisablePluginInfo");
		subMenu.addItem(203, "Set Cabbage Plant Directory");
		subMenu.addItem(200, "Set Csound Manual Directory");
		if(!pluginInfo)
		subMenu.addItem(201, String("Disable Export Plugin Info"), true, false);
		else
		subMenu.addItem(201, String("Disable Export Plugin Info"), true, true);

		int autoUpdate = getPreference(appProperties, "AutoUpdate");
		if(!autoUpdate)
		subMenu.addItem(299, String("Auto-update"), true, false);
		else
		subMenu.addItem(299, String("Auto-update"), true, true);

		int disableGUIEditWarning = getPreference(appProperties, "DisableGUIEditModeWarning");
		if(!disableGUIEditWarning)
		subMenu.addItem(202, String("Disable GUI Edit Mode warning"), true, true);
		else
		subMenu.addItem(202, String("Disable GUI Edit Mode warning"), true, false);

		int csoundIO = getPreference(appProperties, "UseCabbageIO");
		if(!csoundIO)
		subMenu.addItem(204, String("Use Cabbage IO"), true, false);
		else
		subMenu.addItem(204, String("Use Cabbage IO"), true, true);

		m.addSubMenu("Preferences", subMenu);
		
		
	}
	
	
	

	int options = m.showAt (&optionsButton);
 
	//----- open file ------
	if(options==1){
		openFile("");
	}
	else if(options==2000){
		filter->performEntireScore();
		
	}
	//----- view text editor ------
	else if(options==2){
	if(!cabbageCsoundEditor){
	cabbageCsoundEditor = new CabbageEditorWindow(lookAndFeel);
	cabbageCsoundEditor->setVisible(false);
	cabbageCsoundEditor->setFullScreen(true);
	cabbageCsoundEditor->addActionListener(this);
	}
	cabbageCsoundEditor->setCsoundFile(csdFile);
	this->toBehind(cabbageCsoundEditor);
	cabbageCsoundEditor->setVisible(true);
	cabbageCsoundEditor->setFullScreen(true);
	cabbageCsoundEditor->toFront(true);
	cabbageCsoundEditor->setCsoundOutputText(consoleMessages);
	//Logger::writeToLog(consoleMessages);
	cabbageCsoundEditor->csoundEditor->textEditor->setWantsKeyboardFocus(true);
	cabbageCsoundEditor->csoundEditor->textEditor->grabKeyboardFocus();
	}
	//----- new effect ------
	else if(options==30){
	if(!cabbageCsoundEditor){
		cabbageCsoundEditor = new CabbageEditorWindow(lookAndFeel);
		cabbageCsoundEditor->addActionListener(this);
		}
		cabbageCsoundEditor->setVisible(true);
		cabbageCsoundEditor->csoundEditor->newFile("effect");
		saveFileAs();
		cabbageCsoundEditor->csoundEditor->textEditor->grabKeyboardFocus();
	}
	//----- new instrument ------
	else if(options==31){
	if(!cabbageCsoundEditor){
		cabbageCsoundEditor = new CabbageEditorWindow(lookAndFeel);
		cabbageCsoundEditor->addActionListener(this);
		}
	cabbageCsoundEditor->setVisible(true);
	cabbageCsoundEditor->csoundEditor->newFile("instrument");
	saveFileAs();
	cabbageCsoundEditor->csoundEditor->textEditor->grabKeyboardFocus();
	isAFileOpen = true;
	}
	//----- audio settings ------
   	else if(options==4){
        showAudioSettingsDialog();
		int val = getPreference(appProperties, "UseCabbageIO");
		if(!val)
		resetFilter();
	}

	//suspend audio
   	else if(options==400){
        if(AudioEnabled){
			filter->suspendProcessing(true);
			AudioEnabled = false;
		}
		else{
			AudioEnabled = true;
			filter->suspendProcessing(false);
		}
	}

	//----- export ------
	else if(options==15)
	exportPlugin(String("VSTi"), false);
	
	else if(options==16)
	exportPlugin(String("VST"), false);


	//----- export as ------
	else if(options==5)
	exportPlugin(String("VSTi"), true);
	
	else if(options==6)
	exportPlugin(String("VST"), true);

	//----- always on top  ------
	else if(options==7)

	if(getPreference(appProperties, "SetAlwaysOnTop")){
		this->setAlwaysOnTop(false);
		setPreference(appProperties, "SetAlwaysOnTop", 0);
	}
	else{
		this->setAlwaysOnTop(true);
		setPreference(appProperties, "SetAlwaysOnTop", 1);
	}
	
	//----- update instrument  ------
    else if(options==8){
        resetFilter();
	}
	//----- update GUI only -----
	else if(options==9)
	filter->createGUI(csdFile.loadFileAsString());

	//----- batch process ------
	else if(options==11)
		BatchProcess(String("VST"));

	else if(options==12)
		BatchProcess(String("VSTi"));

	//----- auto-update file when saved remotely ------
	else if(options==299){
		int val = getPreference(appProperties, "AutoUpdate");
		if(val==0){
			setPreference(appProperties, "AutoUpdate", 1);
			startTimer(100);	
			}
		else{
			setPreference(appProperties, "AutoUpdate", 0);
			stopTimer();
		}
		}
/*
	//------- cabbage dance ------
	else if(options==99){
		if(!cabbageDance){
		startTimer(20);
		cabbageDance = true;
		}
		else{
		stopTimer();
		cabbageDance = false;
		}
	}
	*/
	//------- preference Csound manual dir ------
	else if(options==200){
		String dir = getPreference(appProperties, "CsoundHelpDir", "");
		FileChooser browser(String("Please select the Csound manual directory..."), File(dir), String("*.csd"));
		if(browser.browseForDirectory()){
			setPreference(appProperties, "CsoundHelpDir", browser.getResult().getFullPathName());
		}	
	}
	//------- preference Csound manual dir ------
	else if(options==203){
		String dir = getPreference(appProperties, "PlantFileDir", "");
		FileChooser browser(String("Please select your Plant file directory..."), File(dir), String("*.csd"));
		if(browser.browseForDirectory()){
			setPreference(appProperties, "PlantFileDir", browser.getResult().getFullPathName());
		}	
	}
	//--------preference Csound IO
	else if(options==204){
		int val = getPreference(appProperties, "UseCabbageIO");
		if(val==0) 
			setPreference(appProperties, "UseCabbageIO", 1);
		else
			setPreference(appProperties, "UseCabbageIO", 0);
	}
	
	//------- preference plugin info ------
	else if(options==201){
		int val = getPreference(appProperties, "DisablePluginInfo");
		if(val==0)
			appProperties->getUserSettings()->setValue("DisablePluginInfo", var(1));
		else
			appProperties->getUserSettings()->setValue("DisablePluginInfo", var(0));
	}
	//------- preference disable gui edit warning ------
	else if(options==202){
		int val = getPreference(appProperties, "DisableGUIEditModeWarning");
		if(val==0) 
			setPreference(appProperties, "DisableGUIEditModeWarning", 1);
		else
			setPreference(appProperties, "DisableGUIEditModeWarning", 0);
	}
	//------- enable GUI edito mode------
	else if(options==100){
		int val = getPreference(appProperties, "DisableGUIEditModeWarning");
		if(val)
			showMessage("Warning!! This feature is bleeding edge! (that's programmer speak for totally untested and likely to crash hard!). If you like to live on the edge, disable this warning under the 'Preferences' menu command and try 'Edit Mode' again, otherwise just let it be...", oldLookAndFeel);
		else{
	if(isAFileOpen == true)
		if(filter->isGuiEnabled()){
		((CabbagePluginAudioProcessorEditor*)filter->getActiveEditor())->setEditMode(false);
		filter->setGuiEnabled(false);
		}
		else{
		((CabbagePluginAudioProcessorEditor*)filter->getActiveEditor())->setEditMode(true);
		filter->setGuiEnabled(true);
		stopTimer();
		setPreference(appProperties, "AutoUpdate", 0);
		}
	else showMessage("Open or create a file first", &getLookAndFeel());
		}
	}
	repaint();
}

//==============================================================================
// open/save/save as methods
//==============================================================================
void StandaloneFilterWindow::openFile(String _csdfile)
{
if(_csdfile.length()>4){
	csdFile = File(_csdfile);
	originalCsdFile = csdFile;
	lastSaveTime = csdFile.getLastModificationTime();
	csdFile.setAsCurrentWorkingDirectory();
	resetFilter();
}	
else{	
#ifdef MACOSX
		FileChooser openFC(String("Open a Cabbage .csd file..."), File::nonexistent, String("*.csd;*.vst"));
		if(openFC.browseForFileToOpen()){
			csdFile = openFC.getResult();
			originalCsdFile = openFC.getResult();
			lastSaveTime = csdFile.getLastModificationTime();
			csdFile.setAsCurrentWorkingDirectory();
			if(csdFile.getFileExtension()==(".vst")){
				String csd = csdFile.getFullPathName();
				csd << "/Contents/" << csdFile.getFileNameWithoutExtension() << ".csd";
				csdFile = File(csd);
			}
			if(cabbageCsoundEditor)
				cabbageCsoundEditor->setCsoundFile(csdFile);
			isAFileOpen = true;
			resetFilter();
			//cabbageCsoundEditor->setCsoundFile(csdFile);
		}			
#else
		FileChooser openFC(String("Open a Cabbage .csd file..."), File::nonexistent, String("*.csd"));
		this->setAlwaysOnTop(false);
		if(openFC.browseForFileToOpen()){
			csdFile = openFC.getResult();
			csdFile.getParentDirectory().setAsCurrentWorkingDirectory();
			lastSaveTime = csdFile.getLastModificationTime();
			resetFilter();
			if(cabbageCsoundEditor)
			cabbageCsoundEditor->setCsoundFile(csdFile);
			isAFileOpen = true;
		}
		if(getPreference(appProperties, "SetAlwaysOnTop"))
			setAlwaysOnTop((true));
		else
			setAlwaysOnTop(false);
#endif
	}

}

void StandaloneFilterWindow::saveFile()
{
if(csdFile.hasWriteAccess())
csdFile.replaceWithText(cabbageCsoundEditor->getCurrentText());
resetFilter();
}

void StandaloneFilterWindow::saveFileAs()
{
FileChooser saveFC(String("Save Cabbage file as..."), File::nonexistent, String("*.csd"));
	this->setAlwaysOnTop(false);
	if(saveFC.browseForFileToSave(true)){
		csdFile = saveFC.getResult().withFileExtension(String(".csd"));
		csdFile.replaceWithText(cabbageCsoundEditor->getCurrentText());
		cabbageCsoundEditor->setCsoundFile(csdFile);
		resetFilter();
	}
	if(getPreference(appProperties, "SetAlwaysOnTop"))
		setAlwaysOnTop(true);
	else
		setAlwaysOnTop(false);

}
//==============================================================================
// Export plugin method
//==============================================================================
int StandaloneFilterWindow::exportPlugin(String type, bool saveAs)
{
File dll;
File loc_csdFile;
File thisFile(File::getSpecialLocation(File::currentApplicationFile));

if(!csdFile.exists()){
					showMessage("You need to open a Cabbage instrument before you can export one as a plugin!", lookAndFeel);
					return 0;
				}
#ifdef LINUX
	FileChooser saveFC(String("Save as..."), File::nonexistent, String(""));
	String VST;
	if (saveFC.browseForFileToSave(true)){
		if(type.contains("VSTi"))
			VST = thisFile.getParentDirectory().getFullPathName() + String("/CabbagePluginSynth.so");
		else if(type.contains(String("VST")))
			VST = thisFile.getParentDirectory().getFullPathName() + String("/CabbagePluginEffect.so");
		else if(type.contains(String("AU"))){
			showMessage("This feature only works on computers running OSX");
		}
		showMessage(VST);
		File VSTData(VST);
		if(!VSTData.exists())showMessage("lib cannot be found?");
		else{
			File dll(saveFC.getResult().withFileExtension(".so").getFullPathName());
			showMessage(dll.getFullPathName());
			if(VSTData.copyFileTo(dll))	showMessage("moved");
			File loc_csdFile(saveFC.getResult().withFileExtension(".csd").getFullPathName());
			loc_csdFile.replaceWithText(csdFile.loadFileAsString());
		}
	}
#elif WIN32
	FileChooser saveFC(String("Save plugin as..."), File::nonexistent, String("*.dll"));
	String VST;
		if(type.contains("VSTi"))
			VST = thisFile.getParentDirectory().getFullPathName() + String("\\CabbagePluginSynth.dat");
		else if(type.contains(String("VST")))
			VST = thisFile.getParentDirectory().getFullPathName() + String("\\CabbagePluginEffect.dat");

		File VSTData(VST);

		if(!VSTData.exists()){
			showMessage("Cabbage cannot find the plugin libraries. Make sure that Cabbage is situated in the same directory as CabbagePluginSynth.dat and CabbagePluginEffect.dat", lookAndFeel);
			return 0;
		}
		else{
			if(saveAs){
			if (saveFC.browseForFileToSave(true)){
			dll = saveFC.getResult().withFileExtension(".dll").getFullPathName();
			loc_csdFile = saveFC.getResult().withFileExtension(".csd").getFullPathName();
			}
			else
				return 0;
			}
			else{
			dll = csdFile.withFileExtension(".dll").getFullPathName();
			loc_csdFile = csdFile.withFileExtension(".csd").getFullPathName();
			
			}
			//showMessage(dll.getFullPathName());
			if(!VSTData.copyFileTo(dll))	
				showMessage("Problem moving plugin lib, make sure it's not currently open in your plugin host!", lookAndFeel);
			
			loc_csdFile.replaceWithText(csdFile.loadFileAsString());
			setUniquePluginID(dll, loc_csdFile);
			String info;
			info = String("Your plugin has been created. It's called:\n\n")+dll.getFullPathName()+String("\n\nIn order to modify this plugin you only have to edit the associated .csd file. You do not need to export every time you make changes.\n\nTo turn off this notice visit 'Preferences' in the main 'options' menu");
			
			int val = getPreference(appProperties, "DisablePluginInfo");
			if(!val)
			showMessage(info, lookAndFeel);
			
#ifdef Cabbage_Named_Pipe
			sendMessageToWinXound("CABBAGE_PLUGIN_FILE_UPDATE", csdFile.getFullPathName()+String("|")+loc_csdFile.getFullPathName());
			csdFile = loc_csdFile;	
			cabbageCsoundEditor->setName(csdFile.getFullPathName());
			sendMessageToWinXound("CABBAGE_SHOW_MESSAGE|Info", "WinXound has been updated\nyour .csd file");
#endif
		}

#endif
	
#if MACOSX
	
	FileChooser saveFC(String("Save as..."), File::nonexistent, String(".vst"));
	String VST;
	if (saveFC.browseForFileToSave(true)){
		//showMessage("name of file is:");
		//showMessage(saveFC.getResult().withFileExtension(".vst").getFullPathName());
				
		if(type.contains("VSTi"))
			//VST = thisFile.getParentDirectory().getFullPathName() + String("//CabbagePluginSynth.dat");
			VST = thisFile.getFullPathName()+"/Contents/CabbagePluginSynth.component";
		else if(type.contains(String("VST")))
			//VST = thisFile.getParentDirectory().getFullPathName() + String("//CabbagePluginEffect.dat");
			VST = thisFile.getFullPathName()+"/Contents/CabbagePluginEffect.component";
		else if(type.contains(String("AU"))){
			showMessage("this feature is coming soon");
			//VST = thisFile.getParentDirectory().getFullPathName() + String("\\CabbageVSTfx.component");
		}
		//showMessage(thisFile.getFullPathName()+"/Contents/CabbagePluginSynth.dat");
		String plist  = String("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		plist.append(String("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"), MAXBYTES);
		plist.append(String("<plist version=\"1.0\">"), MAXBYTES);
		plist.append(String("<dict>"), MAXBYTES);
		plist.append(String("	<key>BuildMachineOSBuild</key>"), MAXBYTES);
		plist.append(String("	<string>10K549</string>"), MAXBYTES);
		plist.append(String("	<key>CFBundleExecutable</key>"), MAXBYTES);
		plist.append(String("   <string>")+String(saveFC.getResult().getFileNameWithoutExtension())+String("</string>\n"), MAXBYTES);
		plist.append(String("	<key>CFBundleName</key>"), MAXBYTES);
		plist.append(String("   <string>")+String(saveFC.getResult().getFileNameWithoutExtension())+String("</string>\n"), MAXBYTES);
		plist.append(String("	<key>CFBundlePackageType</key>"), MAXBYTES);
		plist.append(String("	<string>APPL</string>"), MAXBYTES);
		plist.append(String("	<key>CFBundleShortVersionString</key>"), MAXBYTES);
		plist.append(String("	<string>1.0.0</string>"), MAXBYTES);
		plist.append(String("	<key>CFBundleSignature</key>"), MAXBYTES);
		plist.append(String("	<string>????</string>"), MAXBYTES);
		plist.append(String("	<key>CFBundleVersion</key>"), MAXBYTES);
		plist.append(String("	<string>1.0.0</string>"), MAXBYTES);
		plist.append(String("	<key>DTCompiler</key>"), MAXBYTES);
		plist.append(String("	<string>com.apple.compilers.llvm.clang.1_0</string>"), MAXBYTES);
		plist.append(String("	<key>DTPlatformBuild</key>"), MAXBYTES);
		plist.append(String("	<string>10M2518</string>"), MAXBYTES);
		plist.append(String("	<key>DTPlatformVersion</key>"), MAXBYTES);
		plist.append(String("	<string>PG</string>"), MAXBYTES);
		plist.append(String("	<key>DTSDKBuild</key>"), MAXBYTES);
		plist.append(String("	<string>10K549</string>"), MAXBYTES);
		plist.append(String("	<key>DTSDKName</key>"), MAXBYTES);
		plist.append(String("	<string></string>"), MAXBYTES);
		plist.append(String("	<key>DTXcode</key>"), MAXBYTES);
		plist.append(String("	<string>0400</string>"), MAXBYTES);
		plist.append(String("	<key>DTXcodeBuild</key>"), MAXBYTES);
		plist.append(String("	<string>10M2518</string>"), MAXBYTES);
		plist.append(String("	<key>NSHumanReadableCopyright</key>"), MAXBYTES);
		plist.append(String("	<string></string>"), MAXBYTES);
		plist.append(String("</dict>"), MAXBYTES);
		plist.append(String("</plist>"), MAXBYTES);
		
		/*
		String plist  = String("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		plist.append(String("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"), MAXBYTES);
		plist.append(String("<plist version=\"1.0\">\n"), MAXBYTES);
		plist.append(String("<dict>\n"), MAXBYTES);
		plist.append(String("<key>CFBundleExecutable</key>\n"), MAXBYTES);
		plist.append(String("<string>")+String(saveFC.getResult().getFileNameWithoutExtension())+String("</string>\n"), MAXBYTES);
		plist.append(String("<key>CFBundleIdentifier</key>\n"), MAXBYTES);
		plist.append(String("<string>com.Cabbage.CabbagePlugin</string>\n"), MAXBYTES);
		plist.append(String("<key>CFBundleName</key>\n"), MAXBYTES);
		plist.append(String("<string>")+String(saveFC.getResult().getFileNameWithoutExtension())+String("</string>\n"), MAXBYTES);
		plist.append(String("<key>CFBundlePackageType</key>\n"), MAXBYTES);
		plist.append(String("<string>BNDL</string>\n"), MAXBYTES);
		plist.append(String("<key>CFBundleShortVersionString</key>\n"), MAXBYTES);
		plist.append(String("<string>1.0.0</string>\n"), MAXBYTES);
		plist.append(String("<key>CFBundleSignature</key>\n"), MAXBYTES);
		plist.append(String("<string>PTul</string>\n"), MAXBYTES);
		plist.append(String("<key>CFBundleVersion</key>\n"), MAXBYTES);
		plist.append(String("<string>1.0.0</string>\n"), MAXBYTES);
		plist.append(String("</dict>\n"), MAXBYTES);
		plist.append(String("</plist>\n"), MAXBYTES);
		*/
		//create a copy of the data package and write it to the new location given by user
		File VSTData(VST);
		if(!VSTData.exists()){
			showMessage("Cabbage cannot find the plugin libraries. Make sure that Cabbage is situated in the same directory as CabbagePluginSynth.dat and CabbagePluginEffect.dat", lookAndFeel);
			return 0;
		}
		
		
		String plugType;
		if(type.contains(String("AU")))
			plugType = String(".component");
		else plugType = String(".vst");
		
		File dll(saveFC.getResult().withFileExtension(plugType).getFullPathName());
		
		VSTData.copyFileTo(dll);
		//showMessage("copied");
		
		
		
		File pl(dll.getFullPathName()+String("/Contents/Info.plist"));
		//showMessage(pl.getFullPathName());
		//pl.replaceWithText(plist);

		
		
		File loc_csdFile(dll.getFullPathName()+String("/Contents/")+saveFC.getResult().getFileNameWithoutExtension()+String(".csd"));		
		loc_csdFile.replaceWithText(csdFile.loadFileAsString());		
		//showMessage(loc_csdFile.getFullPathName());
		//showMessage(csdFile.loadFileAsString());
		csdFile = loc_csdFile;		
		
		//showMessage(VST+String("/Contents/MacOS/")+saveFC.getResult().getFileNameWithoutExtension());
		//if plugin already exists there is no point in rewriting the binaries
		if(!File(saveFC.getResult().withFileExtension(".vst").getFullPathName()+("/Contents/MacOS/")+saveFC.getResult().getFileNameWithoutExtension()).exists()){		
		File bin(dll.getFullPathName()+String("/Contents/MacOS/CabbagePlugin"));
		bin.moveFileTo(dll.getFullPathName()+String("/Contents/MacOS/")+saveFC.getResult().getFileNameWithoutExtension());
		setUniquePluginID(bin, loc_csdFile);
		}
		
		String info;
		info = String("Your plugin has been created. It's called:\n\n")+dll.getFullPathName()+String("\n\nIn order to modify this plugin you only have to edit the current .csd file. You do not need to export every time you make changes. To open the current csd file with Cabbage in another session, go to 'Open Cabbage Instrument' and select the .vst file. Cabbage will the load the associated .csd file. \n\nTo turn off this notice visit 'Preferences' in the main 'options' menu");
		
		int val = appProperties->getUserSettings()->getValue("DisablePluginInfo", var(0)).getFloatValue();
		if(!val)
			showMessage(info, lookAndFeel);		
		

	}
	
#endif		
	
	return 0;	
}

//==============================================================================
// Set unique plugin ID for each plugin based on the file name 
//==============================================================================
int StandaloneFilterWindow::setUniquePluginID(File binFile, File csdFile){
String newID;
StringArray csdText;
csdText.addLines(csdFile.loadFileAsString());
//read contents of csd file to find pluginID
for(int i=0;i<csdText.size();i++)
    {
	StringArray tokes;
	tokes.addTokens(csdText[i].trimEnd(), ", ", "\"");
	if(tokes.getReference(0).equalsIgnoreCase(String("form"))){
			CabbageGUIClass cAttr(csdText[i].trimEnd(), 0);		
			if(cAttr.getStringProp("pluginID").length()!=4){
			showMessage(String("Your plugin ID is not the right size. It MUST be 4 characters long. Some hosts may not be able to load your plugin"), lookAndFeel);
			return 0;
			}
			else{
				newID = cAttr.getStringProp("pluginID");
				i = csdText.size();
			}			
		}
	}

size_t file_size;
const char *pluginID = "YROR";

long loc;
fstream mFile(binFile.getFullPathName().toUTF8(), ios_base::in | ios_base::out | ios_base::binary);
if(mFile.is_open())
  {
	mFile.seekg (0, ios::end);
	file_size = mFile.tellg();
	//set plugin ID
	mFile.seekg (0, ios::beg);
	unsigned char* buffer = (unsigned char*)malloc(sizeof(unsigned char)*file_size);
  	mFile.read((char*)&buffer[0], file_size);
	loc = cabbageFindPluginID(buffer, file_size, pluginID);
	if (loc < 0)
		showMessage(String("Internel Cabbage Error: The pluginID was not found"));
	else {
		mFile.seekg (loc, ios::beg);	
		mFile.write(newID.toUTF8(), 4);	
	}

#ifdef WIN32
	//set plugin name based on .csd file
	const char *pluginName = "CabbageEffectNam";
	String plugLibName = csdFile.getFileNameWithoutExtension();
	if(plugLibName.length()<16)
		for(int y=plugLibName.length();y<16;y++)
			plugLibName.append(String(" "), 1);
	
	mFile.seekg (0, ios::beg);
	buffer = (unsigned char*)malloc(sizeof(unsigned char)*file_size);
  	mFile.read((char*)&buffer[0], file_size);
	loc = cabbageFindPluginID(buffer, file_size, pluginName);
	if (loc < 0)
		showMessage(String("Plugin name could not be set?!?"));
	else {
		//showMessage("plugin name set!");
		mFile.seekg (loc, ios::beg);	
		mFile.write(csdFile.getFileNameWithoutExtension().toUTF8(), 16);	
	}
#endif

}
mFile.close();
return 1;
}

//==============================================================================
// Batch process multiple csd files to convert them to plugins libs. 
//==============================================================================
void StandaloneFilterWindow::BatchProcess(String type){
File thisFile(File::getSpecialLocation(File::currentApplicationFile));
#ifdef WIN32  
FileChooser saveFC(String("Select files..."), File::nonexistent, String(""));
String VST;
	if (saveFC.browseForMultipleFilesToOpen()){
		if(type.contains("VSTi"))
			VST = thisFile.getParentDirectory().getFullPathName() + String("\\CabbagePluginSynth.dat");
		else if(type.contains(String("VST")))
			VST = thisFile.getParentDirectory().getFullPathName() + String("\\CabbagePluginEffect.dat");
		else if(type.contains(String("AU"))){
			showMessage("This feature only works on computers running OSX");
			}
	//showMessage(VST);
	File VSTData(VST);
	if(!VSTData.exists())  
		showMessage("problem with plugin lib");
	else{
		for(int i=0;i<saveFC.getResults().size();i++){
			File dll(saveFC.getResults().getReference(i).withFileExtension(".dll").getFullPathName());
		//showMessage(dll.getFullPathName());
		if(!VSTData.copyFileTo(dll))	
			showMessage("problem moving plugin lib");
		else{
		//File loc_csdFile(saveFC.getResults().getReference(i).withFileExtension(".csd").getFullPathName());
		//loc_csdFile.replaceWithText(csdFile.loadFileAsString());
		}
		}
		showMessage("Batch Convertion Complete");
	}
	}
#endif
}
