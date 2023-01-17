/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

// Portions of this code come from the following open source works

/*
 * gen_uuid.c --- generate a DCE-compatible uuid
 *
 * Copyright (C) 1996, 1997, 1998, 1999 Theodore Ts'o.
 *
 * %Begin-Header%
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * %End-Header%
 */

#include "libuuid.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

// Linux system headers
#include <inttypes.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <unistd.h>

// Our targets/SDKs should have this file
#define HAVE_NET_IF_H 1
#include <net/if.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Get the ethernet hardware address, if we can find it...
 *
 * XXX for a windows version, probably should use GetAdaptersInfo:
 * http://www.codeguru.com/cpp/i-n/network/networkinformation/article.php/c5451
 * commenting out get_node_id just to get gen_uuid to compile under windows
 * is not the right way to go!
 */
int uuid_get_node_id(unsigned char *node_id)
{
#ifdef HAVE_NET_IF_H
	int		sd;
	struct ifreq	ifr, *ifrp;
	struct ifconf	ifc;
	char buf[1024];
	int		n, i;
	unsigned char	*a = NULL;
#ifdef HAVE_NET_IF_DL_H
	struct sockaddr_dl *sdlp;
#endif

/*
 * BSD 4.4 defines the size of an ifreq to be
 * max(sizeof(ifreq), sizeof(ifreq.ifr_name)+ifreq.ifr_addr.sa_len
 * However, under earlier systems, sa_len isn't present, so the size is
 * just sizeof(struct ifreq)
 */
#ifdef HAVE_SA_LEN
#define ifreq_size(i) max(sizeof(struct ifreq),\
     sizeof((i).ifr_name)+(i).ifr_addr.sa_len)
#else
#define ifreq_size(i) sizeof(struct ifreq)
#endif /* HAVE_SA_LEN */

	sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sd < 0) {
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl (sd, SIOCGIFCONF, (char *)&ifc) < 0) {
		close(sd);
		return -1;
	}
	n = ifc.ifc_len;
	for (i = 0; i < n; i+= ifreq_size(*ifrp) ) {
		ifrp = (struct ifreq *)((char *) ifc.ifc_buf+i);
		strncpy(ifr.ifr_name, ifrp->ifr_name, IFNAMSIZ);
#ifdef SIOCGIFHWADDR
		if (ioctl(sd, SIOCGIFHWADDR, &ifr) < 0)
			continue;
		a = (unsigned char *) &ifr.ifr_hwaddr.sa_data;
#else
#ifdef SIOCGENADDR
		if (ioctl(sd, SIOCGENADDR, &ifr) < 0)
			continue;
		a = (unsigned char *) ifr.ifr_enaddr;
#else
#ifdef HAVE_NET_IF_DL_H
		sdlp = (struct sockaddr_dl *) &ifrp->ifr_addr;
		if ((sdlp->sdl_family != AF_LINK) || (sdlp->sdl_alen != 6))
			continue;
		a = (unsigned char *) &sdlp->sdl_data[sdlp->sdl_nlen];
#else
		/*
		 * XXX we don't have a way of getting the hardware
		 * address
		 */
		close(sd);
		return 0;
#endif /* HAVE_NET_IF_DL_H */
#endif /* SIOCGENADDR */
#endif /* SIOCGIFHWADDR */
		if (a == NULL || (!a[0] && !a[1] && !a[2] && !a[3] && !a[4] && !a[5]))
			continue;
		if (node_id) {
			memcpy(node_id, a, 6);
			close(sd);
			return 1;
		}
	}
	close(sd);
#endif
	return 0;
}

#ifdef __cplusplus
}
#endif
