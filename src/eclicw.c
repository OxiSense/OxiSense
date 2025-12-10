#include <stdlib.h>
#include "eclicw.h"
#include "gd32vf103.h"

typedef void (*isr_t)(void);
static isr_t *jtbl;
static uint32_t max_irqn=0;

void eclicw_enable(int irqn, int level, int priority, void (*pISR)(void)){

   if (!max_irqn) {                             // Init Int vector table if not done!
        max_irqn = *( volatile uint32_t * )( ECLIC_ADDR_BASE + ECLIC_INFO_OFFSET );
        max_irqn &= ( 0x00001FFF );             // Nb of imp. int. in 13 lsb.
        //eclic_init( max_irqn );                 // Init ECLIC datast. (All int. dis.)
        //eclic_mode_enable();                    // Enable ECLIC & Vectore mode.
        jtbl = (isr_t *)malloc(sizeof(isr_t *)*max_irqn); // Space for isr addresses. 
        //eclic_priority_group_set(ECLIC_PRIGROUP_LEVEL3_PRIO1); // level 0..7, prio 0..1!
   }

   eclic_clear_pending( irqn );                 // Make very sure pending flag is zero!
   eclic_irq_enable(irqn, level, priority);     // Tell ECLIC selected level and priority!
   eclic_set_vmode( irqn );                     // Manage the int through the vector table.
   eclic_enable_interrupt( irqn );              // Enable the selected interrupt.

   jtbl[irqn]=pISR;                             // Remember what to call!!!
}

int eclicw_pending(int irqn){
  return *(volatile uint8_t*)(ECLIC_ADDR_BASE+ECLIC_INT_IP_OFFSET+irqn*4);
}
/*
__attribute__( ( interrupt ) )
void eclic_mtip_handler( void ) {               // c-wrapper saves environment...
  jtbl[CLIC_INT_TMR]();                         // ...Call int's ISR...
}                                               // and restores environment (also (G)IE)!
*/
__attribute__( ( interrupt ) )                  // !!! ALL USART0 INT GOES HERE !!!
void USART0_IRQHandler( void ) {                // c-wrapper saves environment...
  jtbl[USART0_IRQn]();                          // ...Call int's ISR...
}                                               // and restores environment (also (G)IE)!

__attribute__( ( interrupt ) )                  // !!! ALL EXTI5..9 INT GOES HERE !!!
void EXTI5_9_IRQHandler( void ) {               // c-wrapper saves environment...
  jtbl[EXTI5_9_IRQn]();                         // ...Call int's ISR...
}                                               // and restores environment (also (G)IE)!
