sources = "client-chat.c"
CFLAGS = ["-Wall", "-Wextra", "-Werror", "-g"]

env = Environment (CCFLAGS = CFLAGS)
obj = env.Object(source=sources)
env.Program (target="client-chat", source = obj)

CFLAGSBIN = CFLAGS + ["-DBIN"]

env_bin = Environment (CCFLAGS = CFLAGSBIN)
obj_bin = env_bin.Object(target="client-chat-bin.o",source = sources)
env_bin.Program (target="client-chat-bin",source= obj_bin)

CFLAGSFILEIO = CFLAGS + ["-DFILEIO"]

env_fileio = Environment (CCFLAGS = CFLAGSFILEIO)
obj_fileio = env_fileio.Object(target="client-chat-fileio.o",source = sources)
env_fileio.Program (target="client-chat-fileio",source= obj_fileio)

CFLAGSUSR = CFLAGS + ["-DUSR"]

env_usr = Environment (CCFLAGS = CFLAGSUSR)
obj_usr = env_usr.Object(target="client-chat-usr.o",source = sources)
env_usr.Program (target="client-chat-usr",source= obj_usr)