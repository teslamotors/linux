GCOV_PROFILE := n

# List of files in the vdso, has to be asm only for now

obj-vdso32 = vgettimeofday.o $(obj-vdso32-y)

# Build rules

targets := $(obj-vdso32) vdso32.so vdso32.so.dbg
obj-vdso32 := $(addprefix $(obj)/, $(obj-vdso32))

GCOV_PROFILE := n

ccflags-y := -shared -fPIC -fno-common -fno-builtin -march=armv7-a
ccflags-y += -nostdlib -Wl,-soname=linux-vdso32.so.1 \
		$(call cc-ldoption, -Wl$(comma)--hash-style=sysv)
asflags-y := -D__VDSO32__ -s

obj-y += vdso32_wrapper.o
extra-y += vdso32.lds
CPPFLAGS_vdso32.lds += -P -C

# Force dependency (incbin is bad)
$(obj)/vdso32_wrapper.o : $(obj)/vdso32.so

# link rule for the .so file, .lds has to be first
$(obj)/vdso32.so.dbg: $(src)/vdso32.lds $(obj-vdso32)
	$(call if_changed,vdso32ld)

# strip rule for the .so file
$(obj)/%.so: OBJCOPYFLAGS := -S
$(obj)/%.so: $(obj)/%.so.dbg FORCE
	$(call if_changed,objcopy)

# assembly rules for the .S files
$(obj-vdso32): %.o: %.c
	$(call if_changed_dep,vdso32cm)

# filter out options that the aarch32 compiler does not allow
bad-aaarch32-flags := -mgeneral-regs-only -mno-android -pg -Werror=date-time -Qunused-arguments -mno-global-merge
lc_flags = $(filter-out $(bad-aaarch32-flags),$(c_flags))

# actual build commands
quiet_cmd_vdso32ld = VDSO32L $@
      cmd_vdso32ld = $(CROSS32CC) $(lc_flags) -Wl,-T $^ -o $@
quiet_cmd_vdso32as = VDSO32A $@
      cmd_vdso32as = $(CROSS32CC) $(a_flags) -c -o $@ $<
quiet_cmd_vdso32cm = VDSO32C $@
      cmd_vdso32cm = $(CROSS32CC) $(lc_flags) -c -o $@ $<

# install commands for the unstripped file
quiet_cmd_vdso_install = INSTALL $@
      cmd_vdso_install = cp $(obj)/$@.dbg $(MODLIB)/vdso/$@

vdso32.so: $(obj)/vdso32.so.dbg
	@mkdir -p $(MODLIB)/vdso
	$(call cmd,vdso_install)

vdso_install: vdso32.so
