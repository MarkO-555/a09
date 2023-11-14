
/* GPL3+ */

#include <cgilib6/nodelist.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "a09.h"

/**************************************************************************/

char const MSG_DEBUG[]   = "debug";
char const MSG_WARNING[] = "warning";
char const MSG_ERROR[]   = "error";

/**************************************************************************/

bool message(struct a09 *a09,char const *restrict tag,char const *restrict fmt,...)
{
  va_list ap;
  
  if ((tag == MSG_DEBUG) && !a09->debug)
    return true;
    
  fprintf(stderr,"%s:%zu: %s: ",a09->infile,a09->lnum,tag);
  va_start(ap,fmt);
#if defined(__clang__)
#  pragma clang diagnostic push "-Wformat-nonliteral"
#  pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
  vfprintf(stderr,fmt,ap);
#if defined(__clang__)
#  pragma clang diagnostic pop "-Wformat-nonliteral"
#endif
  va_end(ap);
  fprintf(stderr,"\n");
  return tag != MSG_ERROR;
}

/**************************************************************************/

static bool read_line(FILE *in,struct buffer *buffer)
{
  assert(in     != NULL);
  assert(buffer != NULL);
  
  buffer->widx = 0;
  buffer->ridx = 0;
  
  while(!feof(in))
  {
    int c = fgetc(in);
    if (c == EOF)  return false;
    if (c == '\n') break;
    if (c == '\t')
    {
      for (size_t num = 8 - (buffer->widx & 7) , j = 0 ; j < num ; j++)
      {
        if (buffer->widx == sizeof(buffer->buf)-1)
          return false;
        buffer->buf[buffer->widx++] = ' ';
      }
    }
    else if (isprint(c))
    {
      if (buffer->widx == sizeof(buffer->buf)-1)
        return false;
      buffer->buf[buffer->widx++] = c;
    }
    else
      return false;
  }
  
  assert(buffer->widx < sizeof(buffer->buf));
  buffer->buf[buffer->widx] = '\0';
  return true;
}

/**************************************************************************/

static void read_label(struct buffer *buffer,label *label,char c)
{
  assert(buffer != NULL);
  assert(label  != NULL);
  assert(isgraph(c));
  
  size_t i = 0;
  
  while((c == '.') || (c == '_') || (c == '$') || isalnum(c))
  {
    if (i < sizeof(label->text))
      label->text[i++] = c;
    c = buffer->buf[buffer->ridx++];
  }
  
  label->s = i;
  assert(buffer->ridx > 0);
  buffer->ridx--;
}

/**************************************************************************/

bool parse_label(label *res,struct buffer *buffer,struct a09 *a09)
{
  assert(res    != NULL);
  assert(buffer != NULL);
  assert(a09    != NULL);
  
  label tmp;
  char  c = buffer->buf[buffer->ridx];
  
  if ((c == '.') || (c == '_') || isalpha(c))
  {
    buffer->ridx++;
    read_label(buffer,&tmp,c);
    if (tmp.text[0] == '.')
    {
      memcpy(res->text,a09->label.text,a09->label.s);
      size_t s = min(tmp.s,sizeof(tmp.text) - tmp.s);
      assert(s <= sizeof(res->text));
      memcpy(&res->text[a09->label.s],tmp.text,s);
      if (a09->label.s + tmp.s > sizeof(res->text))
        message(a09,MSG_WARNING,"W0001: label '%.*s%.*s' exceeds %zu characters",a09->label.s,a09->label.text,tmp.s,tmp.text,sizeof(res->text));
      res->s = a09->label.s + s;
    }
    else
      *res = tmp;
      
    return true;
  }
  else
    return false;
}

/**************************************************************************/

static bool parse_op(struct buffer *buffer,struct opcode const **pop)
{
  assert(buffer != NULL);
  assert(pop    != NULL);
  
  char top[10];
  char c = 0;
  
  for (size_t i = 0 ; i < 10 ; i++)
  {
    c = buffer->buf[buffer->ridx];
    if (isspace(c) || (c == '\0'))
    {
      top[i] = '\0';
      *pop   = op_find(top);
      return *pop != NULL;
    }
    else if (!isalpha(c) && !isdigit(c) && (c != '.'))
      break;
      
    top[i] = toupper(c);
    buffer->ridx++;
  }
  
  assert(c != 0);
  return false;
}

