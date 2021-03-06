#define VT_OPENQRY	0x5600	/* find available vt */

struct vt_mode {
	char mode;		/* vt mode */
	char waitv;		/* if set, hang on writes if not active */
	short relsig;		/* signal to raise on release req */
	short acqsig;		/* signal to raise on acquisition */
	short frsig;		/* unused (set to 0) */
};

#define VT_GETMODE	0x5601	/* get mode of active vt */
#define VT_SETMODE	0x5602	/* set mode of active vt */
#define		VT_AUTO		0x00	/* auto vt switching */
#define		VT_PROCESS	0x01	/* process controls switching */
#define		VT_ACKACQ	0x02	/* acknowledge switch */

struct vt_stat {
	unsigned short active;	/* active vt */
	unsigned short signal;	/* signal to send */
	unsigned short state;		/* vt bitmask */
};

#define VT_GETSTATE	0x5603	/* get global vt state info */
#define VT_SENDSIG	0x5604	/* signal to send to bitmask of vts */

#define VT_RELDISP	0x5605	/* release display */

#define VT_ACTIVATE	0x5606	/* make vt active */
#define VT_WAITACTIVE	0x5607	/* wait for vt active */
#define VT_DISALLOCATE	0x5608  /* free memory associated to vt */

#define VT_LOCKSWITCH   0x560B  /* disallow vt switching */
#define VT_UNLOCKSWITCH 0x560C  /* allow vt switching */

#define KDSETMODE	0x4B3A
#define KDGETMODE	0x4B3B
#define		KD_TEXT		0x00
#define		KD_GRAPHICS	0x01

#define		K_RAW		0x00
#define		K_XLATE		0x01
#define		K_MEDIUMRAW	0x02
#define		K_UNICODE	0x03
#define		K_OFF		0x04
#define KDGKBMODE	0x4B44	/* gets current keyboard mode */
#define KDSKBMODE	0x4B45	/* sets current keyboard mode */

#define KDSKBMUTE	0x4B51
