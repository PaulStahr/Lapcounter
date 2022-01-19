CFLAGS= -Wall -Wextra -pedantic -g -O2 -fopenmp -std=c++14
CFLAGS += -DNDEBUG

SRC := src
BUILD := build
DOC := doc

CC:=g++

UNAME := $(shell uname -m)
ifeq ($(UNAME), armv7l)
ALLEGRO_FLAGS := $(shell pkg-config --cflags --libs allegro-5.0 allegro_main-5.0 allegro_primitives-5.0 allegro_ttf-5.0 allegro_font-5.0 allegro_audio-5.0 allegro_acodec-5.0 allegro_image-5.0)
CFLAGS += -DLEDSTRIPE
endif
ifeq ($(UNAME), x86_64)
ALLEGRO_FLAGS := $(shell pkg-config --cflags --libs allegro-5 allegro_main-5 allegro_primitives-5 allegro_ttf-5 allegro_font-5 allegro_audio-5 allegro_acodec-5 allegro_image-5)
endif

CFILES:=serialize.cpp  server.cpp loghtml.cpp options.cpp query_performance_counter.cpp performance_counter.cpp data.cpp tournee_plan.cpp tournee_plan_creator.cpp tournee_plan_approximation_creator.cpp tournee_plan_brute_force_creator.cpp firework.cpp input.cpp
OFILES:=$(foreach f,$(CFILES),$(BUILD)/$(subst .cpp,.o,$f))

define built_object
$(BUILD)/$(subst .cpp,.o,$f): $(SRC)/$f $(SRC)/$(subst .cpp,.h,$f)
	$(CC) -c $(CFLAGS) $(DFLAGS) $(LDFLAGS) $(SRC)/$f -o $(BUILD)/$(subst .cpp,.o,$f)
endef
$(foreach f,$(CFILES),$(eval $(call built_object)))

$(BUILD)/main.o: $(SRC)/main.cpp
	g++ -c $(SRC)/main.cpp -g $(CFLAGS) $(allegro-config --libs) -o $(BUILD)/main.o

$(BUILD)/mainpi.o: $(SRC)/main.cpp
	g++ -c $(SRC)/main.cpp -g $(CFLAGS) $(allegro-config --libs) -DRASPBERRY_PI -o $(BUILD)/mainpi.o

$(DOC)/documentation.pdf: $(DOC)/documentation.tex
	pdflatex $(DOC)/documentation.tex -output-directory $(DOC}/

program: $(BUILD)/main.o $(OFILES)
	g++ $(OFILES) $(BUILD)/main.o $(CFLAGS) $(allegro-config --libs) -lallegro $(ALLEGRO_FLAGS) -lboost_thread -lboost_system -lglut -lGL -o program

programpi: $(BUILD)/mainpi.o $(OFILES)
	g++ $(OFILES) $(BUILD)/mainpi.o -g $(CFLAGS) $(allegro-config --libs) -DRASPBERRY_PI -lwiringPi -lallegro $(ALLEGRO_FLAGS) -lboost_thread -lboost_system -lglut -lGL -o programpi libws2811.a

#program: main.cpp dat.o loghtml.o server.o query_performance_counter.o tournee_plan_creator.o performance_counter.o tournee_plan_brute_force_creator.o tournee_plan_approximation_creator.o
#	g++ main.cpp dat.o loghtml.o server.o query_performance_counter.o tournee_plan_creator.o performance_counter.o tournee_plan_brute_force_creator.o tournee_plan_approximation_creator.o -g $(CFLAGS) $(allegro-config --libs) -lallegro $(ALLEGRO_FLAGS) -lboost_thread -lboost_system -o program

#programpi: main.cpp dat.o loghtml.o server.o query_performance_counter.o tournee_plan_creator.o performance_counter.o tournee_plan_brute_force_creator.o tournee_plan_approximation_creator.o
#	g++ main.cpp dat.o loghtml.o server.o query_performance_counter.o tournee_plan_creator.o performance_counter.o tournee_plan_brute_force_creator.o tournee_plan_approximation_creator.o -g $(CFLAGS) $(allegro-config --libs) -DRASPBERRY_PI -lwiringPi -lallegro $(ALLEGRO_PIFLAGS) -lboost_thread -lboost_system -o programpi

clean:
	rm -f $(BUILD)/*.o
	rm -f program
	rm -f programpi
