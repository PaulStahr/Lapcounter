CFLAGS= -Wall -Wextra -pedantic -g -O2 -fopenmp -std=c++14
CFLAGS += -DNDEBUG

SRC := src
BUILT := built
DOC := doc

UNAME := $(shell uname -m)
ifeq ($(UNAME), armv7l)
ALLEGRO_FLAGS := $(shell pkg-config --cflags --libs allegro-5.0 allegro_main-5.0 allegro_primitives-5.0 allegro_ttf-5.0 allegro_font-5.0 allegro_audio-5.0 allegro_acodec-5.0 allegro_image-5.0)
CFLAGS += -DLEDSTRIPE
endif
ifeq ($(UNAME), x86_64)
ALLEGRO_FLAGS := $(shell pkg-config --cflags --libs allegro-5 allegro_main-5 allegro_primitives-5 allegro_ttf-5 allegro_font-5 allegro_audio-5 allegro_acodec-5 allegro_image-5)
endif
$(BUILT)/serialize.o: $(SRC)/serialize.cpp
	g++ -c $(SRC)/serialize.cpp -g $(CFLAGS) -o $(BUILT)/serialize.o

$(BUILT)/server.o: $(SRC)/server.cpp
	g++ -c $(SRC)/server.cpp -g $(CFLAGS) -o $(BUILT)/server.o

$(BUILT)/loghtml.o: $(SRC)/loghtml.cpp
	g++ -c $(SRC)/loghtml.cpp -g $(CFLAGS) -o $(BUILT)/loghtml.o

$(BUILT)/options.o: $(SRC)/options.cpp
	g++ -c $(SRC)/options.cpp -g $(CFLAGS) -o $(BUILT)/options.o

$(BUILT)/query_performance_counter.o: $(SRC)/query_performance_counter.cpp
	g++ -c $(SRC)/query_performance_counter.cpp -g $(CFLAGS) -o $(BUILT)/query_performance_counter.o

$(BUILT)/performance_counter.o: $(SRC)/performance_counter.cpp
	g++ -c $(SRC)/performance_counter.cpp -g $(CFLAGS) -o $(BUILT)/performance_counter.o

$(BUILT)/predictuon_score.o: $(SRC)/predictuon_score.cpp
	g++ -c $(SRC)/predictuon_score.cpp -g $(CFLAGS) -o $(BUILT)/predictuon_score.o

$(BUILT)/data.o: $(SRC)/data.cpp
	g++ -c $(SRC)/data.cpp -g $(CFLAGS) -o $(BUILT)/data.o

$(BUILT)/tournee_plan.o: $(SRC)/tournee_plan.cpp $(SRC)/tournee_plan.h
	g++ -c $(SRC)/tournee_plan.cpp -g $(CFLAGS) -o $(BUILT)/tournee_plan.o

$(BUILT)/tournee_plan_brute_force_creator.o: $(SRC)/tournee_plan_brute_force_creator.cpp
	g++ -c $(SRC)/tournee_plan_brute_force_creator.cpp -g $(CFLAGS) -o $(BUILT)/tournee_plan_brute_force_creator.o

$(BUILT)/tournee_plan_creator.o: $(SRC)/tournee_plan_creator.cpp
	g++ -c $(SRC)/tournee_plan_creator.cpp -g $(CFLAGS) -o $(BUILT)/tournee_plan_creator.o

$(BUILT)/tournee_plan_approximation_creator.o: $(SRC)/tournee_plan_approximation_creator.cpp
	g++ -c $(SRC)/tournee_plan_approximation_creator.cpp -g $(CFLAGS) -o $(BUILT)/tournee_plan_approximation_creator.o

$(BUILT)/firework.o: $(SRC)/firework.cpp $(SRC)/firework.h
	g++ -c $(SRC)/firework.cpp -g $(CFLAGS) -o $(BUILT)/firework.o

$(BUILT)/main.o: $(SRC)/main.cpp
	g++ -c $(SRC)/main.cpp -g $(CFLAGS) $(allegro-config --libs) -o $(BUILT)/main.o

