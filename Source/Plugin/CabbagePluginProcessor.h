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

#ifndef __PLUGINPROCESSOR_H_FE85D052__
#define __PLUGINPROCESSOR_H_FE85D052__

#include "../../JuceLibraryCode/JuceHeader.h"
#include "../../JuceLibraryCode/JucePluginCharacteristics.h"
#include "../CabbageUtils.h"
#include "../CabbageGUIClass.h"

#ifndef Cabbage_No_Csound
#include "csound.hpp"
#endif

//==============================================================================
// CabbagePluginAudioProcessor definition
//==============================================================================
class CabbagePluginAudioProcessor  : public AudioProcessor,
									 public CabbageUtils,
									 public ChangeBroadcaster,
									 public ChangeListener,
									 public Timer
{
public:
    //==============================================================================
#ifdef Cabbage_Build_Standalone
    CabbagePluginAudioProcessor(String inputfile, bool guiOnOff);
#else
	CabbagePluginAudioProcessor();
#endif
    ~CabbagePluginAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void releaseResources();

    void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);

    //==============================================================================
    AudioProcessorEditor* createEditor();
    bool hasEditor() const;

    //==============================================================================
    const String getName() const;

    int getNumParameters();

    float getParameter (int index);
    void setParameter (int index, float newValue);

    const String getParameterName (int index);
    const String getParameterText (int index);

    const String getInputChannelName (int channelIndex) const;
    const String getOutputChannelName (int channelIndex) const;
    bool isInputChannelStereoPair (int index) const;
    bool isOutputChannelStereoPair (int index) const;

    bool acceptsMidi() const;
    bool producesMidi() const;

    //==============================================================================
    int getNumPrograms();
    int getCurrentProgram();
    void setCurrentProgram (int index);
    const String getProgramName (int index);
    void changeProgramName (int index, const String& newName);

    //==============================================================================
    void getStateInformation (MemoryBlock& destData);
    void setStateInformation (const void* data, int sizeInBytes);
	//==============================================================================
	File getCsoundInputFile(){
		return csdFile;
	}

	inline String getCsoundInputFileText(){
		return csdFile.loadFileAsString();
	}

	void updateCsoundFile(String text){
	csdFile.replaceWithText(text);
	}
	
	String getDebugMessage(){
	return debugMessage;
	}

	void addDebugMessage(String message){
		debugMessageArray.add(message);;
	}

	StringArray getDebugMessageArray(){
	return debugMessageArray;
	}

	void clearDebugMessageArray(){
		debugMessageArray.clear();
	}

	int getCompileStatus(){ 
		return csCompileResult;
	}

	void clearDebugMessage(){
		debugMessage="";
	}

	void setPluginName(String name){
	pluginName = name;	
	}

	String getPluginName(){
	return pluginName;
	}

	void setMidiDebug(bool val){
		showMIDI=val;
	}
	bool getMidiDebug(){
		return showMIDI;
	}
	//======== log information about GUI controls ===============
	StringArray logGUIAttributes(CabbageGUIClass cAttr, String type){  
		StringArray arr;
		arr.add(String("----------- ")+type+String(" -----------"));
		arr.add(String("Name:")+cAttr.getStringProp("name")+String(", Type:")+cAttr.getStringProp("type")+String(", Caption:")+cAttr.getStringProp("caption")+String(", RelToPlant:")+cAttr.getStringProp("reltoplant")+String(", Plant:")+cAttr.getStringProp("plant"));
		arr.add(String("PosX:")+String(cAttr.getNumProp("left"))+String(", PosY:")+String(cAttr.getNumProp("top"))+String(", Width:")+String(cAttr.getNumProp("width"))+T(", Height:")+String(cAttr.getNumProp("height")));
		arr.add(T("   "));
		Logger::writeToLog(T("----------- ")+type+T(" -----------"));
		Logger::writeToLog(T("Name:")+cAttr.getStringProp("name")+T(", Type:")+cAttr.getStringProp("type")+T(", Caption:")+cAttr.getStringProp("caption")+String(", RelToPlant:")+cAttr.getStringProp("reltoplant")+String(", Plant:")+cAttr.getStringProp("plant"));
		Logger::writeToLog(T("PosX:")+String(cAttr.getNumProp("left"))+T(", PosY:")+String(cAttr.getNumProp("top"))+T(", Width:")+String(cAttr.getNumProp("width"))+T(", Height:")+String(cAttr.getNumProp("height")));
		Logger::writeToLog("   ");
		return arr;
	}
	inline bool getCsoundStatus(){
		return csoundStatus;
	}

	//===========================================================
	inline int getGUICtrlsSize(){
		return (int)guiCtrls.size();
	}

	inline int getGUILayoutCtrlsSize(){
		return (int)guiLayoutCtrls.size();
	}

	inline CabbageGUIClass getGUILayoutCtrls(int index){
		return guiLayoutCtrls.getReference(index);
	}

	inline CabbageGUIClass getGUICtrls(int index){
		return guiCtrls.getReference(index);
	}

	inline String getChangeMessageType(){
		return changeMessageType;
	}

	inline String getCsoundOutput(){
		return csoundOutput;
	}

	inline void setChangeMessageType(String text){
		changeMessageType = text;
	}

	inline int getCurrentLine(){
		return currentLine;
	}

	inline void setCurrentLine(int line){
		currentLine = line;
	}

