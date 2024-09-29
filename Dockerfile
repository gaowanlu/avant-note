FROM ubuntu:latest
RUN mkdir -p /avant
COPY . /avant
WORKDIR /avant
RUN apt-get update && apt-get install -y apt-utils
RUN apt-get install cmake g++ make git -y
RUN apt-get install protobuf-compiler libprotobuf-dev  -y
RUN apt-get install libssl-dev dos2unix pandoc -y
WORKDIR /avant/bin
RUN git clone https://github.com/gaowanlu/note.git note
WORKDIR /avant/bin/note
RUN chmod +x convert_md_to_html.sh
RUN dos2unix convert_md_to_html.sh
RUN ./convert_md_to_html.sh
WORKDIR /avant
RUN rm -rf CMakeCache.txt \
    && cd protocol \
    && make \
    && cd .. \
    && mkdir build \
    && cd build \
    && cmake .. \
    && make -j3 \
    && cd .. \
    && cd bin \
    && ls
WORKDIR /avant/bin
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["./avant && tail -f /dev/null"]
