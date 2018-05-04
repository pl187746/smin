#include <stdio.h>

static char* g_infile = NULL;
static char* g_outfile = NULL;

static FILE* g_input = NULL;
static FILE* g_output = NULL;

static int g_outappend = 0;
static int g_outendnl = 0;

static int g_inchar = EOF;

static int g_innewline = 1;
static int g_inspace = 1;
static int g_outspace = 1;

static inline int next_in()
{
	int ch = ((g_inchar != EOF) ? g_inchar : fgetc(g_input));
	g_inchar = EOF;
	return ch;
}

static inline int peek_in()
{
	return g_inchar = next_in();
}

static inline void next_out(int ch)
{
	fputc(ch, g_output);
	g_outspace = 0;
}

static inline void emit_space()
{
	if (g_inspace && !g_outspace)
		next_out(' ');
	g_outspace = 1;
}

static inline void punct_char(int ch)
{
	next_out(ch);
	g_innewline = 0;
	g_inspace = 1;
	g_outspace = 1;
}

/**** Lang-specific filters begin ****/

static inline void quoted_string(int ch, int endch)
{
	emit_space();
	next_out(ch);
	while((ch = next_in()) != EOF)
	{
		next_out(ch);
		if (ch == endch)
		{
			g_inspace = 0;
			g_innewline = 0;
			break;
		}
	}
}

static inline void singleline_comment()
{
	int ch;
	while ((ch = next_in()) != EOF)
	{
		switch (ch)
		{
		case '\n':
		case '\v':
		case '\f':
		case '\r':
			g_inspace = 1;
			g_innewline = 1;
			return;
		}
	}
}

static inline void multiline_comment()
{
	int ch;
	while ((ch = next_in()) != EOF)
	{
		if ((ch == '*') && (peek_in() == '/'))
		{
			next_in();
			g_inspace = 1;
			g_innewline = 0;
			break;
		}
	}
}

static inline void dot_command()
{
	int ch;
	next_out('\n');
	next_out('.');
	while ((ch = next_in()) != EOF)
	{
		switch (ch)
		{
		case '\n':
		case '\v':
		case '\f':
		case '\r':
			next_out('\n');
			g_outspace = 1;
			g_inspace = 1;
			g_innewline = 1;
			return;
		default:
			next_out(ch);
		}
	}
}

static inline void general_code()
{
	int ch;
	while ((ch = next_in()) != EOF)
	{
		switch (ch)
		{
		case '\n':
		case '\v':
		case '\f':
		case '\r':
			g_innewline = 1;
			/* fallthrough */
		case '\t':
		case ' ':
			g_inspace = 1;
			break;
		case '\"':
		case '\'':
		case '[':
		case '`':
			quoted_string(ch, ((ch == '[') ? ']' : ch));
			break;
		case '.':
			if (g_innewline)
			{
				dot_command();
				break;
			}
			/* fallthrough */
		case '!':
		case '%':
		case '&':
		case '(':
		case ')':
		case '*':
		case '+':
		case ',':
		case ';':
		case '<':
		case '=':
		case '>':
		case '|':
			punct_char(ch);
			break;
		case '-':
			if (peek_in() == '-')
				singleline_comment();
			else
				punct_char(ch);
			break;
		case '/':
			if (peek_in() == '*')
			{
				next_in();
				multiline_comment();
			}
			else
				punct_char(ch);
			break;
		default:
			emit_space();
			next_out(ch);
			g_innewline = 0;
			g_inspace = 0;
			break;
		}
	}
}

/**** Lang-specific filters end ****/

static inline int parse_args(int argc, char* argv[])
{
	int i;
	for (i = 1; i < argc; ++i)
	{
		if (argv[i][0] == '-')
		{
			switch (argv[i][1])
			{
			case 'i':
				if (g_infile)
				{
					fputs("Only one input file allowed.", stderr);
					return 1;
				}
				if (argv[i][2])
					g_infile = argv[i] + 2;
				else
					g_infile = argv[++i];
				if (!g_infile)
				{
					fputs("Input file name required.", stderr);
					return 1;
				}
				break;
			case 'a':
				g_outappend = 1;
				/* fallthrough */
			case 'o':
				if (g_outfile)
				{
					fputs("Only one output file allowed.", stderr);
					return 1;
				}
				if (argv[i][2])
					g_outfile = argv[i] + 2;
				else
					g_outfile = argv[++i];
				if (!g_outfile)
				{
					fputs("Output file name required.", stderr);
					return 1;
				}
				break;
			case 'n':
				g_outendnl = 1;
				break;
			case '-':
				if(!argv[i][2])
					return 0;
				/* fallthrough */
			default:
				fprintf(stderr, "Unrecognized option \'%s\'.\n", argv[i]);
				/* fallthrough */
			case 'h':
			case '?':
				printf("\n\tUsage: %s -i<inputfile> -o<outputfile>\n\n", argv[0]);
				puts("If -a is used instead of -o then output is appended to outputfile rather than overwriting it.");
				puts("If either inputfile or outputfile is omitted then stdin and/or stdout will be used respectively.");
				puts("\nAdditional options:");
				puts("\t-n\tAdds a new line at end.");
				puts("\t-h\tPrints this help text on stdout and exits.");
				return 1;
			}
		}
		else
		{
			if (!g_infile)
				g_infile = argv[i];
			else if (!g_outfile)
				g_outfile = argv[i];
			else
			{
				fputs("Too many arguments.", stderr);
				return 1;
			}
		}
	}
	return 0;
}

static inline int open_files()
{
	int rc = 0;
	if (!g_infile)
		g_input = stdin;
	else
	{
		g_input = fopen(g_infile, "r");
		if (!g_input)
		{
			fprintf(stderr, "Cannot open input file '%s' for reading.\n", g_infile);
			rc = 1;
		}
	}
	if (!g_outfile)
		g_output = stdout;
	else
	{
		g_output = fopen(g_outfile, (g_outappend ? "a" : "w"));
		if (!g_output)
		{
			fprintf(stderr, "Cannot open output file '%s' for writing.\n", g_outfile);
			rc = 1;
		}
	}
	return rc;
}

int main(int argc, char* argv[])
{
	int rc = 0;
	if (parse_args(argc, argv))
		return 0;
	if (open_files())
		rc = 1;
	else
	{
		general_code();
		if (g_outendnl)
			next_out('\n');
	}
	if(g_output)
		fclose(g_output);
	if(g_input)
		fclose(g_input);
	return rc;
}
