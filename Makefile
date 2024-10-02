BUILD_PRX=0

PSPSDK=$(shell psp-config --pspsdk-path)
PSPDIR=$(shell psp-config --psp-prefix)

TARGET = rawgl_psp
OBJS = aifcplayer.o file.o main.o resource.o resource_win31.o script.o video.o \
bitmap.o mixer.o resource_3do.o scaler.o sfxplayer.o unpack.o \
engine.o graphics_soft.o pak.o resource_nth.o screenshot.o staticres.o util.o systemstub_psp.o graphics_psp.o menu.o graphics_common.o

CFLAGS = -O2 -Wall -I/usr/local/pspdev/psp/include/SDL2/ -DBYPASS_PROTECTION
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LIBS = -lSDL2_mixer -lvorbisfile -lvorbis -logg -lxmp-lite -lSDL2main -lSDL2 -lSDL2main -lSDL2 -lm -lGL -lpspvram -lpspaudio -lpspvfpu -lpspdisplay -lpspgu -lpspge -lpsphprm -lpspctrl -lpsppower -lstdc++ -lz -lpspdmac -lpspgum
LDFLAGS =

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Another World engine (rawgl_psp)

include $(PSPSDK)/lib/build.mak

