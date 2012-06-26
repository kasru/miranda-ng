/*
  Copyright (C) 1999, 2000, 2002 Aladdin Enterprises.  All rights reserved.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  L. Peter Deutsch
  ghost@aladdin.com

 */
/* $Id: md5.c 2874 2006-05-16 21:38:00Z ghazan $ */
/*
  Independent implementation of MD5 (RFC 1321).

  This code implements the MD5 Algorithm defined in RFC 1321, whose
  text is available at
  http://www.ietf.org/rfc/rfc1321.txt
  The code is derived from the text of the RFC, including the test suite
  (section A.5) but excluding the rest of Appendix A.  It does not include
  any code or documentation that is identified in the RFC as being
  copyrighted.

  The original and principal author of md5.c is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  2002-04-13 lpd Clarified derivation from RFC 1321; now handles byte order
  either statically or dynamically; added missing #include <string.h>
  in library.
  2002-03-11 lpd Corrected argument list for main(), and added int return
  type, in test program and T value program.
  2002-02-21 lpd Added missing #include <stdio.h> in test program.
  2000-07-03 lpd Patched to eliminate warnings about "constant is
  unsigned in ANSI C, signed in traditional"; made test program
  self-checking.
  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5).
  1999-05-03 lpd Original version.
 */

// (C) 2005 Joe @ Whale - changed to compile with Miranda

#include "..\..\core\commonheaders.h"

struct MD5_INTERFACE
{
	int cbSize;
	void (*md5_init) (mir_md5_state_t *pms);
	void (*md5_append) (mir_md5_state_t *pms, const mir_md5_byte_t *data, int nbytes);
	void (*md5_finish) (mir_md5_state_t *pms, mir_md5_byte_t digest[16]);
	void (*md5_hash) (const mir_md5_byte_t *data, int len, mir_md5_byte_t digest[16]);
};

INT_PTR GetMD5Interface(WPARAM, LPARAM lParam)
{
	struct MD5_INTERFACE *md5i = (struct MD5_INTERFACE*) lParam;
	if (md5i == NULL)
		return 1;
	if (md5i->cbSize != sizeof(struct MD5_INTERFACE))
		return 1;

	md5i->md5_init = mir_md5_init;
	md5i->md5_append = mir_md5_append;
	md5i->md5_finish = mir_md5_finish;
	md5i->md5_hash = mir_md5_hash;
	return 0;
}
