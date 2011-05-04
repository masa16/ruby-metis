require "mkmf"

# configure options:
#  --with-metis-dir=path
#  --with-metis-include=path
#  --with-metis-lib=path

metis_top = "../metis-5*"
libdir = Dir.glob(metis_top+"/build/*")[0]
incdir = Dir.glob(metis_top+"/include")[0]

dir_config("metis",incdir,libdir)

have_header("metis.h")

have_library("metis")

create_makefile("metis")
