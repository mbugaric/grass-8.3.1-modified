./configure \
 --with-libs=/usr/lib64 \
 --with-cxx \
 --without-ffmpeg \
 --with-gdal=o/bin/gdal-config \
 --without-odbc \
 --with-sqlite \
 --with-postgres \
 --without-mysql \
 --with-nls \
 --with-python \
 --with-cairo \
 --with-wxwidgets=/usr/bin/wx-config \
 --without-fftw \
 --with-freetype --with-freetype-includes=/usr/include/freetype2 \
 --enable-largefile \
 --with-pthread
make
make install