TEMPLATE = subdirs

SUBDIRS += \
    server \
    client

# Work around MinGW make/cmd Unicode-path issues on Windows by ensuring the
# sub-project .pro paths passed to qmake are relative (ASCII-only).
server.file = server/server.pro
client.file = client/client.pro
