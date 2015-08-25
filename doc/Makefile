NAME=doc
SOURCE=${NAME}.tex
JUNK=${NAME}.log ${NAME}.aux ${NAME}.out missfont.log

all:
	pdflatex -shell-escape ${SOURCE} 
clean:
	rm -f ${JUNK}