/**************************************************************************/

char skip_space(struct buffer *buffer)
{
  char c;
  
  do
  {
    c = buffer->buf[buffer->ridx];
    if (c == '\0')
      return c;
    buffer->ridx++;
  } while (isspace(c));
  
  return c;
}

/**************************************************************************/

bool print_list(struct a09 *a09,struct opcdata *opd,bool labelonly)
{
  assert(a09 != NULL);
  assert(opd != NULL);
  
  if ((a09->list != NULL) && (!opd->includehack))
  {
    if ((opd->sz == 0) && (opd->datasz == 0))
    {
      if (labelonly && opd->label.s > 0)
        fprintf(a09->list,"%04X:             ",a09->pc);
      else
        fprintf(a09->list,"                  ");
    }
    else
    {
      if (opd->data)
      {
        size_t c = 0; // # byte columns printed
        fprintf(a09->list,"%04X: ",a09->pc);
        for (size_t i = 0 ; i < opd->sz ; i++,c++)
          fprintf(a09->list,"%02X",opd->bytes[i]);
        for ( ; c < sizeof(opd->bytes) ; c++)
          fprintf(a09->list,"  ");
      }
      else
      {
        size_t i = 0; // index into opd->bytes[]
        size_t c = 0; // # byte columns printed
        
        fprintf(a09->list,"%04X: %02X",a09->pc,opd->bytes[i++]);
        c++;
        
        if ((opd->bytes[0] == 0x10) || (opd->bytes[0] == 0x11))
        {
          fprintf(a09->list,"%02X  ",opd->bytes[i++]);
          c++;
        }
        else
        {
          fprintf(a09->list,"    ");
          c++;
        }
        
        for ( ; i < opd->sz  ; i++)
        {
          fprintf(a09->list,"%02X",opd->bytes[i]);
          c++;
        }
        
        for ( ; c < 5 ; c++)
          fprintf(a09->list,"  ");
      }
    }
    
    fprintf(a09->list," %5zu | %s\n",a09->lnum,a09->inbuf.buf);
  }
  opd->includehack = false;
  return true;
}

/**************************************************************************/

