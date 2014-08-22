.SUFFIXES:

%.o: %.cpp
	@( .ib/ib compile; )

test: OctetSequence.o
	.ib/ib link 
