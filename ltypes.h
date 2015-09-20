/* ltypes.h */

#if !defined(__LTYPES_H__)
#define __LTYPES_H__

#if !defined(FALSE)
#define FALSE 0
#endif

#if !defined(TRUE)
#define TRUE !(FALSE)
#endif

typedef signed char bool;
typedef void(*FUNCPTR)(void);

#endif /* __LTYPES_H__ */
