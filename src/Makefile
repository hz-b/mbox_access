TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================


PROD += demo_client
demo_client_SRCS += demo_client.cpp

PROD += demo_server
demo_server_SRCS += demo_server.cpp

PROD_LIBS += pvxs Com
#PROD_SYS_LIBS += event_core

#PROD_SYS_LIBS_DEFAULT += event_pthreads
#PROD_SYS_LIBS_WIN32 += netapi32 ws2_32
#PROD_SYS_LIBS_vxWorks =

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

