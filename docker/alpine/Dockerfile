FROM alpine:3.18 as common

# Based on:
# https://github.com/mundialis/docker-grass-gis/blob/master/Dockerfile
LABEL authors="Carmen Tawalika,Pietro Zambelli,Markus Neteler"
LABEL maintainer="neteler@osgeo.org"

# PACKAGES VERSIONS
ARG PYTHON_VERSION=3

# List of packages to be installed (proj-data omitted: 570.04 MB)
ENV GRASS_RUN_PACKAGES="\
      attr \
      build-base \
      bash \
      bison \
      bzip2 \
      cairo \
      curl \
      fftw \
      flex \
      freetype \
      g++ \
      gcc \
      gdal \
      gdal-dev \
      gdal-driver-GMLAS \
      gdal-driver-HDF5 \
      gdal-driver-JP2OpenJPEG \
      gdal-driver-LIBKML \
      gdal-driver-MSSQLSpatial \
      gdal-driver-netCDF \
      gdal-driver-ODBC \
      gdal-driver-PG \
      gdal-driver-PNG \
      gdal-driver-WMS \
      gdal-tools \
      gettext \
      geos \
      geos-dev \
      git \
      gnutls \
      jsoncpp \
      laszip \
      libbz2 \
      libgeotiff \
      libjpeg-turbo \
      libpng \
      libpq-dev \
      libunwind \
      make \
      musl \
      musl-utils \
      ncurses \
      openjpeg \
      openblas \
      py3-numpy \
      py3-pillow \
      py3-six \
      pdal \
      pdal-dev \
      postgresql15-client \
      proj-util \
      sqlite \
      sqlite-libs \
      subversion \
      tiff \
      zstd \
      zstd-libs \
    "
# ====================
# INSTALL DEPENDENCIES
# ====================

WORKDIR /src

ENV PYTHONBIN=python$PYTHON_VERSION

RUN echo "Install Python";\
    apk add --no-cache $PYTHONBIN && \
    $PYTHONBIN -m ensurepip && \
    rm -r /usr/lib/python*/ensurepip && \
    pip$PYTHON_VERSION install --upgrade pip setuptools && \
    if [ ! -e /usr/bin/pip ]; then ln -s pip$PYTHON_VERSION /usr/bin/pip ; fi && \
    if [ ! -e /usr/bin/python ]; then ln -sf /usr/bin/$PYTHONBIN /usr/bin/python; fi && \
    rm -r /root/.cache; \
    # Add the packages
    echo "Install main packages";\
    apk update; \
    apk add --no-cache $GRASS_RUN_PACKAGES


FROM common as build

# ================
# CONFIG VARIABLES
# ================

# set configuration options, without wxGUI
ENV GRASS_CONFIG="\
      --enable-largefile \
      --with-cxx \
      --with-proj-share=/usr/share/proj \
      --with-gdal \
      --with-pdal \
      --with-geos \
      --with-sqlite \
      --with-bzlib \
      --with-zstd \
      --with-cairo --with-cairo-ldflags=-lfontconfig \
      --with-fftw \
      --with-postgres --with-postgres-includes=/usr/include/postgresql \
      --with-openmp \
      --without-freetype \
      --without-opengl \
      --without-nls \
      --without-mysql \
      --without-odbc \
      "

# Set environmental variables for GRASS GIS compilation, without debug symbols
ENV MYCFLAGS="-O2 -std=gnu99 -m64" \
    MYLDFLAGS="-s -Wl,--no-undefined -lblas" \
    # CXX stuff:
    LD_LIBRARY_PATH="/usr/local/lib" \
    LDFLAGS="$MYLDFLAGS" \
    CFLAGS="$MYCFLAGS" \
    CXXFLAGS="$MYCXXFLAGS" \
    NUMTHREADS=2

