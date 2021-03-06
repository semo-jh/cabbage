/*
  ==============================================================================

  Copyright (C) 2012 Rory Walsh

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

  ==============================================================================
*/

#ifndef _CSOUNDEDITORCOMPONENT_H_
#define _CSOUNDEDITORCOMPONENT_H_

#include "../../JuceLibraryCode/JuceHeader.h" 
#include "../CabbageUtils.h"
#include "../CabbageLookAndFeel.h"
#include "CabbageEditorCommandManager.h"
#include "../CabbageGUIClass.h"

#ifdef Cabbage_Build_Standalone
extern ApplicationProperties* appProperties;
#endif

class CsoundTokeniser : public CodeTokeniser
{
public:
	CsoundTokeniser(){}
	~CsoundTokeniser(){}
	 
	//==============================================================================
    enum TokenType
    {
        tokenType_error = 0,
        tokenType_comment,
        tokenType_builtInKeyword,
        tokenType_identifier,
        tokenType_integerLiteral,
        tokenType_floatLiteral,
        tokenType_stringLiteral,
        tokenType_operator,
        tokenType_bracket,
        tokenType_punctuation,
        tokenType_preprocessor,
		tokenType_csdTag
    };

	CodeEditorComponent::ColourScheme getDefaultColourScheme()
	{
		struct Type
		{
			const char* name;
			uint32 colour;
		};

		const Type types[] =
		{
			{ "Error",              Colours::black.getARGB() },
			{ "Comment",            Colours::green.getARGB() },
			{ "Keyword",            Colours::blue.getARGB() },
			{ "Identifier",         Colours::black.getARGB() },
			{ "Integer",            Colours::orange.getARGB() },
			{ "Float",              Colours::black.getARGB() },
			{ "String",             Colours::red.getARGB() },
			{ "Operator",           Colours::pink.getARGB() },
			{ "Bracket",            Colours::darkgreen.getARGB() },
			{ "Punctuation",        Colours::black.getARGB() },
			{ "Preprocessor Text",  Colours::green.getARGB() },
			{ "Csd Tag",			Colours::brown.getARGB() }
		};

		CodeEditorComponent::ColourScheme cs;

		for (int i = 0; i < sizeof (types) / sizeof (types[0]); ++i)  // (NB: numElementsInArray doesn't work here in GCC4.2)
			cs.set (types[i].name, Colour (types[i].colour));

		return cs;
	}

	CodeEditorComponent::ColourScheme getDarkColourScheme()
	{
		struct Type
		{
			const char* name;
			uint32 colour;
		};

		const Type types[] =
		{
			{ "Error",              Colours::white.getARGB() },
			{ "Comment",            Colours::green.getARGB() },
			{ "Keyword",            Colours::blue.getARGB() },
			{ "Identifier",         Colours::white.getARGB() },
			{ "Integer",            Colours::orange.getARGB() },
			{ "Float",              Colours::white.getARGB() },
			{ "String",             Colours::red.getARGB() },
			{ "Operator",           Colours::pink.getARGB() },
			{ "Bracket",            Colours::darkgreen.getARGB() },
			{ "Punctuation",        Colours::white.getARGB() },
			{ "Preprocessor Text",  Colours::green.getARGB() },
			{ "Csd Tag",			Colours::brown.getARGB() }
		};

		CodeEditorComponent::ColourScheme cs;

		for (int i = 0; i < sizeof (types) / sizeof (types[0]); ++i)  // (NB: numElementsInArray doesn't work here in GCC4.2)
			cs.set (types[i].name, Colour (types[i].colour));

		return cs;
	}


private:
	//==============================================================================
	StringArray getTokenTypes()
	{
    StringArray s;
    s.add ("Error");
    s.add ("Comment");
    s.add ("C++ keyword");
    s.add ("Identifier");
    s.add ("Integer literal");
    s.add ("Float literal");
    s.add ("String literal");
    s.add ("Operator");
    s.add ("Bracket");
    s.add ("Punctuation");
    s.add ("Preprocessor line");
	s.add ("CSD Tag");
    return s;
	}

