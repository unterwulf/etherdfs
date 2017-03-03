;
; Place all segments into the same group (DGROUP), so all offsets
; will have the same base segment (i.e. CS = DS = SS).
;

TSRDATA		segment byte public 'TSR'
TSRDATA		ends

TSRTEXT		segment byte public 'TSR'
TSRTEXT		ends

_TEXT		segment word public 'CODE'
_TEXT		ends

TEXT		segment word public 'CODE'
TEXT		ends

BEGTEXT		segment word public 'CODE'
BEGTEXT		ends

DGROUP		group TSRDATA, TSRTEXT, BEGTEXT, TEXT, _TEXT

		end
