#use Fatal qw(open close rename chmod unlink);
open DESC, 'regcomp.sym';
$ind = 0;

while (<DESC>) {
  next if /^\s*($|\#)/;
  $ind++;
  chomp;
  ($name[$ind], $desc, $rest[$ind]) = split /\t+/, $_, 3;
  ($type[$ind], $code[$ind], $args[$ind], $longj[$ind]) 
    = split /[,\s]\s*/, $desc, 4;
}
close DESC;
$tot = $ind;

$tmp_h = 'tmp_reg.h';

unlink $tmp_h if -f $tmp_h;

open OUT, ">$tmp_h";

print OUT <<EOP;
/* !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
   This file is built by regcomp.pl from regcomp.sym.
   Any changes made here will be lost!
*/

EOP

$ind = 0;
while (++$ind <= $tot) {
  $oind = $ind - 1;
  $hind = sprintf "%#4x", $oind;
  print OUT <<EOP;
#define	$name[$ind]	$oind	/* $hind $rest[$ind] */
EOP
}

print OUT <<EOP;

#ifndef DOINIT
EXTCONST U8 PL_regkind[];
#else
EXTCONST U8 PL_regkind[] = {
EOP

$ind = 0;
while (++$ind <= $tot) {
  print OUT <<EOP;
	$type[$ind],		/* $name[$ind] */
EOP
}

print OUT <<EOP;
};
#endif


#ifdef REG_COMP_C
static const U8 regarglen[] = {
EOP

$ind = 0;
while (++$ind <= $tot) {
  $size = 0;
  $size = "EXTRA_SIZE(struct regnode_$args[$ind])" if $args[$ind];
  
  print OUT <<EOP;
	$size,		/* $name[$ind] */
EOP
}

print OUT <<EOP;
};

static const char reg_off_by_arg[] = {
EOP

$ind = 0;
while (++$ind <= $tot) {
  $size = $longj[$ind] || 0;

  print OUT <<EOP;
	$size,		/* $name[$ind] */
EOP
}

print OUT <<EOP;
};

#ifdef DEBUGGING
static const char * const reg_name[] = {
EOP

$ind = 0;
while (++$ind <= $tot) {
  $hind = sprintf "%#4x", $ind-1;
  $size = $longj[$ind] || 0;

  print OUT <<EOP;
	"$name[$ind]",		/* $hind */
EOP
}

print OUT <<EOP;
};

static const int reg_num = $tot;

#endif /* DEBUGGING */
#endif /* REG_COMP_C */

EOP

close OUT;

chmod 0666, 'regnodes.h';
unlink 'regnodes.h';
rename $tmp_h, 'regnodes.h';