	//==============================================================================
	void skipQuotedString (CodeDocument::Iterator& source)
	{
    const juce_wchar quote = source.nextChar();
    for (;;)
    {
        const juce_wchar c = source.nextChar();
        if (c == quote || c == 0)
            break;

        if (c == '\\')
            source.skip();
		}
	}


	//==============================================================================
    void skipCSDTag (CodeDocument::Iterator& source) noexcept
    {
        for (;;)
        {
            const juce_wchar c = source.nextChar();
            if (c == 0 || (c == '>'))
                break;
        }
    }

	//==============================================================================
	bool isIdentifierStart (const char c) 
	{
		return CharacterFunctions::isLetter (c)
				|| c == '_' || c == '@';
	}

	//==============================================================================
	bool isIdentifierBody (const char c)
	{
		return CharacterFunctions::isLetter (c)
				|| CharacterFunctions::isDigit (c)
				|| c == '_' || c == '@';
	}

	//==============================================================================
    bool isReservedKeyword (String::CharPointerType token, const int tokenLength) noexcept
    {
	//populate char array with Csound keywords
	//this list of keywords is not completely up to date! 
 		 static const char* const keywords[] =
			{ "a","abetarand", "abexprnd", "groupbox", "combobox", "vslider", "hslider", "rslider", "groupbox", "combobox", "xypad", "image", "plant", "csoundoutput", "button", "form", "checkbox", "tab", "abs","acauchy","active","adsr","adsyn","adsynt","adsynt2","aexprand","aftouch","agauss","agogobel","alinrand","alpass","ampdb","ampdbfs","ampmidi","apcauchy","apoisson","apow","areson","aresonk","atone","atonek","atonex","atrirand","aunirand","aweibull","babo","balance","bamboo","bbcutm","bbcuts","betarand","bexprnd","bformenc","bformdec","biquad","biquada","birnd","bqrez","butbp","butbr","buthp","butlp","butterbp","butterbr","butterhp","butterlp","button","buzz","cabasa","cauchy","ceil","cent","cggoto","chanctrl","changed","chani","chano","checkbox","chn","chnclear","chnexport","chnget","chnmix","chnparams","chnset","cigoto","ckgoto","clear","clfilt","clip","clock","clockoff","clockon","cngoto","comb","control","convle","convolve","cos","cosh","cosinv","cps2pch","cpsmidi","cpsmidib","cpsmidib","cpsoct","cpspch","cpstmid","cpstun","cpstuni","cpsxpch","cpuprc","cross2","crunch","ctrl14","ctrl21","ctrl7","ctrlinit","cuserrnd","dam","db","dbamp","dbfsamp","dcblock","dconv","delay","delay1","delayk","delayr","delayw","deltap","deltap3","deltapi","deltapn","deltapx","deltapxw","denorm","diff","diskin","diskin2","dispfft","display","distort1","divz","downsamp","dripwater","dssiactivate","dssiaudio","dssictls","dssiinit","dssilist","dumpk","dumpk2","dumpk3","dumpk4","duserrnd","else","elseif","endif","endin","endop","envlpx","envlpxr","event","event_i","exitnow","exp","expon","exprand","expseg","expsega","expsegr","filelen","filenchnls","filepeak","filesr","filter2","fin","fini","fink","fiopen","flanger","flashtxt","FLbox","FLbutBank","FLbutton","FLcolor","FLcolor2","FLcount","FLgetsnap","FLgroup","FLgroupEnd","FLgroupEnd","FLhide","FLjoy","FLkeyb","FLknob","FLlabel","FLloadsnap","flooper","floor","FLpack","FLpackEnd","FLpackEnd","FLpanel","FLpanelEnd","FLpanel_end","FLprintk","FLprintk2","FLroller","FLrun","FLsavesnap","FLscroll","FLscrollEnd","FLscroll_end","FLsetAlign","FLsetBox","FLsetColor","FLsetColor2","FLsetFont","FLsetPosition","FLsetSize","FLsetsnap","FLsetText","FLsetTextColor","FLsetTextSize","FLsetTextType","FLsetVal_i","FLsetVal","FLshow","FLslidBnk","FLslider","FLtabs","FLtabsEnd","FLtabs_end","FLtext","FLupdate","fluidAllOut","fluidCCi","fluidCCk","fluidControl","fluidEngine","fluidLoad","fluidNote","fluidOut","fluidProgramSelect","FLvalue","fmb3","fmbell","fmmetal","fmpercfl","fmrhode","fmvoice","fmwurlie","fof","fof2","fofilter","fog","fold","follow","follow2","foscil","foscili","fout","fouti","foutir","foutk","fprintks","fprints","frac","freeverb","ftchnls","ftconv","ftfree","ftgen","ftgentmp","ftlen","ftload","ftloadk","ftlptim","ftmorf","ftsave","ftsavek","ftsr","gain","gauss","gbuzz","gogobel","goto","grain","grain2","grain3","granule","guiro","harmon","hilbert","hrtfer","hsboscil","i","ibetarand","ibexprnd","icauchy","ictrl14","ictrl21","ictrl7","iexprand","if","igauss","igoto","ihold","ilinrand","imidic14","imidic21","imidic7","in","in32","inch","inh","init","initc14","initc21","initc7","ink","ino","inq","ins","instimek","instimes","instr","int","integ","interp","invalue","inx","inz","ioff","ion","iondur","iondur2","ioutat","ioutc","ioutc14","ioutpat","ioutpb","ioutpc","ipcauchy","ipoisson","ipow","is16b14","is32b14","islider16","islider32","islider64","islider8","itablecopy","itablegpw","itablemix","itablew","itrirand","iunirand","iweibull","jitter","jitter2","jspline","k","kbetarand","kbexprnd","kcauchy","kdump","kdump2","kdump3","kdump4","kexprand","kfilter2","kgauss","kgoto","klinrand","kon","koutat","koutc","koutc14","koutpat","koutpb","koutpc","kpcauchy","kpoisson","kpow","kr","kread","kread2","kread3","kread4","ksmps","ktableseg","ktrirand","kunirand","kweibull","lfo","limit","line","linen","linenr","lineto","linrand","linseg","linsegr","locsend","locsig","log","log10","logbtwo","loop","loopseg","loopsegp","lorenz","lorisread","lorismorph","lorisplay","loscil","loscil3","lowpass2","lowres","lowresx","lpf18","lpfreson","lphasor","lpinterp","lposcil","lposcil3","lpread","lpreson","lpshold","lpsholdp","lpslot","mac","maca","madsr","mandel","mandol","marimba","massign","maxalloc","max_k","mclock","mdelay","metro","midic14","midic21","midic7","midichannelaftertouch","midichn","midicontrolchange","midictrl","mididefault","midiin","midinoteoff","midinoteoncps","midinoteonkey","midinoteonoct","midinoteonpch","midion","midion2","midiout","midipitchbend","midipolyaftertouch","midiprogramchange","miditempo","mirror","MixerSetLevel","MixerGetLevel","MixerSend","MixerReceive","MixerClear","moog","moogladder","moogvcf","moscil","mpulse","mrtmsg","multitap","mute","mxadsr","nchnls","nestedap","nlfilt","noise","noteoff","noteon","noteondur","noteondur2","notnum","nreverb","nrpn","nsamp","nstrnum","ntrpol","octave","octcps","octmidi","octmidib octmidib","octpch","opcode","OSCsend","OSCinit","OSClisten","oscbnk","oscil","oscil1","oscil1i","oscil3","oscili","oscilikt","osciliktp","oscilikts","osciln","oscils","oscilx","out","out32","outc","outch","outh","outiat","outic","outic14","outipat","outipb","outipc","outk","outkat","outkc","outkc14","outkpat","outkpb","outkpc","outo","outq","outq1","outq2","outq3","outq4","outs","outs1","outs2","outvalue","outx","outz","p","pan","pareq","partials","pcauchy","pchbend","pchmidi","pchmidib pchmidib","pchoct","pconvolve","peak","peakk","pgmassign","phaser1","phaser2","phasor","phasorbnk","pinkish","pitch","pitchamdf","planet","pluck","poisson","polyaft","port","portk","poscil","poscil3","pow","powoftwo","prealloc","print","printf","printk","printk2","printks","prints","product","pset","puts","pvadd","pvbufread","pvcross","pvinterp","pvoc","pvread","pvsadsyn","pvsanal","pvsarp","pvscross","pvscent","pvsdemix","pvsfread","pvsftr","pvsftw","pvsifd","pvsinfo","pvsinit","pvsmaska","pvsynth","pvscale","pvshift","pvsmix","pvsfilter","pvsblur","pvstencil","pvsvoc","pyassign Opcodes","pycall","pyeval Opcodes","pyexec Opcodes","pyinit Opcodes","pyrun Opcodes","rand","randh","randi","random","randomh","randomi","rbjeq","readclock","readk","readk2","readk3","readk4","reinit","release","repluck","reson","resonk","resonr","resonx","resonxk","resony","resonz","resyn resyn","reverb","reverb2","reverbsc","rezzy","rigoto","rireturn","rms","rnd","rnd31","rspline","rtclock","s16b14","s32b14","samphold","sandpaper","scanhammer","scans","scantable","scanu","schedkwhen","schedkwhennamed","schedule","schedwhen","seed","sekere","semitone","sense","sensekey","seqtime","seqtime2","setctrl","setksmps","sfilist","sfinstr","sfinstr3","sfinstr3m","sfinstrm","sfload","sfpassign","sfplay","sfplay3","sfplay3m","sfplaym","sfplist","sfpreset","shaker","sin","sinh","sininv","sinsyn","sleighbells","slider16","slider16f","slider32","slider32f","slider64","slider64f","slider8","slider8f","sndloop","sndwarp","sndwarpst","soundin","soundout","soundouts","space","spat3d","spat3di","spat3dt","spdist","specaddm","specdiff","specdisp","specfilt","spechist","specptrk","specscal","specsum","spectrum","splitrig","spsend","sprintf","sqrt","sr","statevar","stix","strcpy","strcat","strcmp","streson","strget","strset","strtod","strtodk","strtol","strtolk","subinstr","subinstrinit","sum","svfilter","syncgrain","timedseq","tb","tb3_init","tb4_init","tb5_init","tb6_init","tb7_init","tb8_init","tb9_init","tb10_init","tb11_init","tb12_init","tb13_init","tb14_init","tb15_init","tab","tabrec","table","table3","tablecopy","tablegpw","tablei","tableicopy","tableigpw","tableikt","tableimix","tableiw","tablekt","tablemix","tableng","tablera","tableseg","tablew","tablewa","tablewkt","tablexkt","tablexseg","tambourine","tan","tanh","taninv","taninv2","tbvcf","tempest","tempo","tempoval","tigoto","timeinstk","timeinsts","timek","times","timout","tival","tlineto","tone","tonek","tonex","tradsyn","transeg","trigger","trigseq","trirand","turnoff","turnoff2","turnon","unirand","upsamp","urd","vadd","vaddv","valpass","vbap16","vbap16move","vbap4","vbap4move","vbap8","vbap8move","vbaplsinit","vbapz","vbapzmove","vcella","vco","vco2","vco2ft","vco2ift","vco2init","vcomb","vcopy","vcopy_i","vdelay","vdelay3","vdelayx","vdelayxq","vdelayxs","vdelayxw","vdelayxwq","vdelayxws","vdivv","vdelayk","vecdelay","veloc","vexp","vexpseg","vexpv","vibes","vibr","vibrato","vincr","vlimit","vlinseg","vlowres","vmap","vmirror","vmult","vmultv","voice","vport","vpow","vpowv","vpvoc","vrandh","vrandi","vstaudio","vstaudiog","vstbankload","vstedit","vstinit","vstinfo","vstmidiout","vstnote","vstparamset","vstparamget","vstprogset","vsubv","vtablei","vtablek","vtablea","vtablewi","vtablewk","vtablewa","vtabi","vtabk","vtaba","vtabwi","vtabwk","vtabwa","vwrap","waveset","weibull","wgbow","wgbowedbar","wgbrass","wgclar","wgflute","wgpluck","wgpluck2","wguide1","wguide2","wrap","wterrain","xadsr","xin","xout","xscanmap","xscansmap","xscans","xscanu","xtratim","xyin","zacl","zakinit","zamod","zar","zarg","zaw","zawm","zfilter2","zir","ziw","ziwm","zkcl","zkmod","zkr","zkw","zkwm ", 0 };


        const char* const* k;

       if (tokenLength < 2 || tokenLength > 16)
                    return false;

	   else
		  k = keywords;


        int i = 0;
        while (k[i] != 0)
        {
            if (token.compare (CharPointer_ASCII (k[i])) == 0)
                return true;

            ++i;
        }
        return false;
    }