#ifndef Cabbage_No_Csound
	Csound* getCsound(){
		return csound;
	}

	CSOUND* getCsoundStruct(){
		return csound->GetCsound();
	}

	MYFLT getCSScale(){
		return cs_scale;
	}

#endif

	bool isGuiEnabled(){
		return guiOnOff;
	}

	void setGuiEnabled(bool onoff){
		guiOnOff = onoff;
	}

	void createGUI(String source);
	MidiKeyboardState keyboardState;
	//midiBuffers
	MidiBuffer midiBuffer;		
	MidiBuffer ccBuffer;
	bool showMIDI;
	
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabbagePluginAudioProcessor);
	File csdFile;
	String filename;
	String pluginName;
	bool csoundStatus;
	int csCompileResult;
	void timerCallback();
	String csoundOutput;
	void changeListenerCallback(ChangeBroadcaster *source);
	String changeMessageType;
	bool guiOnOff;
	int currentLine;

	//============== Csound related variables/methods ==============================
#ifndef Cabbage_No_Csound
	MYFLT cs_scale;
	ScopedPointer<Csound> csound;				//Csound instance
	MYFLT *CSspin, *CSspout;	//Csound audio IO pointers
	int csndIndex;				//Csound sample counter
	int CSCompResult;			//result of Csound performKsmps
	CsoundChannelListEntry* csoundChanList;  	// list of all available channels...
	int numCsoundChannels;		//number of Csound channels
	static void messageCallback(CSOUND *csound, int attr, 
								const char *fmt, va_list args);  //message callback function
	int pos;
	//Csound API functions for deailing with midi input
	static int OpenMidiInputDevice(CSOUND * csnd, void **userData, const char *devName);
	static int OpenMidiOutputDevice(CSOUND * csnd, void **userData, const char *devName);
	static int ReadMidiData(CSOUND *csound, void *userData, unsigned char *mbuf, int nbytes);
	static int WriteMidiData(CSOUND *csound, void *userData, const unsigned char *mbuf, int nbytes);
#endif
	StringArray debugInfo;
	Array<CabbageGUIClass, CriticalSection> guiLayoutCtrls;
	Array<CabbageGUIClass, CriticalSection> guiCtrls;
	String plantFlag;
	String debugMessage;
	StringArray debugMessageArray;


};

#endif  // __PLUGINPROCESSOR_H_FE85D052__