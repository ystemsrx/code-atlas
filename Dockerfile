FROM conanio/gcc10

WORKDIR /app

# Copy build scripts and configuration files, setting the owner to the 'conan' user
COPY --chown=conan:conan CMakeLists.txt conanfile.txt build.sh config_template.json ./

# Copy source code and headers, setting the owner to the 'conan' user
COPY --chown=conan:conan src/ ./src/
COPY --chown=conan:conan include/ ./include/

# Grant execution permissions to the build script
RUN chmod +x build.sh

# Run the build script to compile the application
RUN ./build.sh

# Set the command to run the application
CMD ["./build/code-atlas"] 