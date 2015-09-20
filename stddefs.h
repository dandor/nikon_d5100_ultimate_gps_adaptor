/* stddefs.h - local project-wide, but not project-specific, definitions */

#if !defined(__STDDEFS_H__)
#define __STDDEFS_H__

#include <avr/interrupt.h>

#define TRUE 1
#define FALSE 0

#define IRQMASK_SAVE uint8_t sreg = SREG; cli()  /* save IRQ mask state */
#define IRQMASK_RESTORE SREG = sreg /* restore IRQ mask state (-may- re-enable IRQs) */

#define IRQLOCK cli()
#define IRQUNLOCK sei()

#define NELEMS(a) ( sizeof(a) / sizeof( (a)[0] ) )

#endif /* __STDDEFS_H__ */
