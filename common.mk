# --------------------------------------------------------------------------
# Functions
#

# Create a directory if it doesn't exist
ifeq ($(OS),Windows_NT)
define mkdir_if_needed
	powershell If (!(Test-Path '$(1)')) { [void](New-Item -ItemType Directory -Path '$(1)') }
endef
else
define mkdir_if_needed
	mkdir -p '$(1)'
endef
endif


# Delete a file if it exists and do nothing otherwise
ifeq ($(OS),Windows_NT)
define rm_if_exists
	powershell If (Test-Path '$(1)') { Remove-Item -Recurse -Force '$(1)' }
endef
else
define rm_if_exists
	rm -rf '$(1)'
endef
endif


# Copy a file or directory (recursively)
ifeq ($(OS),Windows_NT)
define copy
	powershell Copy-Item -Path '$(1)' -Destination '$(2)' -Recurse
endef
else
define copy
	cp -Rp '$(1)' '$(2)'
endef
endif
