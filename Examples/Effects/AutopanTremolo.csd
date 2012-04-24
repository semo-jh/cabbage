<Cabbage>
form caption("Autopan / Tremolo") size(380, 110)
image pos(0, 0), size(360, 90), colour("Maroon"), shape("rounded"), outline("white"), line(4) 
rslider bounds(10, 11, 70, 70), text("Rate"), channel("rate"), range(0.1, 50, 0.5, 0.5)
rslider bounds(75, 11, 70, 70), text("Depth"), channel("depth"), range(0, 1, 1, 0.5)
combobox bounds(150, 20, 120,25), channel("mode"), size(100,50), value(1), text("Autopan", "Tremolo")
combobox bounds(150, 50, 120,25), channel("wave"), size(100,50), value(1), text("Sine", "Triangle", "Square")
rslider bounds(280, 11, 70, 70), text("Level"), channel("level"), range(0, 1, 1)
}
</Cabbage>
<CsoundSynthesizer>
<CsOptions>
-d -n
</CsOptions>
<CsInstruments>
sr = 44100
ksmps = 32
nchnls = 2
0dbfs = 1

;Author: Iain McCurdy (2012)

opcode	PanTrem,aa,aakkkK
	ainL,ainR,krate,kdepth,kmode,kwave	xin	;READ IN INPUT ARGUMENTS
	ktrig	changed	kwave				;IF LFO WAVEFORM TYPE IS CHANGED GENERATE A MOMENTARY '1' (BANG)
	if ktrig=1 then					;IF A 'BANG' HAS BEEN GENERATED IN THE ABOVE LINE
		reinit	UPDATE				;BEGIN A REINITIALIZATION PASS FROM LABEL 'UPDATE' SO THAT LFO WAVEFORM TYPE CAN BE UPDATED
	endif						;END OF THIS CONDITIONAL BRANCH
	UPDATE:						;LABEL CALLED UPDATE
	iwave	init		i(kwave)
	iwave	limit	iwave,	0,2
	klfo	lfo	kdepth, krate, iwave		;CREATE AN LFO
	rireturn					;RETURN FROM REINITIALIZATION PASS
	klfo	=	(klfo*0.5)+0.5			;RESCALE AND OFFSET LFO SO IT STAY WITHIN THE RANGE 0 - 1 ABOUT THE VALUE 0.5
	if kwave=2 then					;IF SQUARE WAVE MODULATION HAS BEEN CHOSEN...
		klfo	portk	klfo, 0.001		;SMOOTH THE SQUARE WAVE A TINY BIT TO PREVENT CLICKS
	endif						;END OF THIS CONDITIONAL BRANCH	
	if kmode=0 then	;PAN				;IF PANNING MODE IS CHOSEN FROM BUTTON BANK...
		alfo	interp	klfo			;INTERPOLATE K-RATE LFO AND CREATE A-RATE VARIABLE
		aoutL	=	ainL*sqrt(alfo)		;REDEFINE GLOBAL AUDIO LEFT CHANNEL SIGNAL WITH AUTO-PANNING
		aoutR	=	ainR*(1-sqrt(alfo))	;REDEFINE GLOBAL AUDIO RIGHT CHANNEL SIGNAL WITH AUTO-PANNING
	elseif kmode=1 then	;TREM			;IF TREMELO MODE IS CHOSEN FROM BUTTON BANK...
		klfo	=	klfo+(0.5-(kdepth*0.5))	;MODIFY LFO AT ZERO DEPTH VALUE IS 1 AND AT MAX DEPTH CENTRE OF MODULATION IS 0.5
		alfo	interp	klfo			;INTERPOLATE K-RATE LFO AND CREATE A-RATE VARIABLE
		aoutL	=	ainL*(alfo^2)		;REDEFINE GLOBAL AUDIO LEFT CHANNEL SIGNAL WITH TREMELO
		aoutR	=	ainR*(alfo^2)		;REDEFINE GLOBAL AUDIO RIGHT CHANNEL SIGNAL WITH TREMELO
	endif						;END OF THIS CONDITIONAL BRANCH
		xout	aoutL,aoutR			;SEND AUDIO BACK TO CALLER INSTRUMENT
endop

instr 1
krate chnget "rate"
kdepth chnget "depth"
kmode chnget "mode"
kwave chnget "wave"
klevel chnget "level"
a1,a2	ins
a1,a2	PanTrem	a1,a2,krate,kdepth,kmode-1,kwave-1	
a1	=	a1 * klevel
a2	=	a2 * klevel
	outs	a1,a2
endin

</CsInstruments>

<CsScore>
i 1 0 [60*60*24*7]
</CsScore>

</CsoundSynthesizer>