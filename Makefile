HDRS:=$(wildcard src/*.hh)
CTRS:=$(wildcard src/*.cc)
LDLIBS:=-lz -lpthread
LDFLAGS:=
CFLAGS_RELEASE:=-O3 -s -flto
CFLAGS_DEBUG:=-g -Og -Wall -Wextra \
              -Wno-sign-compare -Wno-unused-parameter -Wno-unused-function
CFLAGS:=-std=c++11 -pipe -march=native -pthread $(CFLAGS_DEBUG)

main: hl-csa-raptor.o
	@echo "Try it with 'make test'"

simple_raptor: simple_raptor.o
	:

test: _data/London hl-csa-raptor.o
	./hl-csa-raptor.o -nq=5 $<

%.o: src/%.cc $(HDRS)
	g++ $(CFLAGS) $(LDFLAGS) $(LDLIBS) -o $@ $<


REPO:=https://files.inria.fr/gang/graphs/public_transport

_data/%:
	mkdir -p _data/$*
	cd _data/$*; \
	for f in queries-unif.csv stop_times.csv.gz transfers.csv.gz walk_and_transfer_inhubs.gr.gz walk_and_transfer_outhubs.gr.gz; do \
		curl -o $$f $(REPO)/$*/$$f ;\
	done

clean:
	rm -fr *~ */*~ _* *.o