	//==============================================================================
   int parseIdentifier (CodeDocument::Iterator& source) noexcept
    {
        int tokenLength = 0;
        String::CharPointerType::CharType possibleIdentifier [100];
        String::CharPointerType possible (possibleIdentifier);

        while (isIdentifierBody (source.peekNextChar()))
        {
            const juce_wchar c = source.nextChar();

            if (tokenLength < 20)
                possible.write (c);

            ++tokenLength;
        }

        if (tokenLength > 1 && tokenLength <= 16)
        {
            possible.writeNull();

            if (isReservedKeyword (String::CharPointerType (possibleIdentifier), tokenLength))
                return CsoundTokeniser::tokenType_builtInKeyword;
        }

        return CsoundTokeniser::tokenType_identifier;
    }

   //==============================================================================
	int readNextToken (CodeDocument::Iterator& source)
	{
    int result = tokenType_error;
    source.skipWhitespace();
    char firstChar = source.peekNextChar();

    switch (firstChar)
    {
    case 0:
        source.skip();
        break;

    case ';':
		source.skipToEndOfLine();
        result = tokenType_comment;
        break;

	case '"':
 //   case T('\''):
    	skipQuotedString (source);
       result = tokenType_stringLiteral;
       break;

	case '<':
		source.skip();
		if((source.peekNextChar() == 'C') ||
			(source.peekNextChar() == '/')){
		skipCSDTag (source);
        result = tokenType_csdTag;
		}
		break;

    default:
		if (isIdentifierStart (firstChar)){
            result = parseIdentifier (source);
		}
        else
            source.skip();
        break;
    }
    //jassert (result != tokenType_unknown);
    return result;
	}
};

