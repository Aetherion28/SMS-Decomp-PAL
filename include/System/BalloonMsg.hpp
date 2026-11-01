#ifndef SYSTEM_BALLOONMSG_HPP
#define SYSTEM_BALLOONMSG_HPP

#ifdef VERSION_GMSP01
#define BALLOON_MSG(id) ((id) & 0xFFFF)
#else
#define BALLOON_MSG(id) (id)
#endif

#endif // SYSTEM_BALLOONMSG_HPP
