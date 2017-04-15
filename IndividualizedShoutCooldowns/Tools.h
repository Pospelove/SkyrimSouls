#ifndef _TOOLS_H
#define _TOOS_H

#include <windows.h>

#define START_ASM(id, startAddr, endAddr, isReturn)				\
			__asm{												\
			__asm push dword ptr isReturn						\
			__asm push ASM_##id##_END							\
			__asm push ASM_##id##_START							\
			__asm push  endAddr									\
			__asm push  startAddr								\
			__asm call WriteHook								\
			__asm jmp ASM_##id##_SKIP							\
		__asm ASM_##id##_START:		

#define JMP_ASM(id)											\
		__asm jmp ASM_##id##_END							\

#define END_ASM(id)											\
		__asm ASM_##id##_END:								\
			__asm nop										\
			__asm nop										\
			__asm nop										\
			__asm nop										\
			__asm nop										\
		__asm ASM_##id##_SKIP:								\
		}

void __stdcall WriteHook(int sourceBegin, int sourceEnd, int targetBegin, int targetEnd, bool isReturn)
{
	DWORD oldProtect = 0;

	int len1 = sourceEnd - sourceBegin;
	if (VirtualProtect((void *)sourceBegin, len1, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		*((unsigned char *)sourceBegin) = (unsigned char)0xE9;
		*((int *)(sourceBegin + 1)) = (int)targetBegin - sourceBegin - 5;
		for (int i = 5; i < len1; i++)
			*((unsigned char *)(i + sourceBegin)) = (unsigned char)0x90;
		VirtualProtect((void *)sourceBegin, len1, oldProtect, &oldProtect);
		if (isReturn && VirtualProtect((void *)targetEnd, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			*((unsigned char *)targetEnd) = (unsigned char)0xE9;
			*((int *)(targetEnd + 1)) = (int)sourceEnd - targetEnd - 5;
			VirtualProtect((void *)targetEnd, 5, oldProtect, &oldProtect);
		}

	}
}

#endif