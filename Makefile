################################################################################
# Automatically-generated file. Do not edit!
################################################################################

DAEMON_NAME = des-olsr
DESTDIR ?=
DIR_BIN=$(DESTDIR)/usr/sbin
DIR_ETC=$(DESTDIR)/etc
DIR_ETC_DEF=$(DIR_ETC)/default
DIR_ETC_INITD=$(DIR_ETC)/init.d

CONFIG+=debug

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include src/pipeline/subdir.mk
-include src/subdir.mk
-include src/database/topology_set/subdir.mk
-include src/database/subdir.mk
-include src/database/routing_table/subdir.mk
-include src/database/routing_calculation/subdir.mk
-include src/database/rl_seq_t/subdir.mk
-include src/database/neighbor_set/subdir.mk
-include src/database/local_iface_set/subdir.mk
-include src/database/link_set/subdir.mk
-include src/database/duplicate_table/subdir.mk
-include src/database/broadcast_log/subdir.mk
-include src/database/2hop_neighbor_set/subdir.mk
-include src/cli/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: des-olsr

# Tool invocations
des-olsr: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc  -o"des-olsr" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Installation
install:
	mkdir -p $(DIR_BIN)
	install -m 755 $(DAEMON_NAME) $(DIR_BIN)
	mkdir -p $(DIR_ETC)
	install -m 666 etc/$(addsuffix .conf,$(DAEMON_NAME)) $(DIR_ETC)
	mkdir -p $(DIR_ETC_DEF)
	install -m 644 etc/default/$(DAEMON_NAME) $(DIR_ETC_DEF)
	mkdir -p $(DIR_ETC_INITD)
	install -m 755 etc/init.d/$(DAEMON_NAME) $(DIR_ETC_INITD)

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) des-olsr
	-@echo ' '

run: des-olsr
	./des-olsr

-include ../makefile.targets
