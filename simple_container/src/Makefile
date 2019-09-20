build : main.cc
	g++ -o ../build/main.out main.cc

run : main.cc
	make build
	../build/main.out

.PHONY : clean
clean :
	-rm ../build/main.out
	-rm -r ../simp_container_root