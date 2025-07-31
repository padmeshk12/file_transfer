# List of subdirectories for which to call make
SUBDIR_LIST=tools

# All targets are just handed through to the subdirectories
.DEFAULT:
	@for subdir in $(SUBDIR_LIST);\
	do \
		parentDir=`pwd`;\
		echo '{';\
		echo 'Current directory is:' $$parentDir;\
		echo 'Entering subdirectory' $$parentDir/$$subdir ;\
		echo 'calling make $@ ...' ;\
		if [ -d $$subdir ] && cd $$subdir ; then \
			$(MAKE) $@;\
			if [ $$? -ne 0 ]; then exit 1; fi;\
		else \
			echo subdirectory $$parentDir/$$subdir does not exist, ignored; \
		fi ; \
		cd $$parentDir;\
		echo 'Finished make $@ in subdirectory' $$parentDir/$$subdir .;\
		echo '}';\
	done
