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
#include "../../JuceLibraryCode/juce_PluginHeaders.h"


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
      optionsButton ("options"), isGUIOn(false), pipeOpenedOk(false)
{
	consoleMessages = "";
	setTitleBarButtonsRequired (DocumentWindow::minimiseButton | DocumentWindow::closeButton, false);
    Component::addAndMakeVisible (&optionsButton);
    optionsButton.addListener (this);
	optionsButton.setTooltip("This is a test");
	timerRunning = false;
	yAxis = 0;
    optionsButton.setTriggeredOnMouseDown (true);
	setAlwaysOnTop(true);
	this->setResizable(false, false);

	lookAndFeel = new CabbageLookAndFeel();
	this->setLookAndFeel(lookAndFeel);

// MOD - Stefano Bonetti 
#ifdef Cabbage_Named_Pipe 
	ipConnection = new socketConnection(*this);
	pipeOpenedOk = ipConnection->createPipe(T("cabbage"));
	if(pipeOpenedOk) Logger::writeToLog(T("Namedpipe created ..."));
#endif 
// MOD - End

	JUCE_TRY
    {
        filter = createCabbagePluginFilter("", false);
		filter->addChangeListener(this);
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


    player.setProcessor (filter);
	

    ScopedPointer<XmlElement> savedState;

    if (globalSettings != nullptr)
        savedState = globalSettings->getXmlValue ("audioSetup");

    deviceManager->initialise (filter->getNumInputChannels(),
                               filter->getNumOutputChannels(),
                               savedState,
                               true);

	deviceManager->closeAudioDevice();

    if (globalSettings != nullptr)
    {
        MemoryBlock data;

        if (data.fromBase64Encoding (globalSettings->getValue ("filterState"))
             && data.getSize() > 0)
        {
            filter->setStateInformation (data.getData(), data.getSize());
        }
    }

    setContentOwned (filter->createEditorIfNeeded(), true);

    const int x = globalSettings->getIntValue ("windowX", -100);
    const int y = globalSettings->getIntValue ("windowY", -100);

    if (x != -100 && y != -100)
        setBoundsConstrained (Rectangle<int> (x, y, getWidth(), getHeight()));
    else
        centreWithSize (getWidth(), getHeight());
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

    deleteFilter();
}

//==============================================================================
// sends messages to WinXound
//==============================================================================
void StandaloneFilterWindow::sendMessageToWinXound(String messageType, String message)
{
if(pipeOpenedOk){
String text = messageType+T("|")+message;
MemoryBlock messageData (text.toUTF8(), text.getNumBytesAsUTF8());
pipeOpenedOk = ipConnection->sendMessage(messageData);
}
}

void StandaloneFilterWindow::sendMessageToWinXound(String messageType, int value)
{
if(pipeOpenedOk){
String text = messageType+T("|")+String(value);
MemoryBlock messageData (text.toUTF8(), text.getNumBytesAsUTF8());
pipeOpenedOk = ipConnection->sendMessage(messageData);
}
}

//==============================================================================
// insane Cabbage dancing....
//==============================================================================
void StandaloneFilterWindow::timerCallback()
{   
	float moveY = sin(yAxis*2*3.14*20/100); 
	float moveX = sin(yAxis*2*3.14*10/100); 
	yAxis+=1;
	this->setTopLeftPosition(this->getScreenX()+(moveX*5), this->getScreenY()+(moveY*10));
}

//==============================================================================
// action Callback - updates instrument according to changes in source code
//==============================================================================
void StandaloneFilterWindow::actionListenerCallback (const String& message){
	if(message.equalsIgnoreCase("fileSaved")){
		saveFile();
	}
	else if(message.contains("fileOpen")){
		openFile();
	}
	else if(message.contains("fileSaveAs")){
		saveFileAs();
	}
	else if(message.contains("fileExportSynth")){
	exportPlugin(T("VSTi"), false);
	}
	else if(message.contains("fileExportEffect")){
	exportPlugin(T("VST"), false);
	}
}
//==============================================================================

//==============================================================================
// listener Callback - updates WinXound compiler output with Cabbage messages
//==============================================================================
void StandaloneFilterWindow::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
String text = "";
#ifdef Cabbage_Named_Pipe
#ifdef Cabbage_GUI_Editor
if(filter->isGuiEnabled()){
if(filter->getChangeMessageType().containsIgnoreCase("GUI_edit")){
setGuiEnabled(true);
setCurrentLine(filter->getCurrentLine()+1);
sendMessageToWinXound(T("CABBAGE_FILE_UPDATED"), csdFile.getFullPathName());
sendMessageToWinXound(T("CABBAGE_UPDATE"), "");
}
else if(filter->getChangeMessageType().containsIgnoreCase("GUI_lock")){
setGuiEnabled(false);
setCurrentLine(filter->getCurrentLine()+1);
sendMessageToWinXound(T("CABBAGE_FILE_UPDATED"), csdFile.getFullPathName());
sendMessageToWinXound(T("CABBAGE_UPDATE"), "");
if(getCurrentLine()>1)
sendMessageToWinXound(T("CABBAGE_SELECT_LINE"), getCurrentLine()); 
Logger::writeToLog(String(getCurrentLine()));
}
}
else
#endif
// MOD - Stefano Bonetti
  if(filter && ipConnection->isConnected()){
      for(int i=0;i<filter->getDebugMessageArray().size();i++)
      {
          if(filter->getDebugMessageArray().getReference(i).length()>0)
          {
              text += String(filter->getDebugMessageArray().getReference(i).toUTF8());
          }
          else 
          {
              sendMessageToWinXound(T("CABBAGE_DEBUG"), "Debug message string is empty?");
              break;
          }

      }

      filter->clearDebugMessageArray();
	  sendMessageToWinXound(T("CABBAGE_DEBUG"), text);

  }
#endif
  // MOD - End

for(int i=0;i<filter->getDebugMessageArray().size();i++)
      {
          if(filter->getDebugMessageArray().getReference(i).length()>0)
          {
              text += String(filter->getDebugMessageArray().getReference(i).toUTF8());
          }
          else 
          {
              //cabbageCsoundEditor->setCsoundOutputText(text);
              break;
          }

      }

      filter->clearDebugMessageArray();
	  consoleMessages = consoleMessages+text+"\n"; 
	  if(cabbageCsoundEditor)
		  cabbageCsoundEditor->setCsoundOutputText(text+"\n");
	  //sendMessageToWinXound(T("CABBAGE_DEBUG"), text);

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
//const MessageManagerLock mmLock; 
    deleteFilter();
	deviceManager->closeAudioDevice();
	filter = createCabbagePluginFilter(csdFile.getFullPathName(), isGuiEnabled());
	//filter->suspendProcessing(isGuiEnabled());
	filter->addChangeListener(this);
	filter->sendChangeMessage();
	filter->createGUI(csdFile.loadFileAsString());
	String test = filter->getPluginName();
	setName(filter->getPluginName());

    if (filter != nullptr)
    {
        if (deviceManager != nullptr){
            player.setProcessor (filter);
			deviceManager->restartLastAudioDevice();
		}

        setContentOwned (filter->createEditorIfNeeded(), true);
    }


    PropertySet* const globalSettings = getGlobalSettings();

    if (globalSettings != nullptr)
        globalSettings->removeValue ("filterState");
	
#ifdef Cabbage_Named_Pipe
	//notify WinXound that Cabbage is set up and ready for action
	sendMessageToWinXound(T("CABBAGE_LOADED"), "");
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
	appProperties->getUserSettings()->setValue("htmlHelp", String("some directory"));
	return appProperties->getUserSettings();
}

