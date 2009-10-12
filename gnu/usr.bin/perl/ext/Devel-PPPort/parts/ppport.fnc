::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:
:  Perl/Pollution/Portability
:
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:
:  $Revision: 1.1 $
:  $Author: millert $
:  $Date: 2009/10/12 18:11:13 $
:
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:
:  Version 3.x, Copyright (C) 2004-2009, Marcus Holland-Moritz.
:  Version 2.x, Copyright (C) 2001, Paul Marquess.
:  Version 1.x, Copyright (C) 1999, Kenneth Albanowski.
:
:  This program is free software; you can redistribute it and/or
:  modify it under the same terms as Perl itself.
:
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

:
: This file lists all API functions/macros that are provided purely
: by Devel::PPPort. It is in the same format as the F<embed.fnc> that
: ships with the Perl source code.
:

Am	|void	|sv_magic_portable|NN SV* sv|NULLOK SV* obj|int how|NULLOK const char* name \
				|I32 namlen
