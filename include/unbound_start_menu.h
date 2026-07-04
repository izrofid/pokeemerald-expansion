#ifndef GUARD_UNBOUND_START_MENU_H
#define GUARD_UNBOUND_START_MENU_H

#include "gba/types.h"

void Usm_InitStartMenu(void);
void Usm_LoadIconPalette(void);
bool8 FieldCB_UsmReturnToField(void);
void Usm_ReturnToFieldOpenMenu(void);
void CB2_ReturnToFieldWithOpenUsm(void);

#endif // GUARD_UNBOUND_START_MENU_H
