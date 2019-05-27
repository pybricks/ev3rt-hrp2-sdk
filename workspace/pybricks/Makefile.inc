APPL_COBJS += 

COPTS += -DPYBRICKS

MICROPYTHON_PATH = $(APPLDIR)/../../../../../../../../

INCLUDES += -I$(MICROPYTHON_PATH)
INCLUDES += -I$(MICROPYTHON_PATH)/ports/pybricks/bricks/ev3rt/
INCLUDES += -I$(MICROPYTHON_PATH)/ports/pybricks/bricks/ev3rt/build/
LIBS += -L$(MICROPYTHON_PATH)/ports/pybricks/bricks/ev3rt/build/ -lmicropython
