#readelf: --sections
#name: label arithmetic with multiple same-name sections

#...
[ 	]*\[.*\][ 	]+foo[ 	]+GROUP.*
#...
[ 	]*\[.*\][ 	]+\.text[ 	]+PROGBITS.*
#...
[ 	]*\[.*\][ 	]+\.data[ 	]+PROGBITS.*
#...
[ 	]*\[.*\][ 	]+\.text[ 	]+PROGBITS.*
#pass