//==============================================================================
class CsoundEditor  :  public Component,
					   public MenuBarModel,
					   public ActionBroadcaster,
					   public ApplicationCommandTarget,
					   public CodeDocument::Listener,
					   public CabbageUtils
{

private:
	
	File openCsdFile;

public:
	class helpContext : public Component
	{
	String helpText;
	Colour background, text, outline;
	Font font;
	public:
		helpContext(){}
		~helpContext(){}

		void paint(Graphics& g){
                       g.fillAll(background);
                       g.setColour(outline);
                       int width = getWidth();
                       int height = getHeight();
                       g.drawRect(1, 1, getWidth()-2, getHeight()-2);
                       g.setColour(text);
                       Justification just(33);
                       g.drawFittedText(helpText, 10, 10, width-10, height-25, just, 5, 0.2);
			}

		void resized(){
			repaint();
		}

		void setHelpText(String textString){
			helpText = textString;
			repaint();
		}

		void setHelpFont(Font fonttype){
			font = fonttype;
		}		

		void setBackgroundColour(Colour colour){
			background = colour;
		}

		void setTextColour(Colour colour){
		text = colour;
		}

		void setOutlineColour(Colour colour){
		outline = colour;
		}
	};

	class helpPopup : public Component
	{
	String helpText;
	Colour background, text, outline;
	Font font;
	public:
		helpPopup(){}
		~helpPopup(){}

		void paint(Graphics& g){
                       g.fillAll(background);
                       g.setColour(outline);
                       int width = getWidth();
                       int height = getHeight();
                       g.drawRect(1, 1, getWidth()-2, getHeight()-2);
					   g.setColour(Colours::black);
                       Justification just(33);
                       g.drawFittedText(helpText, 10, 10, width-10, height-25, just, 5, 0.2);
			}

		void resized(){
			repaint();
		}

		void setPopupText(String textString){
			helpText = textString;
			repaint();
		}

	};

	class CsoundCodeEditor : public CodeEditorComponent,
							public ChangeListener
	{
		class colourPallete : public ColourSelector
		{
			Array <Colour> swatchColours;
		
		    public:			
			colourPallete(): ColourSelector(){
			swatchColours.add(Colour(0xFF000000));
			swatchColours.add(Colour(0xFFFFFFFF));
			swatchColours.add(Colour(0xFFFF0000));
			swatchColours.add(Colour(0xFF00FF00));
			swatchColours.add(Colour(0xFF0000FF));
			swatchColours.add(Colour(0xFFFFFF00));
			swatchColours.add(Colour(0xFFFF00FF));
			swatchColours.add(Colour(0xFF00FFFF));
			swatchColours.add(Colour(0x80000000));
			swatchColours.add(Colour(0x80FFFFFF));
			swatchColours.add(Colour(0x80FF0000));
			swatchColours.add(Colour(0x8000FF00));
			swatchColours.add(Colour(0x800000FF));
			swatchColours.add(Colour(0x80FFFF00));
			swatchColours.add(Colour(0x80FF00FF));
			swatchColours.add(Colour(0x8000FFFF));
			};

			~colourPallete(){};

			/*int getNumSwatches() const 
			{
			return swatchColours.size();
			}

			Colour getSwatchColour(int index) const 
			{
				return swatchColours[index];
			}

			void setSwatchColour (int index, const Colour &newColour)
			{
				swatchColours.insert(index, newColour);
			}*/
		};


	public:
			CodeDocument::Position positionInCode;
			CsoundCodeEditor(CodeDocument &document, CodeTokeniser *codeTokeniser)
					: CodeEditorComponent(document, codeTokeniser)
			{
				setColour(CodeEditorComponent::highlightColourId, Colours::cornflowerblue); 
				xPos = 10;
				yPos = 10;
			}
			~CsoundCodeEditor(){};

	void 	addPopupMenuItems (PopupMenu &menuToAddTo, const MouseEvent *mouseClickEvent){
		menuToAddTo.addItem(1, "Cut");
		menuToAddTo.addItem(1, "Copy");
		menuToAddTo.addItem(1, "Paste");
		menuToAddTo.addItem(1, "Select All");
		menuToAddTo.addSeparator();
		menuToAddTo.addItem(1, "Undo");
		menuToAddTo.addItem(1, "Redo");
		menuToAddTo.addItem(10, "Colour selector");
		xPos = mouseClickEvent->x;
		yPos = mouseClickEvent->y;
	};

	void 	performPopupMenuAction (int menuItemID){
 	if(menuItemID==10){
			//method for adding colours directly to code from popup colour selector
			pos1 = getDocument().findWordBreakBefore(getCaretPos());
			pos2 = getDocument().findWordBreakAfter(getCaretPos());
			String identifierName = getDocument().getTextBetween(pos1, pos2);
			identifierName = identifierName.removeCharacters("\"(), ");
			identifierName = identifierName.trim();
			//CabbageUtils::showMessage(identifierName);
			//identifierName = identifierName.substring(0, identifierName.indexOf("("));
			String line = getDocument().getLine(pos1.getLineNumber());
			String identifierString = line.substring(line.indexOf(identifierName));
			identifierString = identifierString.substring(0, identifierString.indexOf(")")+1);

			if((identifierName=="colour") ||
				(identifierName=="fontcolour")||
				(identifierName=="outline")||
				(identifierName=="tracker")){
					colourPallete colourSelector;
					//colourSelector.setLookAndFeel(&getLookAndFeel());
					colourSelector.setBounds(xPos, yPos, 200, 200);
					colourSelector.addChangeListener (this);   
					Rectangle<int> rectum;
					CallOutBox callOut (colourSelector, rectum, nullptr);
					callOut.setTopLeftPosition(xPos+20, yPos);
			
					callOut.runModalLoop();
					StringArray csoundFile;

					csoundFile.addLines(getDocument().getAllContent());

					CabbageGUIClass cAttr(line, -90);
					int r = selectedColour.getRed();
					int g = selectedColour.getGreen();
					int b = selectedColour.getBlue();
					int a = selectedColour.getAlpha();
			
					String newColour = identifierName + "(" + String(r) + ", " + String(g) + ", " + String(b) + ", " + String(a) + ")";
					line = CabbageUtils::replaceIdentifier(line, identifierName, newColour);
					line = line.removeCharacters("\n");
			
					csoundFile.set(pos1.getLineNumber(), line);
					getDocument().replaceAllContent(csoundFile.joinIntoString("\n"));
					int newCursorPos = line.indexOf(newColour);
					Range<int> range;
					range.setStart(getDocument().getAllContent().indexOf(line)+newCursorPos);
					range.setEnd(getDocument().getAllContent().indexOf(line)+newCursorPos+newColour.length());
					setHighlightedRegion(range);
			}
			else
				CabbageUtils::showMessage("No valid colur identifier found? Please use a valid identifer and try again. (See any of the colour entries in the Cabbage help file for details on using the colour selector)", &getLookAndFeel());
	}
	};

	void changeListenerCallback(juce::ChangeBroadcaster *source){
		ColourSelector* cs = dynamic_cast <ColourSelector*> (source);
		selectedColour = cs->getCurrentColour();
	};

	

	private:
		int xPos, yPos;
		CodeDocument::Position pos1, pos2;
		Colour selectedColour;
	};



	int fontSize;
	ScopedPointer<TabbedComponent> tabComp;
	ScopedPointer<WebBrowserComponent> htmlHelp;
	CodeDocument csoundDoc;
	CsoundTokeniser csoundToker;
	ScopedPointer<CsoundCodeEditor> textEditor;
	ScopedPointer<TextEditor> output;
	ScopedPointer<CabbageLookAndFeel> lookAndFeel;
	ScopedPointer<helpContext> helpLabel;
	ScopedPointer<helpPopup> helpPopupMenu;
	StretchableLayoutManager horizontalLayout;
    ScopedPointer<StretchableLayoutResizerBar> horizontalDividerBar;
	void setFontSize(String zoom);
	void setHelpText(String text){
		helpLabel->setHelpText(text);
	}

	void codeDocumentTextInserted(const juce::String &,int){}
	void codeDocumentTextDeleted(int,int){}

	CommandManager commandManager;
    //==============================================================================
	juce_UseDebuggingNewOperator;
    CsoundEditor ();
	ScopedPointer<OldSchoolLookAndFeel> oldLookAndFeel;
    ~CsoundEditor ();
    //==============================================================================
	void codeDocumentChanged (const CodeDocument::Position &affectedTextStart, const CodeDocument::Position &affectedTextEnd);
	//=============================================================================
	void resized ();
	void setCurrentFile(File input);
	void highlightLine(String line){
		String temp = textEditor->getDocument().getAllContent();
		Range<int> range;
		range.setStart(temp.indexOf(line));
		range.setEnd(temp.indexOf(line)+line.length());
		textEditor->setHighlightedRegion(range);
	}
	String getCurrentText();
	void newFile(String type);
	bool hasChanged;
	bool unSaved;
	//================= command methods ====================
	ApplicationCommandTarget* getNextCommandTarget(){
		return findFirstTargetParentComponent();
	}
	void getAllCommands(Array <CommandID>& commands);
	void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result);
	bool perform(const InvocationInfo& info);
	PopupMenu getMenuForIndex(int topLevelMenuIndex, const String& menuName);

	//==============================================================================
    StringArray getMenuBarNames()
    {
        const char* const names[] = { "File", "Edit", "Help", 0 };
        return StringArray (names);
    }


    void menuItemSelected (int /*menuItemID*/, int /*topLevelMenuIndex*/){}
	StringArray opcodes;

};

#endif//_CSOUNDEDITORCOMPONENT_H_
