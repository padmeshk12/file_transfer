#-----------------------------------------------------------------------------#
# Legend:
#
# all         - build program (objects, library)
# debug       - build program (objects, library) with debug option
# clean       - remove any object files
# clobber     - remove anything which can be regenerated
# depend      - update dependency list
#
#-----------------------------------------------------------------------------#

-include .common_settings

# list of subdirectories for the handler driver framework
HFRAMEWORK = tools log guiServer config mhcom diagnostics estate state frame tcom event testtools

#list of subdirectories for specific handler driver plugins
REGULAR_HDRIVER = Delta Multitest Seiko-Epson Synax Yokogawa Tesec Mirae TechWing Rasco SRM YAC Advantest Aetrium Ismeca MCT SPEA OSAI Esmo Pyramid ASM Atom DLH
HDRIVER = h_template $(REGULAR_HDRIVER)

#the directories to be built. !Don't change the order!
BUILD_DIRS = $(HFRAMEWORK) $(HDRIVER) libgeneric_93k_driver

DEST=.

SH_LIBS= -lhw_cor_gio -lhw_cor_gio -ldriver_dev_kits
SYS_LIBS= -ldl

LD=/usr/bin/g++ -m64

LDFLAGS= -Wl,-fatal-warnings -Wl,-melf_x86_64

default: all

.DEFAULT:
	echo $(REGULAR_HDRIVER) > .plugin_list
	if [ $@ = clobber ]; then rm -rf .plugin_list; fi
	@for subdir in $(BUILD_DIRS);\
	do \
		parentDir=`pwd`;\
		echo '{';\
		echo 'Current directory is:' $$parentDir;\
		echo 'Entering subdirectory' $$parentDir/$$subdir ;\
		echo 'calling make $@ ...' ;\
		if [ -d $$subdir ] && cd $$subdir ; then \
			$(MAKE) $@;\
			if [ $$? -ne 0 ]; then exit 1; fi; \
		else \
			echo subdirectory $$parentDir/$$subdir does not exist, ignored; \
		fi ; \
		cd $$parentDir;\
		echo 'Finished make $@ in subdirectory' $$parentDir/$$subdir .;\
		echo '}';\
	done
