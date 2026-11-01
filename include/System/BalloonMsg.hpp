#ifndef SYSTEM_BALLOONMSG_HPP
#define SYSTEM_BALLOONMSG_HPP

#ifdef VERSION_GMSP01
#define BALLOON_MSG_PAL_(n) \
	((n) >= 0x31 ? (n) + 3 : (n) >= 0x28 ? (n) - 3 : (n))
#define BALLOON_MSG(id) BALLOON_MSG_PAL_((id) & 0xFFFF)
#else
#define BALLOON_MSG(id) (id)
#endif

#endif // SYSTEM_BALLOONMSG_HPP
