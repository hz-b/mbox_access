TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# PROD += mbox
# mbox_SRCS +=  mbox_server.cpp mbox.cpp

# PROD += mbox_threaded
# mbox_threaded_SRCS += mbox_threaded.cpp

PROD += mbox_simple
mbox_simple_SRCS += mbox_simple.cpp


PROD_LIBS += pvxs Com
PROD_SYS_LIBS += pthread # event_core
#PROD_SYS_LIBS += event_core

#PROD_SYS_LIBS_DEFAULT += event_pthreads
#PROD_SYS_LIBS_WIN32 += netapi32 ws2_32
#PROD_SYS_LIBS_vxWorks =

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