void StandaloneFilterWindow::showAudioSettingsDialog()
{
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
	setAlwaysOnTop(true);
}
//==============================================================================
void StandaloneFilterWindow::closeButtonPressed()
{
if(cabbageCsoundEditor)
cabbageCsoundEditor->closeButtonPressed();
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

	m.addItem(1, T("Open Cabbage Instrument"));
	

	subMenu.addItem(30, T("Effect"));
	subMenu.addItem(31, T("Instrument"));
	m.addSubMenu(T("New Cabbage..."), subMenu);

	m.addItem(2, T("View Source Editor"));
    m.addItem(4, TRANS("Audio Settings..."));
    m.addSeparator();
	subMenu.clear();
	subMenu.addItem(15, TRANS("Plugin Synth"));
	subMenu.addItem(16, TRANS("Plugin Effect"));
	m.addSubMenu(TRANS("Export..."), subMenu);
#ifndef MACOSX
	subMenu.clear();
	subMenu.addItem(5, TRANS("Plugin Synth"));
	subMenu.addItem(6, TRANS("Plugin Effect"));
	m.addSubMenu(TRANS("Export Plugin As..."), subMenu);
#endif
    m.addSeparator();
	if(isAlwaysOnTop())
	m.addItem(7, T("Always on Top"), true, true);
	else
	m.addItem(7, T("Always on Top"));
	m.addItem(8, T("Update instrument"));
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
	if(timerRunning)
	m.addItem(99, T("Cabbage Dance"), true, true);
	else
	m.addItem(99, T("Cabbage Dance"));


	subMenu.clear();
	int val = appProperties->getUserSettings()->getValue("DisablePluginInfo", var(0)).getFloatValue();
	subMenu.addItem(200, "Set Csound Manual Directory");
	if(!val)
	subMenu.addItem(201, T("Disable Export Plugin Info"));
	else
	subMenu.addItem(201, T("Disable Export Plugin Info"), true, true);

	m.addSubMenu("Preferences", subMenu);
	
	
	
	

	int options = m.showAt (&optionsButton);
 
	//----- open file ------
	if(options==1){
		openFile();
	}
	//----- view text editor ------
	else if(options==2){
	if(!cabbageCsoundEditor){
	cabbageCsoundEditor = new CabbageEditorWindow(lookAndFeel);
	cabbageCsoundEditor->setVisible(false);
	cabbageCsoundEditor->addActionListener(this);
	}
	cabbageCsoundEditor->setCsoundFile(csdFile);
	this->toBehind(cabbageCsoundEditor);
	cabbageCsoundEditor->setVisible(true);
	cabbageCsoundEditor->toFront(true);
	cabbageCsoundEditor->setCsoundOutputText(consoleMessages);
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
	}
	//----- audio settings ------
   	else if(options==4){
        showAudioSettingsDialog();
		resetFilter();
	}

	//----- export ------
	else if(options==15)
	exportPlugin(T("VSTi"), false);
	
	else if(options==16)
	exportPlugin(T("VST"), false);


	//----- export as ------
	else if(options==5)
	exportPlugin(T("VSTi"), true);
	
	else if(options==6)
	exportPlugin(T("VST"), true);

	//----- always on top  ------
	else if(options==7)
	if(isAlwaysOnTop())
		this->setAlwaysOnTop(false);
	else
		this->setAlwaysOnTop(true);
	
	//----- update instrument  ------
    else if(options==8)
        resetFilter();

	/*
   	else if(options==9){
		setAlwaysOnTop(false);

		//hint.showDialog("Cabbage", 0, 0, Colours::beige, true);

		hintDialog->toFront(true);
		if(filter->getMidiDebug())
			filter->setMidiDebug(false);
		else 
			filter->setMidiDebug(true);
        setAlwaysOnTop(true);
	}*/

	//----- batch process ------
	else if(options==11)
		BatchProcess(T("VST"));

	else if(options==12)
		BatchProcess(T("VSTi"));

	//------- cabbage dance ------
	else if(options==99){
		if(!timerRunning){
		startTimer(20);
		timerRunning = true;
		}
		else{
		stopTimer();
		timerRunning = false;
		}
	}
//------- preference Csound manual dir ------
	else if(options==200){
		String dir = appProperties->getUserSettings()->getValue("CsoundHelpDir", "");
		FileChooser browser(T("Please select the Csound manual directory..."), File(dir), T("*.csd"));
		if(browser.browseForDirectory()){
			appProperties->getUserSettings()->setValue("CsoundHelpDir", browser.getResult().getFullPathName());
		}	
	}

	else if(options==201){
		int val = appProperties->getUserSettings()->getValue("DisablePluginInfo", var(0)).getFloatValue();
		if(val==0) 
			appProperties->getUserSettings()->setValue("DisablePluginInfo", var(1));
		else
			appProperties->getUserSettings()->setValue("DisablePluginInfo", var(0));
	}
	repaint();
}

