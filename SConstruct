sources = "client-chat.c"
CFLAGS = ["-Wall", "-Wextra", "-Werror", "-g"]

env = Environment (CCFLAGS = CFLAGS)
obj = env.Object(source=sources)
env.Program (target="client-chat", source = obj)