$(BUILT)/mainpi.o: $(SRC)/main.cpp
	g++ -c $(SRC)/main.cpp -g $(CFLAGS) $(allegro-config --libs) -DRASPBERRY_PI -o $(BUILT)/mainpi.o

$(DOC)/documentation.pdf: $(DOC)/documentation.tex
	pdflatex $(DOC)/documentation.tex -output-directory $(DOC}/

program: $(BUILT)/main.o $(BUILT)/tournee_plan.o $(BUILT)/firework.o $(BUILT)/options.o $(BUILT)/serialize.o $(BUILT)/data.o $(BUILT)/loghtml.o $(BUILT)/server.o $(BUILT)/query_performance_counter.o $(BUILT)/tournee_plan_creator.o $(BUILT)/performance_counter.o $(BUILT)/tournee_plan_brute_force_creator.o $(BUILT)/tournee_plan_approximation_creator.o
	g++ $(BUILT)/tournee_plan.o $(BUILT)/firework.o $(BUILT)/main.o $(BUILT)/options.o $(BUILT)/serialize.o $(BUILT)/data.o $(BUILT)/loghtml.o $(BUILT)/server.o $(BUILT)/query_performance_counter.o $(BUILT)/tournee_plan_creator.o $(BUILT)/performance_counter.o $(BUILT)/tournee_plan_brute_force_creator.o $(BUILT)/tournee_plan_approximation_creator.o -g $(CFLAGS) $(allegro-config --libs) -lallegro $(ALLEGRO_FLAGS) -lboost_thread -lboost_system -lglut -lGL -o program

programpi: $(BUILT)/mainpi.o $(BUILT)/tournee_plan.o $(BUILT)/firework.o $(BUILT)/options.o $(BUILT)/serialize.o $(BUILT)/data.o $(BUILT)/loghtml.o $(BUILT)/server.o $(BUILT)/query_performance_counter.o $(BUILT)/tournee_plan_creator.o $(BUILT)/performance_counter.o $(BUILT)/tournee_plan_brute_force_creator.o $(BUILT)/tournee_plan_approximation_creator.o
	g++ $(BUILT)/tournee_plan.o $(BUILT)/firework.o $(BUILT)/mainpi.o $(BUILT)/options.o $(BUILT)/serialize.o $(BUILT)/data.o $(BUILT)/loghtml.o $(BUILT)/server.o $(BUILT)/query_performance_counter.o $(BUILT)/tournee_plan_creator.o $(BUILT)/performance_counter.o $(BUILT)/tournee_plan_brute_force_creator.o $(BUILT)/tournee_plan_approximation_creator.o -g $(CFLAGS) $(allegro-config --libs) -DRASPBERRY_PI -lwiringPi -lallegro $(ALLEGRO_FLAGS) -lboost_thread -lboost_system -lglut -lGL -o programpi libws2811.a

#program: main.cpp dat.o loghtml.o server.o query_performance_counter.o tournee_plan_creator.o performance_counter.o tournee_plan_brute_force_creator.o tournee_plan_approximation_creator.o
#	g++ main.cpp dat.o loghtml.o server.o query_performance_counter.o tournee_plan_creator.o performance_counter.o tournee_plan_brute_force_creator.o tournee_plan_approximation_creator.o -g $(CFLAGS) $(allegro-config --libs) -lallegro $(ALLEGRO_FLAGS) -lboost_thread -lboost_system -o program

#programpi: main.cpp dat.o loghtml.o server.o query_performance_counter.o tournee_plan_creator.o performance_counter.o tournee_plan_brute_force_creator.o tournee_plan_approximation_creator.o
#	g++ main.cpp dat.o loghtml.o server.o query_performance_counter.o tournee_plan_creator.o performance_counter.o tournee_plan_brute_force_creator.o tournee_plan_approximation_creator.o -g $(CFLAGS) $(allegro-config --libs) -DRASPBERRY_PI -lwiringPi -lallegro $(ALLEGRO_PIFLAGS) -lboost_thread -lboost_system -o programpi

clean:
	rm -f $(BUILT)/*.o
	rm -f program
	rm -f programpi