//==============================================================================
// open/save/save as methods
//==============================================================================
void StandaloneFilterWindow::openFile()
{
#ifdef MACOSX
	FileChooser openFC(T("Open a Cabbage .csd file..."), File::nonexistent, T("*.csd;*.vst"));
	if(openFC.browseForFileToOpen()){
		csdFile = openFC.getResult();
		if(csdFile.getFileExtension()==(".vst")){
			String csd = csdFile.getFullPathName();
			csd << "/Contents/" << csdFile.getFileNameWithoutExtension() << ".csd";
			csdFile = File(csd);
		}
		resetFilter();
		//cabbageCsoundEditor->setCsoundFile(csdFile);
	}	
	
#else
	FileChooser openFC(T("Open a Cabbage .csd file..."), File::nonexistent, T("*.csd"));
	if(openFC.browseForFileToOpen()){
		csdFile = openFC.getResult();
		resetFilter();
		if(cabbageCsoundEditor)
		cabbageCsoundEditor->setCsoundFile(csdFile);
	}
#endif
}

void StandaloneFilterWindow::saveFile()
{
csdFile.replaceWithText(cabbageCsoundEditor->getCurrentText());
resetFilter();
}

void StandaloneFilterWindow::saveFileAs()
{
FileChooser saveFC(T("Save Cabbage file as..."), File::nonexistent, T("*.csd"));
	if(saveFC.browseForFileToSave(true)){
		csdFile = saveFC.getResult().withFileExtension(T(".csd"));
		csdFile.replaceWithText(cabbageCsoundEditor->getCurrentText());
		cabbageCsoundEditor->setCsoundFile(csdFile);
		resetFilter();
	}

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
	FileChooser saveFC(T("Save as..."), File::nonexistent, T(""));
	String VST;
	if (saveFC.browseForFileToSave(true)){
		if(type.contains("VSTi"))
			VST = thisFile.getParentDirectory().getFullPathName() + T("/CabbagePluginSynth.so");
		else if(type.contains(T("VSTfx")))
			VST = thisFile.getParentDirectory().getFullPathName() + T("CabbagePluginEffect.so");
		else if(type.contains(T("AU"))){
			showMessage("This feature only works on computers running OSX");
		}
		showMessage(VST);
		File VSTData(VST);
		if(VSTData.exists())showMessage("lib exists");
		else{
			File dll(saveFC.getResult().withFileExtension(".so").getFullPathName());
			showMessage(dll.getFullPathName());
			if(VSTData.copyFileTo(dll))	showMessage("moved");
			File loc_csdFile(saveFC.getResult().withFileExtension(".csd").getFullPathName());
			loc_csdFile.replaceWithText(csdFile.loadFileAsString());
		}
	}
#elif WIN32
	FileChooser saveFC(T("Save plugin as..."), File::nonexistent, T("*.dll"));
	String VST;
		if(type.contains("VSTi"))
			VST = thisFile.getParentDirectory().getFullPathName() + T("\\CabbagePluginSynth.dat");
		else if(type.contains(T("VST")))
			VST = thisFile.getParentDirectory().getFullPathName() + T("\\CabbagePluginEffect.dat");

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
			info = T("Your plugin has been created. It's called:\n\n")+dll.getFullPathName()+T("\n\nIn order to modify this plugin you only have to edit the associated .csd file. You do not need to export every time you make changes.\n\nTo turn off this notice visit 'Preferences' in the main 'options' menu");
			
			int val = appProperties->getUserSettings()->getValue("DisablePluginInfo", var(0)).getFloatValue();
			if(!val)
			showMessage(info, lookAndFeel);
			
#ifdef Cabbage_Named_Pipe
			sendMessageToWinXound("CABBAGE_PLUGIN_FILE_UPDATE", csdFile.getFullPathName()+T("|")+loc_csdFile.getFullPathName());
			csdFile = loc_csdFile;	
			cabbageCsoundEditor->setName(csdFile.getFullPathName());
			sendMessageToWinXound("CABBAGE_SHOW_MESSAGE|Info", "WinXound has been updated\nyour .csd file");
#endif
		}

