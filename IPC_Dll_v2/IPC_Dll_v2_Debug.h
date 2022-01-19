#pragma once
//#define DEBUG

#ifdef DEBUG
#define LOG_INFO(fmt,...) do{printf("INFO:	%s	%s	%s	Line:%d:	" fmt,__TIME__,__FILE__,__func__,__LINE__,__VA_ARGS__);}while(0)
#define LOG_ERROR(fmt,...) do{printf("ERROR:	%s	%s	%s	Line:%d:	" fmt,__TIME__,__FILE__,__func__,__LINE__,__VA_ARGS__);}while(0)
#else
#define LOG_INFO(fmt,...) do{}while(0)
#define LOG_ERROR(fmt,...) do{}while(0)
#endif


