;
; End of TSR part marker. This file should be linked last.
;

TSRTEXT		segment byte public 'TSR'
public _tsr_end_label
_tsr_end_label:
TSRTEXT		ends

		end
