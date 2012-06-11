#include "precompiled.h"

double			Sys_GetClockTicks( void ) {
	return 0.f;
}
double			Sys_ClockTicksPerSecond( void ) {
	return 0.f;
}
cpuid_t			Sys_GetProcessorId_2( void ) {
	return CPUID_NONE;
}
// sets Flush-To-Zero mode (only available when CPUID_FTZ is set)
void			Sys_FPU_SetFTZ( bool enable ) {

}
// sets Denormals-Are-Zero mode (only available when CPUID_DAZ is set)
void			Sys_FPU_SetDAZ( bool enable ) {

}
bool			Sys_UnlockMemory( void *ptr, int bytes ) {
	return 0;
}
bool			Sys_LockMemory( void *ptr, int bytes ) {
	return 0;
}