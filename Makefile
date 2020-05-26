DIRS = nomount justmount search mkdir rmdir persist createfile filedata \
        stamfs-standalone utils

all:
	@for DIR in $(DIRS); do \
		echo "Making 'all' in directory '$$DIR'..."; \
		$(MAKE) -C $$DIR all; \
	done

clean:
	@for DIR in $(DIRS); do \
		echo "Making 'clean' in directory '$$DIR'..."; \
		$(MAKE) -C $$DIR clean; \
	done
