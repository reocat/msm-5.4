# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_SERVICEBUS) += service_bus.o

service_bus-objs :=  \
	bus_manager.o \
	mc_repository.o \
	wn_repository.o \
	message_queue.o \
	message_container.o \
	name_parser.o \
	servicebus_entry.o \
	servicebus_interface.o \
	wait_notify.o \
	event_source.o \
	event_listener.o \
	el_logger.o \
	el_dlt_logger.o \
	el_wait_notify.o \
	el_bus_status.o \
	el_reg_unreg.o \
	el_service_status.o \
	servicebus_if_ioctl.o

ccflags-y += -DBUSBUSMNG
ccflags-y += -DIMX_ZANTEI