# These packages are required to compile GRASS GIS.
ENV GRASS_BUILD_PACKAGES="\
      build-base \
      bzip2-dev \
      cairo-dev \
      fftw-dev \
      freetype-dev \
      geos-dev \
      git \
      gnutls-dev \
      libc6-compat \
      libjpeg-turbo-dev \
      libpng-dev \
      libpq-dev \
      openjpeg-dev \
      openblas-dev \
      pdal \
      pdal-dev \
      proj-dev \
      python3-dev \
      py3-numpy-dev \
      sqlite-dev \
      tar \
      tiff-dev \
      unzip \
      vim \
      wget \
      zip \
      zstd-dev \
    "

# Add the packages
RUN echo "Install main packages";\
    # Add packages just for the GRASS build process
    apk add --no-cache --virtual .build-deps $GRASS_BUILD_PACKAGES
    # echo LANG="en_US.UTF-8" > /etc/default/locale;

# Copy and install GRASS GIS
COPY . /src/grass_build/
WORKDIR /src/grass_build/

# Configure compile and install GRASS GIS
RUN echo "  => Configure and compile grass" && \
    /src/grass_build/configure $GRASS_CONFIG && \
    make -j $NUMTHREADS && \
    make install && \
    ldconfig /etc/ld.so.conf.d

# Get rid of version number here, restore in next stage via symbolic link
RUN mv $(grass --config path) /usr/local/grass

# Reduce the image size - Remove unnecessary grass files
RUN cp /usr/local/grass/gui/wxpython/xml/module_items.xml module_items.xml; \
    rm -rf /usr/local/grass/demolocation; \
    rm -rf /usr/local/grass/fonts; \
    rm -rf /usr/local/grass/gui; \
    rm -rf /usr/local/grass/share; \
    mkdir -p /usr/local/grass/gui/wxpython/xml/; \
    mv module_items.xml /usr/local/grass/gui/wxpython/xml/module_items.xml;

RUN git clone https://github.com/OSGeo/gdal-grass /src/gdal-grass
WORKDIR /src/gdal-grass
RUN ./configure \
    --with-gdal=/usr/bin/gdal-config \
    --with-grass=/usr/local/grass && \
    make -j $NUMTHREADS && \
    make install -j $NUMTHREADS


FROM common as grass

# GRASS GIS specific
# allow work with MAPSETs that are not owned by current user
ENV GRASSBIN="/usr/local/bin/grass" \
    GRASS_SKIP_MAPSET_OWNER_CHECK=1 \
    SHELL="/bin/bash" \
    # https://proj.org/usage/environmentvars.html#envvar-PROJ_NETWORK
    PROJ_NETWORK=ON \
    GRASSBIN=grass \
    LC_ALL="en_US.UTF-8"

# Copy GRASS GIS and GDAL GRASS driver from build image
COPY --from=build /usr/local/bin/grass /usr/local/bin/grass
COPY --from=build /usr/local/grass* /usr/local/grass/
COPY --from=build /usr/lib/gdalplugins/*_GRASS.so /usr/lib/gdalplugins/
# run simple LAZ test
COPY docker/testdata/simple.laz /tmp/
COPY docker/testdata/test_grass_session.py docker/alpine/grass_tests.sh /scripts/
COPY docker/testdata/test_grass_session.py /scripts/

# install external Python API
RUN pip3 install --upgrade pip six grass-session --ignore-installed six; \
    ln -sf /usr/local/grass $(grass --config path); \
    # run some tests and cleanup
    $SHELL /scripts/grass_tests.sh \
    && rm -f /scripts/grass_tests.sh /tmp/simple.laz /scripts/test_grass_session.py; \
    # delete unused packages
    apk del --no-cache gettext pdal-dev; \
    # show installed version
    grass --tmp-location XY --exec g.version -rge \
    && pdal --version \
    && python3 --version

# Data workdir
WORKDIR /grassdb
VOLUME /grassdb

CMD ["$GRASSBIN", "--version"]