static bool parse_line(struct a09 *a09,struct buffer *buffer,int pass)
{
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert((pass == 1) || (pass == 2));
  
  int            c;
  bool           rc;
  struct opcdata opd =
  {
    .a09    = a09,
    .op     = NULL,
    .buffer = buffer,
    .label  = { .s = 0 },
    .pass   = pass,
    .sz     = 0,
    .data   = false,
    .datasz = 0,
    .mode   = AM_INHERENT,
    .value  =
    {
      .value        = 0,
      .bits         = 0,
      .unknownpass1 = false,
      .defined      = false,
      .external     = false,
    },
    .bits        = 16,
    .pcrel       = false,
    .includehack = false,
  };
  
  if (parse_label(&opd.label,&a09->inbuf,a09))
  {
    if (pass == 1)
    {
      if (symbol_add(a09,&opd.label,a09->pc) == NULL)
        return false;
    }
    else if (pass == 2)
    {
      /*--------------------------------------------------------------
      ; On pass 2, all ADDRESS labels should be the same.  If they're not,
      ; there's an internal error somewhere, so abort the assembly to avoid
      ; troubleshooting an assembler error in the program we're writing.
      ;---------------------------------------------------------------*/
      struct symbol *sym = symbol_find(a09,&opd.label);
      if (sym == NULL)
        return message(a09,MSG_ERROR,"E0001: Internal error---'%.*s' should exist, but doesn't",a09->label.s,a09->label.text);
      if ((sym->type == SYM_ADDRESS) && (sym->value != a09->pc))
        return message(a09,MSG_ERROR,"E0002: Internal error---out of phase;\n\t'%.*s' = %04X pass 1, %04X pass 2",a09->label.s,a09->label.text,sym->value,a09->pc);
    }
    
    /*-------------------------------------------------------------------
    ; Check to see if we have a proper label in case we have a local label
    ;--------------------------------------------------------------------*/
    
    if ((pass == 1) && (opd.label.text[0] == '.') && (a09->label.s == 0))
      message(a09,MSG_WARNING,"W0012: missing initial label");
    
    /*-----------------------------------
    ; store the current global label
    ;------------------------------------*/
    
    a09->label = opd.label;
    char *p    = memchr(a09->label.text,'.',a09->label.s);
    if (p != NULL)
      a09->label.s = (unsigned char)(p - a09->label.text);
  }
  
  c = skip_space(&a09->inbuf);
  
  if ((c == ';') || (c == '\0'))
    return print_list(a09,&opd,true);
    
  a09->inbuf.ridx--; // ungetc()
  
  if (!parse_op(&a09->inbuf,&opd.op))
    return message(a09,MSG_ERROR,"E0003: unknown opcode");
    
  rc = opd.op->func(&opd);
  
  if (rc)
  {
    print_list(a09,&opd,false);
    
    if (opd.data)
      a09->pc += opd.datasz;
    else
      a09->pc += opd.sz;
      
    if ((pass == 2) && (opd.sz > 0) && opd.datasz == 0)
    {
      size_t x = fwrite(opd.bytes,1,opd.sz,a09->out);
      if (x != opd.sz)
        rc = false;
    }
  }
  
  return rc;
}

/**************************************************************************/

bool assemble_pass(struct a09 *a09,int pass)
{
  assert(a09     != NULL);
  assert(a09->in != NULL);
  assert(pass    >= 1);
  assert(pass    <= 2);
  
  rewind(a09->in);
  a09->lnum          = 0;
  a09->label.text[0] = '\0';
  a09->label.s       = 0;
  
  message(a09,MSG_DEBUG,"Pass %d",pass);
  
  while(!feof(a09->in))
  {
    if (!read_line(a09->in,&a09->inbuf))
      return true;
    a09->lnum++;
    if (!parse_line(a09,&a09->inbuf,pass))
      return false;
  }
  
  return true;
}

/**************************************************************************/

static int parse_command(int argc,char *argv[],struct a09 *a09)
{
  int i;
  
  assert(argc >= 1);
  assert(argv != NULL);
  assert(a09  != NULL);
  
  for (i = 1 ; i < argc ; i++)
  {
    if (argv[i][0] == '-')
    {
      char const *format;
      
      switch(argv[i][1])
      {
        case 'o':
             if (argv[i][2] == '\0')
               a09->outfile = argv[++i];
             else
               a09->outfile = &argv[i][2];
             break;
             
        case 'l':
             if (argv[i][2] == '\0')
               a09->listfile = argv[++i];
             else
               a09->listfile = &argv[i][2];
             break;
             
        case 'd':
             a09->debug = true;
             break;
             
        case 'f':
             if (argv[i][2] == '\0')
               format = argv[++i];
             else
               format = &argv[i][2];
             
             if (strcmp(format,"bin") == 0)
             {
               if (!format_bin_init(&a09->format.bin,a09))
                 exit(1);
             }
             else if (strcmp(format,"rsdos") == 0)
             {
               if (!format_rsdos_init(&a09->format.rsdos,a09))
                 exit(1);
             }
             else
             {
               fprintf(stderr,"%s: E0053: format '%s' not supported\n",MSG_ERROR,format);
               exit(1);
             }
             break;
             
        case 'h':
        default:
             fprintf(
                      stderr,
                      "usage: [options] [files...]\n"
                      "\t-o filename\toutput filename\n"
                      "\t-l listfile\tlist filename\n"
                      "\t-f format\toutput format (bin)\n"
                      "\t-d\t\tdebug output\n"
                      "\t-h\t\thelp (this text)\n"
                      "\n"
                      "\tformats: bin rsdos\n"
                    );
             exit(1);
      }
    }
    else
      break;
  }
  
  return i;
}

