#pragma once
#ifdef  ss_compile_RD
void RD_init();
void RD_StartCapture();
void RD_EndCapture();
#else
void RD_init() {}
void RD_StartCapture() {}
void RD_EndCapture() {}
#endif //  ss_compile_RD

