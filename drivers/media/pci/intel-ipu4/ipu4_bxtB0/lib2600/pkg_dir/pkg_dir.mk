# 
#
# MODULE is PKG DIR

PKG_DIR_DIR  		= $${MODULES_DIR}/pkg_dir
PKG_DIR_INTERFACE	= $(PKG_DIR_DIR)/interface
PKG_DIR_SOURCES		= $(PKG_DIR_DIR)/src

PKG_DIR_FILES		= $(PKG_DIR_DIR)/src/ia_css_pkg_dir.c
PKG_DIR_CPPFLAGS	= -I$(PKG_DIR_INTERFACE)
PKG_DIR_CPPFLAGS	+= -I$(PKG_DIR_SOURCES)
PKG_DIR_CPPFLAGS	+= -I$${MODULES_DIR}/../isp/kernels/io_ls/common

PKG_DIR_CREATE_FILES	= $(PKG_DIR_DIR)/src/ia_css_pkg_dir_create.c
PKG_DIR_UPDATE_FILES    = $(PKG_DIR_DIR)/src/ia_css_pkg_dir_update.c