#endif
	
#if MACOSX
	
	FileChooser saveFC(T("Save as..."), File::nonexistent, T(".vst"));
	String VST;
	if (saveFC.browseForFileToSave(true)){
		//showMessage("name of file is:");
		//showMessage(saveFC.getResult().withFileExtension(".vst").getFullPathName());
				
		if(type.contains("VSTi"))
			//VST = thisFile.getParentDirectory().getFullPathName() + T("//CabbagePluginSynth.dat");
			VST = thisFile.getFullPathName()+"/Contents/CabbagePluginSynth.dat";
		else if(type.contains(T("VST")))
			//VST = thisFile.getParentDirectory().getFullPathName() + T("//CabbagePluginEffect.dat");
			VST = thisFile.getFullPathName()+"/Contents/CabbagePluginEffect.dat";
		else if(type.contains(T("AU"))){
			showMessage("this feature is coming soon");
			//VST = thisFile.getParentDirectory().getFullPathName() + T("\\CabbageVSTfx.component");
		}
		//showMessage(thisFile.getFullPathName()+"/Contents/CabbagePluginSynth.dat");
		
		String plist  = T("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		plist.append(T("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"), MAXBYTES);
		plist.append(T("<plist version=\"1.0\">\n"), MAXBYTES);
		plist.append(T("<dict>\n"), MAXBYTES);
		plist.append(T("<key>CFBundleExecutable</key>\n"), MAXBYTES);
		plist.append(T("<string>")+String(saveFC.getResult().getFileNameWithoutExtension())+T("</string>\n"), MAXBYTES);
		plist.append(T("<key>CFBundleIdentifier</key>\n"), MAXBYTES);
		plist.append(T("<string>com.Cabbage.CabbagePlugin</string>\n"), MAXBYTES);
		plist.append(T("<key>CFBundleName</key>\n"), MAXBYTES);
		plist.append(T("<string>CabbagePlugin</string>\n"), MAXBYTES);
		plist.append(T("<key>CFBundlePackageType</key>\n"), MAXBYTES);
		plist.append(T("<string>BNDL</string>\n"), MAXBYTES);
		plist.append(T("<key>CFBundleShortVersionString</key>\n"), MAXBYTES);
		plist.append(T("<string>1.0.0</string>\n"), MAXBYTES);
		plist.append(T("<key>CFBundleSignature</key>\n"), MAXBYTES);
		plist.append(T("<string>PTul</string>\n"), MAXBYTES);
		plist.append(T("<key>CFBundleVersion</key>\n"), MAXBYTES);
		plist.append(T("<string>1.0.0</string>\n"), MAXBYTES);
		plist.append(T("</dict>\n"), MAXBYTES);
		plist.append(T("</plist>\n"), MAXBYTES);
		
		//create a copy of the data package and write it to the new location given by user
		File VSTData(VST);
		if(!VSTData.exists()){
			showMessage("Cabbage cannot find the plugin libraries. Make sure that Cabbage is situated in the same directory as CabbagePluginSynth.dat and CabbagePluginEffect.dat", lookAndFeel);
			return 0;
		}
		
		
		String plugType;
		if(type.contains(T("AU")))
			plugType = T(".component");
		else plugType = T(".vst");
		
		File dll(saveFC.getResult().withFileExtension(plugType).getFullPathName());
		
		VSTData.copyFileTo(dll);
		//showMessage("copied");
		
		
		
		File pl(dll.getFullPathName()+T("/Contents/Info.plist"));
		//showMessage(pl.getFullPathName());
		pl.replaceWithText(plist);

		
		
		File loc_csdFile(dll.getFullPathName()+T("/Contents/")+saveFC.getResult().getFileNameWithoutExtension()+T(".csd"));		
		loc_csdFile.replaceWithText(csdFile.loadFileAsString());		
		//showMessage(loc_csdFile.getFullPathName());
		//showMessage(csdFile.loadFileAsString());
		csdFile = loc_csdFile;		
		
		//showMessage(VST+T("/Contents/MacOS/")+saveFC.getResult().getFileNameWithoutExtension());
		//if plugin already exists there is no point in rewriting the binaries
		if(!File(saveFC.getResult().withFileExtension(".vst").getFullPathName()+("/Contents/MacOS/")+saveFC.getResult().getFileNameWithoutExtension()).exists()){		
		File bin(dll.getFullPathName()+T("/Contents/MacOS/CabbagePlugin"));
		bin.moveFileTo(dll.getFullPathName()+T("/Contents/MacOS/")+saveFC.getResult().getFileNameWithoutExtension());
		setUniquePluginID(bin, loc_csdFile);
		}
		
		String info;
		info = T("Your plugin has been created. It's called:\n\n")+dll.getFullPathName()+T("\n\nIn order to modify this plugin you only have to edit the current .csd file. You do not need to export every time you make changes. To open the current csd file with Cabbage in another session, go to 'Open Cabbage Instrument' and select the .vst file. Cabbage will the load the associated .csd file. \n\nTo turn off this notice visit 'Preferences' in the main 'options' menu");
		
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
	if(tokes.getReference(0).equalsIgnoreCase(T("form"))){
			CabbageGUIClass cAttr(csdText[i].trimEnd(), 0);		
			if(cAttr.getStringProp("pluginID").length()!=4){
			showMessage(T("Your plugin ID is not the right size. It MUST be 4 characters long. Some hosts may not be able to load your plugin"), lookAndFeel);
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
		showMessage(T("Internel Cabbage Error: The pluginID was not found"));
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
			plugLibName.append(T(" "), 1);
	
	mFile.seekg (0, ios::beg);
	buffer = (unsigned char*)malloc(sizeof(unsigned char)*file_size);
  	mFile.read((char*)&buffer[0], file_size);
	loc = cabbageFindPluginID(buffer, file_size, pluginName);
	if (loc < 0)
		showMessage(T("Plugin name could not be set?!?"));
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
FileChooser saveFC(T("Select files..."), File::nonexistent, T(""));
String VST;
	if (saveFC.browseForMultipleFilesToOpen()){
		if(type.contains("VSTi"))
			VST = thisFile.getParentDirectory().getFullPathName() + T("\\CabbagePluginSynth.dat");
		else if(type.contains(T("VST")))
			VST = thisFile.getParentDirectory().getFullPathName() + T("\\CabbagePluginEffect.dat");
		else if(type.contains(T("AU"))){
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