/**************************************************************************/

static void warning_unused_symbols(struct a09 *a09,tree__s *tree)
{
  assert(a09 != NULL);
  
  if (tree != NULL)
  {
    warning_unused_symbols(a09,tree->left);
    struct symbol *sym = tree2sym(tree);
    if ((sym->refs == 0) && (sym->type == SYM_ADDRESS))
    {
      a09->lnum = sym->ldef;
      message(a09,MSG_WARNING,"W0002: symbol '%.*s' defined but not used",sym->name.s,sym->name.text);
    }
    warning_unused_symbols(a09,tree->right);
  }
}

/**************************************************************************/

static void dump_symbols(FILE *out,tree__s *tree)
{
  assert(out != NULL);
  if (tree != NULL)
  {
    static char const *const symtypes[] =
    {
      [SYM_UNDEF]   = "undefined",
      [SYM_ADDRESS] = "address",
      [SYM_EQU]     = "equate",
      [SYM_SET]     = "set",
      [SYM_PUBLIC]  = "public",
      [SYM_EXTERN]  = "extern",
    };
    
    dump_symbols(out,tree->left);
    struct symbol *sym = tree2sym(tree);
    if (sym->type != SYM_SET)
    {
      fprintf(
             out,
             "%5zu | %-11.11s %04X %5d %.*s\n",
             sym->ldef,
             symtypes[sym->type],
             sym->value,
             sym->refs,
             sym->name.s,sym->name.text
          );
    }
    dump_symbols(out,tree->right);
  }
}

/**************************************************************************/

int main(int argc,char *argv[])
{
  int         fi;
  bool        rc;
  struct a09  a09 =
  {
    .infile    = NULL,
    .outfile   = "a09.obj",
    .listfile  = NULL,
    .in        = NULL,
    .out       = NULL,
    .list      = NULL,
    .lnum      = 0,
    .symtab    = NULL,
    .label     =
    {
      .s    = 0,
      .text = { '\0' },
    },
    .pc        = 0,
    .dp        = 0,
    .debug     = false,
    .inbuf     =
    {
      .buf   = {0},
      .widx  = 0,
      .ridx  = 0,
    },
  };
  
  ListInit(&a09.symbols);
  format_bin_init(&a09.format.bin,&a09);
  fi = parse_command(argc,argv,&a09);
  
  if (fi == argc)
  {
    fprintf(stderr,"no input file specified\n");
    exit(1);
  }
  
  a09.infile = argv[fi];
  a09.in = fopen(a09.infile,"r");
  if (a09.in == NULL)
  {
    perror(a09.infile);
    exit(1);
  }
  
  if (!assemble_pass(&a09,1))
    exit(1);
    
  a09.out  = fopen(a09.outfile,"wb");
  
  if (a09.out == NULL)
  {
    perror(a09.outfile);
    exit(1);
  }
  
  if (a09.listfile != NULL)
  {
    a09.list = fopen(a09.listfile,"w");
    if (a09.list == NULL)
    {
      perror(a09.listfile);
      exit(1);
    }
    
    fprintf(a09.list,"                         | FILE %s\n",a09.infile);
  }
  
  a09.pc = 0;
  rc     = assemble_pass(&a09,2);
  
  if (rc)
    warning_unused_symbols(&a09,a09.symtab);
  
  if (a09.list != NULL)
  {
    fprintf(a09.list,"\n");
    dump_symbols(a09.list,a09.symtab);
    fclose(a09.list);
  }
  
  fclose(a09.out);
  fclose(a09.in);
  
  if (!rc)
  {
    if (a09.listfile) remove(a09.listfile);
    remove(a09.outfile);
  }
  
  for(
       Node *node = ListRemTail(&a09.symbols);
       NodeValid(node);
       node = ListRemTail(&a09.symbols)
     )
  {
    struct symbol *sym = node2sym(node);
    free(sym);
  }
  
  return rc ? 0 : 1;
}
