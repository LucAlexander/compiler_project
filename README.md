This is a highlevel compiler. Currently it:
* parses
* type checks
* lifts closures/lambdas
* validates constants/aliases/intrinsics
* monomorphizes parametric types 

All that needs doing is:
* check for valid main entry point
* drop all functions that never have a chance to be called
* code generation pass

Here is the test file:
```
using io;

constant array_size = 7;
constant pi = 3.14;

type Food {
	Apple{
		u8 seeds;
		{u8 r; u8 g; u8 b;} color;
	};
	Nothing;
	Orange{
		u8 slices;
		f32 skin_toughness;
	};
	Fish{
		{Salmon; Herring; Trout;} Species;
		u8 good_in_soup;
	};
};

alias fish_species {Salmon; Herring; Trout;};

Food change_enumerator = (
	Food a = {Apple, 2, {255, 0, 0}};
	Food b = {Fish, {Salmon,}, 1};
	u8 seed_count = {a seeds};
	{Salmon; Herring; Trout;} kind = {b Species};
	fish_species spec = {b Species};
	return a;
);

type fish{
	u32 x;
	u32 y;
	koi {
		{u8 r; u8 g; u8 b;} color;
	};
	cat {
		u8 whisker_length;
	} = 2;
	state_enum state;
};

type state_enum {
	swimming = 0;
	swooming;
	stunned{};
	dead{} = 1;
};

type ebdb {
	frog body;
	u8 tail_length;
};

type String [i8 16];
alias aString [i8 var 16];

type frog {
	u8 number_eyes;
	{u8 r;u8 g; u8 b;} color;
};

alias toad frog;

type vec3 {
	u8 x;
	u8 y;
	u8 z;
};

alias colr {
	u8 r;
	u8 g;
	u8 b;
};

type byte u8;

type player {
	i64 x;
	i64 y;
	u8 hp;
	u8 symbol;
	frog pet;
	byte flags;
};

u64 -> [u8] mem = \x [1,];

u8 -> player inst = \x {10, 10, 10, 2, {2,}, 1};

i64 -> u64 as_u64 = \x (3);
i64 -> u8 as_u8 = \x (return 4;);
u64 -> i64 as_i64 = \x ( i64 a = 5; return 5;);
u8 main = (
	u8 w = 256;
	u8 h = 256;
	u64 -> u8 -> u64 coord = \a b ( a + (b * w));
	player -> u64 plcoord = \pl ( (as_u64 {pl x}) + (w * {pl y}));
	[u8 var] buffer = mem $ w * h;
	u64 x = 6;
	u8 y = 7;
	u64 z = 6 * 2;
	player bob = {as_i64 x, as_i64 $ 1 * y, 10, 1, {2,}, 0};
	[buffer $ coord x y] = 0;
	[buffer $ plcoord bob] = {bob symbol};
	[buffer (plcoord bob)] = {bob pet number_eyes};
	{bob pet};
	frog jeff = {bob pet};
	u8 var eyes = 1;
	eyes = {jeff number_eyes};
	byte eight = {bob flags};
	i16 a = w + h;
	ptr bob;
	ptr ptr bob;
	[player] ref = ptr bob;
	[[player]] dptr = ptr ptr bob;
	[ref symbol];
	[[dptr] symbol];
	return w;
);

u32 constants = (
	i32 var a = 1;
	[i32 var ] b = ptr a;
	[i32 ] var d = ptr a;
	[i16 var 2] c = [5, 6];
	[i16 2] var e = [7, 8];
	i16 y = 8;
	i16 var h = y+1;
	[i16 var 2] var ef = [y, h];
	(([[i64 var ] var 6] var) -> i32 var) var aaf = \x (6);
	aaf = \x (7);
	return 8;
);

u8 partial = (
	u8 -> u8 -> u8 sum = \x y (x+y);
	u8 -> u8 add5 = sum 5;
	return add5 6;
);

[i8] string = "This is an arbitrary string\n";

u8 single_block = (
	return 7;
);

u8 single_inline = (return 7);
u8 single_application = return 7;

{u32 x;u32 y;} anon_structs = (
	u32 x = 0;
	u32 y = 0;
	return {x,y};
);

f32 -> frog
nested_anonymous = \x (
	frog a = {2, {255, 255, 0}};
	return {21, [(ptr a) color]};
);

f64
floats = (
	f32 a = 4.6;
	f64 b = 6.0;
	i32 c = 7;
	f32 d = 7;
	return b;
);

f32 float_bind = (
	i32 a = 7;
	return a;
);

toad float_prim = (
	i32 a = 7;
	frog f = {3, {2, 2, 2}};
	toad t = {2, {1, 1, 1}};
	return f;
);

u8 show_aliases = (
	[i8 var 16] ss = "abcdefghijkl";
	aString sss = ss;
	[ss 2] = 1;

	String str = [1, 2, 3, 4];
	String s = "abcdefghijkl";

	[sss 2] = 1;
	
	colr c = {1, 1, 1};
	frog f = {2, c};
	toad t = {3, c};
	frog ff = t;
	toad tt = f;
	return {t color r};
);

u8 lambda_lifting = (
	u8 b = 1;
	u8 c = 2;
	u8 -> u8 f = \x (x);
	u8 a = \x y z ((y*z)+(f b)+x) 7 8 9;
	return a;
);

u8 closure_lifting = (
	;;
	u8 -> u8 f = \b (
		u8 c = 2;
		u8 a = \x y z ((y*z)+b+c+c+x) 7 8 9;
		return a;
	);
	u8 a = 6;
	u8 -> u8 capturing = \x (x+a+(f 4));
	u8 b = capturing a;
	;
	return f 4;
);

u8 bu = (
	;
	u8 -> u8 a = \x (5+(a x));
	;;;;
	return 5;
);

u8 procwrapper = (
	[i8] -> procedure u8 print = \x (u8 y = 7;);
	u8 -> [i8] -> procedure u8 assert = \condition message (
		if condition (
			print message;
			return 1;
		);
	);
	u8 x = 0;
	assert (x==0) "x must be 0";
	return 0;
);

u8 simple_conditionals = (
	u32 var a = 0;
	u32 var b = 1;
	u32 c = if (a>b) 1 2;
	if c (
		a = 0;
		b = 1;
	);
	u8 var d = 0;
	u8 e = if a (
		return 1;
	) if b (
		return 0;
	) (
		d = 1;
		return 7;
	);
	if a (
		return 1;
	) if b (
		return 0;
	) (
		d = 1;
	);
	return d;
);

procedure u8 more_procedures = (
	(procedure u8) pr = (return 1;);
	pr;
);

procedure u8 char_lits = (
	i8 a = 'a';
	[i8] stirng = "asdasdasdasd";
);

procedure f32 test_consts = (
	u8 x = 0;
	procedure f32 ff = (
		[u32 array_size] current = [7,];
		return pi;
	);
	ff;
);

procedure f32 casting = (
	u8 x = 6;
	[u8] xp = ptr x;
	[f32] yp = xp as [f32];
	[f64] zp = yp as [f64];
);

constant BUFFER_SIZE = 1024;

procedure u64 sizes = (
	u64 a = sizeof u8;
	u32 b = sizeof (5+ 7);
	[u8] buffer = (alloc $ (sizeof Food) * BUFFER_SIZE);
	[fish] school = (alloc $ (sizeof fish) * BUFFER_SIZE) as [fish];
	free buffer;
	free school;
);

u8 procedure_access = (
	u32 var i = 0;
	u32 -> u32 inc = (+1);
	procedure u8 x = (
		i = i + 1;
		i = inc i;
	);
	x;
	return 1;
);

u32 loops = (
	u32 var sum = 0;
	for 0 100 (+1) \i (
		sum = sum + i;
	);
	first: for sum (sum+100) (\x (x+1)) \i(
		sum = sum - i;
	);
	second: if (sum > 200)(
		return 0;
	);
	return sum;
);

u32 while = (
	u32 var i = 0;
	if (1)(
		if (i > 64)(
			break;
		);
		i = i+1;
		continue;
	);
	i = 0;
	while: if (1)( if (i > 64) (break :while;);
		i = i+1;
	continue :while;);
	while: if (1)(
		if (i > 64) (
			break :while;
		);
		u32 -> u32 xx = \y (
			//break :while;
			return y + 1;
		);
		i = i+1;
		continue :while;
	);
	return i;
);

u8
better_literal_parsing = (
	u32 positive = 7;
	i32 negative = -8;
	f32 fpositive = 9;
	f32 fnegative = -8.9;
	u32 octal = 0o076;
	u32 hex = 0xFF;
	u32 bin = 0b101101;
	u32 noctal = -0o076;
	u32 nhex = -0xFF;
	u32 nbin = -0b101101;
	f64 sci_not = 1.05E11;
	f64 sfc_ntn = 5e-5;
	return 0;
);

[i8]
string_escape_literals = (
	return "\t\n\"\\\"";
);

i8
character_escape_literals = (
	i8 x = '\n';
	i8 y = '\'';
	i8 z = '\\';
	return '\'';
);

f64
coersions = (
	u8 x = 6;
	return x;
);

procedure u64
size_checks = (
	u64 var x = 0;
	x = sizeof u8;
	x = sizeof u16;
	x = sizeof u32;
	x = sizeof u64;
	x = sizeof i8;
	x = sizeof i16;
	x = sizeof i32;
	x = sizeof i64;
	x = sizeof f32;
	x = sizeof f64;
	x = sizeof {u8 a; u8 b; u8 c;};
	x = sizeof {u8 a; u32 b; u8 c;};
	x = sizeof {u8 a; u64 b; u8 c;};
	x = sizeof {u32 a; u64 b; u8 c;};
	x = sizeof {u64 a; u64 b; u8 c;};
	x = sizeof {u8 a; u8 d; u64 b; u8 c;};
	x = sizeof [{f32 a; i8 b; u8 c; i32 d; i16 e;} 3];
);

type oNode (T) => {u8 x;};
type oMode T => {u8 x;};

(T) => u8 nap_cons = 7;

oNode (oMode [oNode (oMode [Food]) array_size]) -> u8 param_test = \x (1);

u8 nested_tested = (
	u8 -> u8 bb = \x (x);
	u8 c = (bb) 6;
	return c;
);

T => T -> u8
morph_test = \x (
	T y = x;
	return 1;
);

T => T -> T
more_involved = \x (x);

u8 calling_test = (
	u8 res = morph_test "hello this is a test";
	u8 var ans = morph_test 5.6;
	i8 more = more_involved 'F';
	ans = morph_test "reuse";
	return res + ans;
);

type Node T => {
	T var data;
	Cons {[Node] next;} = 1;
	Nothing = 0;
};


T => (T->T) -> [(Node T)] -> [(Node T)]
map_cons = \f x (
	[x data] = f [x data];
	return x;
);

u8 -> u8 morph_aux = \x (x);

u8 type_morph_test = (
	(Node u8) x = {6,Nothing};
	[(Node u8)] y = map_cons morph_aux $ ptr x;	
	return {x data};
);

type Maybe T => {
	Just {
		T value;
	};
	Nothing;
};

u8 -> u8 -> Maybe u8
divide_byte = \numerand divisor (
	return $ if (divisor == 0)
		{Nothing,}
		{Just, numerand / divisor};
);

u8 test_maybe_monad = (
	u8 x = 8;
	u8 y = 0;
	Maybe u8 z = divide_byte x y;
	return {z value};
);

u64 -> u64 -> u64 -> u64
fib = \a b count (
	if (count > 0) 
		(fib b (a+b) (count - 1)) 
		b
);

u8 nested_parametric = (
	T => u8 -> T -> u8 npf = \x y(x);
	return $ npf 1 2;
);


T => (Maybe T) only_type_mono = (
	return $ Nothing;
);

u8 test_only_type_mono = (
	(Maybe u8) x = only_type_mono;
	return 0;
);



```
