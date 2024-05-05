FROM alpine:3.18.6
WORKDIR /usr/local/app
COPY srv .
RUN chmod u+x srv && apk add libc6-compat 
CMD ["./srv", "8080"]
