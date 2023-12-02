/****************************************************************************
*
*   The Unit Test backend
*   Copyright (C) 2023 Sean Conner
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Comments, questions and criticisms can be sent to: sean@conman.org
*
****************************************************************************/

// XXX - the way the code is now, we can't call forward in the tests
//	because the code isn't laid down yet.

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>

#include "a09.h"

enum vmops
{
  VM_LOR,
  VM_LAND,
  VM_GT,
  VM_GE,
  VM_EQ,
  VM_LE,
  VM_LT,
  VM_NE,
  VM_BOR,
  VM_BEOR,
  VM_BAND,
  VM_SHR,
  VM_SHL,
  VM_SUB,
  VM_ADD,
  VM_MUL,
  VM_DIV,
  VM_MOD,
  VM_NEG,
  VM_NOT,
  VM_LIT,
  VM_AT8,
  VM_AT16,
  VM_CPUCCc,
  VM_CPUCCv,
  VM_CPUCCz,
  VM_CPUCCn,
  VM_CPUCCi,
  VM_CPUCCh,
  VM_CPUCCf,
  VM_CPUCCe,
  VM_CPUA,
  VM_CPUB,
  VM_CPDP,
  VM_CPUD,
  VM_CPUX,
  VM_CPUY,
  VM_CPUU,
  VM_CPUS,
  VM_CPUPC,
  VM_EXIT,
};

struct memprot
{
  bool read  : 1;
  bool write : 1;
  bool exec  : 1;
  bool tron  : 1;
  bool check : 1;
};

struct vmcode
{
  mc6809addr__t  here;
  char const    *tag;
  enum vmops    *prog;
};

struct testdata
{
  struct a09     *a09;
  const char     *corefile;
  mc6809__t       cpu;
  mc6809dis__t    dis;
  struct buffer   name;
  uint16_t        addr;
  uint16_t        pc;
  uint16_t        sp;
  mc6809byte__t   fill;
  bool            tron;
  size_t          numtrig;
  struct vmcode  *triggers;
  mc6809byte__t   memory[65536u];
  struct memprot  prot  [65536u];
};

/**************************************************************************/

char const format_test_usage[] =
        "\n"
        "Test format options:\n"
        "\t-S addr\t\taddress of system stack (default=0x8000)\n"
        "\t-F byte\t\tfill memory with value (default=0x3F, SWI inst)\n"
        "\t-R range\tmark memory read-only (see below)\n"
        "\t-W range\tmark memory write-only (see below)\n"
        "\t-E range\tmark memory as code (see below)\n"
        "\t-T range\ttrace execution of code (see below)\n"
        "\t-D file\t\tcore file (of 6809 VM) name\n"
        "\n"
        "NOTE:\tmemory ranges can be specified as:\n"
        "\t\t<low-address>[ '-' <high-address>]\n"
        "\tmultiple ranges can be specified, separated by a comma\n"
        "\tBy default, all memory is readable, writable and executable,\n"
        "\tbut not traced.  You can also enable tracing with the\n"
        "\t.TRON directive.\n"
        ;

/**************************************************************************/

static bool runvm(struct a09 *a09,mc6809__t *cpu,enum vmops *prog)
{
  assert(a09  != NULL);
  assert(cpu  != NULL);
  assert(prog != NULL);
  
  uint16_t stack[15];
  uint16_t result;
  size_t   sp = sizeof(stack) / sizeof(stack[0]);
  size_t   ip = 0;
  
  /*--------------------------------------------------------------
  ; I control the code generation, so I can skip checks that would
  ; otherwise have to be made.
  ;---------------------------------------------------------------*/
  
  while(true)
  {
    switch(prog[ip++])
    {
      case VM_LOR:
           result      = stack[sp + 1] || stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_LAND:
           result      = stack[sp + 1] && stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_GT:
           result      = stack[sp + 1] > stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_GE:
           result      = stack[sp + 1] >= stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_EQ:
           result      = stack[sp + 1] == stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_LE:
           result      = stack[sp + 1] <= stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_LT:
           result      = stack[sp + 1] < stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_NE:
           result      = stack[sp + 1] != stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_BOR:
           result      = stack[sp + 1] | stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_BEOR:
           result      = stack[sp + 1] ^ stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_BAND:
           result      = stack[sp + 1] & stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_SHR:
           result      = stack[sp + 1] >> stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_SHL:
           result      = stack[sp + 1] << stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_SUB:
           result      = stack[sp + 1] - stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_ADD:
           result      = stack[sp + 1] + stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_MUL:
           result      = stack[sp + 1] * stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_DIV:
           if (stack[sp] == 0)
             return message(a09,MSG_ERROR,"E0008: divide by 0 error");
           result      = stack[sp + 1] / stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_MOD:
           if (stack[sp] == 0)
             return message(a09,MSG_ERROR,"E0008: divide by 0 error");
           result      = stack[sp + 1] % stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_NEG:
           stack[sp] = -stack[sp];
           break;
           
      case VM_NOT:
           stack[sp] = ~stack[sp];
           break;
           
      case VM_LIT:
           stack[--sp] = prog[ip++];
           break;
           
      case VM_AT8:
           {
             struct format_test *test = cpu->user;
             struct testdata    *data = test->data;
             uint16_t            addr = stack[sp];
             stack[sp]                = data->memory[addr];
           }
           break;
           
      case VM_AT16:
           {
             struct format_test *test = cpu->user;
             struct testdata    *data = test->data;
             uint16_t            addr = stack[sp];
             stack[sp]                = (data->memory[addr] << 8)
                                      |  data->memory[addr+1]
                                      ;
           }
           break;
           
      case VM_CPUCCc:
           stack[--sp] = cpu->cc.c;
           break;
           
      case VM_CPUCCv:
           stack[--sp] = cpu->cc.v;
           break;
           
      case VM_CPUCCz:
           stack[--sp] = cpu->cc.z;
           break;
           
      case VM_CPUCCn:
           stack[--sp] = cpu->cc.n;
           break;
           
      case VM_CPUCCi:
           stack[--sp] = cpu->cc.i;
           break;
           
      case VM_CPUCCh:
           stack[--sp] = cpu->cc.h;
           break;
           
      case VM_CPUCCf:
           stack[--sp] = cpu->cc.f;
           break;
           
      case VM_CPUCCe:
           stack[--sp] = cpu->cc.e;
           break;
           
      case VM_CPUA:
           stack[--sp] = cpu->A;
           break;
           
      case VM_CPUB:
           stack[--sp] = cpu->B;
           break;
           
      case VM_CPDP:
           stack[--sp] = cpu->dp;
           break;
           
      case VM_CPUD:
           stack[--sp] = cpu->d.w;
           break;
           
      case VM_CPUX:
           stack[--sp] = cpu->X.w;
           break;
           
      case VM_CPUY:
           stack[--sp] = cpu->Y.w;
           break;
           
      case VM_CPUU:
           stack[--sp] = cpu->U.w;
           break;
           
      case VM_CPUS:
           stack[--sp] = cpu->S.w;
           break;
           
      case VM_CPUPC:
           stack[--sp] = cpu->pc.w;
           break;
           
      case VM_EXIT:
           assert(sp == (sizeof(stack) / sizeof(stack[0]) - 1));
           return stack[sp] != 0;
    }
  }
}

/**************************************************************************/

static mc6809byte__t ft_cpu_read(mc6809__t *cpu,mc6809addr__t addr,bool inst)
{
  assert(cpu       != NULL);
  assert(cpu->user != NULL);
  (void)inst;
  
  struct format_test *test = cpu->user;
  struct testdata    *data = test->data;
  
  if (cpu->instpc == addr)	/* start of an instruction */
  {
    if (!data->prot[addr].read)
      message(data->a09,MSG_WARNING,"W9999: reading from non-readable memory");
    if (!data->prot[addr].exec)
      message(data->a09,MSG_WARNING,"W9998: possible code execution in the weeds");
  }
  
  return data->memory[addr];
}

/**************************************************************************/

static void ft_cpu_write(mc6809__t *cpu,mc6809addr__t addr,mc6809byte__t byte)
{
  assert(cpu       != NULL);
  assert(cpu->user != NULL);
  
  struct format_test *test = cpu->user;
  struct testdata    *data = test->data;
  
  if (!data->prot[addr].write)
    message(data->a09,MSG_WARNING,"W9997: writing to non-writable memory");
  if (data->prot[addr].exec)
    message(data->a09,MSG_WARNING,"W9996: possible self-modifying code");
  if (data->prot[addr].tron)
    message(data->a09,MSG_WARNING,"W9995: memory write of %02X to %04X",byte,addr);
  data->memory[addr] = byte;
}

/**************************************************************************/

static void ft_cpu_fault(mc6809__t *cpu,mc6809fault__t fault)
{
  assert(cpu != NULL);
  longjmp(cpu->err,fault);
}

/**************************************************************************/

static mc6809byte__t ft_dis_read(mc6809dis__t *dis,mc6809addr__t addr)
{
  assert(dis       != NULL);
  assert(dis->user != NULL);
  
  struct format_test *test = dis->user;
  struct testdata    *data = test->data;
  
  return data->memory[addr];
}

/**************************************************************************/

static void ft_dis_fault(mc6809dis__t *dis,mc6809fault__t fault)
{
  assert(dis != NULL);
  longjmp(dis->err,fault);
}

/**************************************************************************/

static bool ftest_cmdline(union format *fmt,int *pi,char *argv[])
{
  assert(fmt  != NULL);
  assert(pi   != NULL);
  assert(*pi  >  0);
  assert(argv != NULL);
  
  struct format_test *test = &fmt->test;
  struct testdata    *data = test->data;
  int                 i    = *pi;
  unsigned long int   value;
  
  switch(argv[i][1])
  {
    case 'S':
         if (argv[i][2] == '\0')
           value = strtoul(argv[++i],NULL,0);
         else
           value = strtoul(&argv[i][2],NULL,0);
         
         if (value > 65535u)
         {
           fprintf(stderr,"%s: E9999: address exceeds address space\n",MSG_ERROR);
           exit(1);
         }
         
         data->sp = value;
         break;
         
    case 'D':
         if (argv[i][2] == '\0')
           data->corefile = argv[++i];
         else
           data->corefile = &argv[i][2];
         break;
         
    case 'F':
         if (argv[i][2] == '\0')
           value = strtoul(argv[++i],NULL,0);
         else
           value = strtoul(&argv[i][2],NULL,0);
         
         if (value > 255)
         {
           fprintf(stderr,"%s: E9999: byte should be 0..255\n",MSG_ERROR);
           exit(1);
         }
         
         data->fill = value;
         break;
         
    case 'R':
    case 'W':
    case 'E':
    case 'T':
         fprintf(stderr,"%s: E9999: '%c' not implemented\n",MSG_ERROR,argv[i][1]);
         break;
         
    default:
         return false;
  }
  
  *pi = i;
  return true;
}

/**************************************************************************/

static bool ftest_pass_start(union format *fmt,struct a09 *a09,int pass)
{
  assert(fmt != NULL);
  (void)a09;
  (void)pass;
  
  struct format_test *test = &fmt->test;
  test->intest = false;
  return true;
}

/**************************************************************************/

static bool ftest_inst_write(union format *fmt,struct opcdata *opd)
{
  assert(fmt       != NULL);
  assert(opd       != NULL);
  assert(opd->pass == 2);
  
  struct format_test *test = &fmt->test;
  struct testdata    *data = test->data;
  memcpy(&data->memory[data->addr],opd->bytes,opd->sz);
  for (size_t i = 0 ; i < opd->sz ; i++)
  {
    data->prot[data->addr].exec  = true;
    data->prot[data->addr].tron |= data->tron;
    data->addr++;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_data_write(
        union format   *fmt,
        struct opcdata *opd,
        char const     *buffer,
        size_t          len
)
{
  assert(fmt       != NULL);
  assert(opd       != NULL);
  assert(buffer    != NULL);
  assert(opd->pass == 2);
  
  struct format_test *test = &fmt->test;
  struct testdata    *data = test->data;
  
  memcpy(&data->memory[data->addr],buffer,len);
  data->addr += len;
  return true;
}

/**************************************************************************/

static bool ftest_org(
        union format   *fmt,
        struct opcdata *opd,
        uint16_t        start,
        uint16_t        last
)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  (void)last;
  
  if (opd->pass == 2)
  {
    struct format_test *test = &fmt->test;
    struct testdata    *data = test->data;
    
    data->addr = start;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_test(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  struct format_test *test = &fmt->test;
  struct testdata    *data = test->data;
  
  if (test->intest)
    return message(opd->a09,MSG_ERROR,"E9999: cannot nest .TEST");
  test->intest = true;
  data->pc     = opd->a09->pc;
  parse_string(opd->a09,&data->name,opd->buffer);
  return true;
}

/**************************************************************************/

static bool ftest_tron(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct format_test *test = &fmt->test;
    struct testdata    *data = test->data;
    data->tron               = true;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_troff(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct format_test *test = &fmt->test;
    struct testdata    *data = test->data;
    data->tron               = false;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_assert(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct format_test *test = &fmt->test;
    struct testdata    *data = test->data;
    
    data->prot[opd->a09->pc].check = true;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_endtst(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    static char const *const mfaults[] =
    {
      "internal error---you should never see this one",
      "MC6809_FAULT_INTERNAL_ERROR",
      "MC6809_FAULT_INSTRUCTION",
      "MC6809_FAULT_ADDRESS_MODE",
      "MC6809_FAULT_EXG",
      "MC6809_FAULT_TFR",
    };
    
    struct format_test *test = &fmt->test;
    struct testdata    *data = test->data;
    char const         *tag  = "";
    int                 rc;
    
    data->cpu.pc.w = data->pc;
    data->cpu.S.w  = data->sp - 2;
    
    do
    {
      if (data->memory[data->cpu.pc.w] == data->fill)
      {
        message(opd->a09,MSG_WARNING,"W9994: code went into the weeds\n");
        break;
      }
      
      if (data->prot[data->cpu.pc.w].tron)
      {
        char inst[128];
        char regs[128];
        
        data->dis.pc = data->cpu.pc.w;
        rc = mc6809dis_step(&data->dis,&data->cpu);
        if (rc != 0)
          break;
        mc6809dis_format(&data->dis,inst,sizeof(inst));
        mc6809dis_registers(&data->cpu,regs,sizeof(regs));
        printf("%s | %s\n",regs,inst);
      }
      
      if (data->prot[data->cpu.pc.w].check)
      {
        bool okay;
        
        for (size_t i = 0 ; i < data->numtrig ; i++)
        {
          if (data->triggers[i].here == data->cpu.pc.w)
          {
            okay = runvm(data->a09,&data->cpu,data->triggers[i].prog);
            if (!okay)
            {
              tag = data->triggers[i].tag;
              break;
            }
          }
        }
        
        if (!okay)
        {
          rc = MC6809_FAULT_user;
          break;
        }
      }
      
      rc = mc6809_step(&data->cpu);
    }
    while((rc == 0) && (data->cpu.S.w != data->sp));
    
    if (rc != 0)
    {
      if (rc < MC6809_FAULT_user)
        return message(opd->a09,MSG_ERROR,"E9999: Internal error---%s",mfaults[rc]);
      else // XXX
        return message(opd->a09,MSG_ERROR,"E9999: test failed---%s",tag);
    }
  }
  return true;
}

/**************************************************************************/

static bool ftest_fini(union format *fmt,struct a09 *a09)
{
  assert(fmt          != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(a09          != NULL);
  (void)a09;
  
  struct format_test *test = &fmt->test;
  struct testdata    *data = test->data;
  
  if (data->corefile)
  {
    FILE *fp = fopen(data->corefile,"wb");
    if (fp != NULL)
    {
      fwrite(data->memory,1,65535u,fp);
      fputc(mc6809_cctobyte(&data->cpu),fp);
      fwrite(&data->cpu.A,1,1,fp);
      fwrite(&data->cpu.B,1,1,fp);
      fwrite(&data->cpu.dp,1,1,fp);
      fwrite(&data->cpu.X.b[MSB],1,1,fp);
      fwrite(&data->cpu.X.b[LSB],1,1,fp);
      fwrite(&data->cpu.Y.b[MSB],1,1,fp);
      fwrite(&data->cpu.Y.b[LSB],1,1,fp);
      fwrite(&data->cpu.U.b[MSB],1,1,fp);
      fwrite(&data->cpu.U.b[LSB],1,1,fp);
      fwrite(&data->cpu.S.b[MSB],1,1,fp);
      fwrite(&data->cpu.S.b[LSB],1,1,fp);
      fwrite(&data->cpu.pc.b[MSB],1,1,fp);
      fwrite(&data->cpu.pc.b[LSB],1,1,fp);
      fclose(fp);
    }
    else
      fprintf(stderr,"%s: %s: %s\n",MSG_ERROR,test->data->corefile,strerror(errno));
  }
  free(test->data);
  return true;
}

/**************************************************************************/

bool format_test_init(struct format_test *fmt,struct a09 *a09)
{
  assert(fmt != NULL);
  assert(a09 != NULL);
  (void)a09;
  
  fmt->backend    = BACKEND_TEST;  
  fmt->cmdline    = ftest_cmdline;
  fmt->pass_start = ftest_pass_start;
  fmt->pass_end   = fdefault_pass;
  fmt->inst_write = ftest_inst_write;
  fmt->data_write = ftest_data_write;
  fmt->dp         = fdefault;
  fmt->code       = fdefault;
  fmt->align      = fdefault;
  fmt->end        = fdefault_end;
  fmt->org        = ftest_org;
  fmt->setdp      = fdefault;
  fmt->test       = ftest_test;
  fmt->tron       = ftest_tron;
  fmt->troff      = ftest_troff;
  fmt->assert     = ftest_assert;
  fmt->endtst     = ftest_endtst;
  fmt->fini       = ftest_fini;
  fmt->intest     = false;
  fmt->data       = calloc(1,sizeof(struct testdata));
  
  if (fmt->data != NULL)
  {
    union
    {
      struct memprot p;
      bool           b;
    } init =
    {
      .p =
      {
        .read  = true  ,
        .write = true  ,
        .exec  = false ,
        .tron  = false ,
        .check = false ,
      }
    };
    
    fmt->data->a09       = a09;
    fmt->data->corefile  = NULL;
    fmt->data->cpu.user  = fmt;
    fmt->data->cpu.read  = ft_cpu_read;
    fmt->data->cpu.write = ft_cpu_write;
    fmt->data->cpu.fault = ft_cpu_fault;
    fmt->data->dis.user  = fmt;
    fmt->data->dis.read  = ft_dis_read;
    fmt->data->dis.fault = ft_dis_fault;
    fmt->data->sp        = 0x8000;
    fmt->data->fill      = 0x3F; // SWI instruction
    
    memset(fmt->data->memory,fmt->data->fill,65536u);
    memset(fmt->data->prot,init.b,65536u);
    
    static enum vmops p1[5] = { VM_CPUB   , VM_LIT , 0 , VM_NE , VM_EXIT };
    static enum vmops p2[5] = { VM_CPUCCz , VM_LIT , 0 , VM_NE , VM_EXIT };
    static enum vmops p3[]  = {
        VM_LIT , 0x4003 , VM_AT8  , VM_LIT ,   0x55 , VM_EQ ,
        VM_LIT , 0x4004 , VM_AT16 , VM_LIT , 0xAAAA , VM_EQ ,
        VM_LAND  , VM_EXIT
    };
    static struct vmcode vmcode[] =
    {
      { .here = 0x402C , .prog = p1 , .tag = "degenerate LFSR" },
      { .here = 0x402E , .prog = p2 , .tag = "non-repeating" },
      { .here = 0x4033 , .prog = p3 , .tag = "tis a silly test" },
    };
    
    fmt->data->numtrig = sizeof(vmcode) / sizeof(vmcode[0]);
    fmt->data->triggers = vmcode;
    
    for (size_t i = 0 ; i < fmt->data->numtrig ; i++)
      fmt->data->prot[vmcode[i].here].check = true;
    
    mc6809_reset(&fmt->data->cpu);
    return true;
  }
  else
    return false;
}

/**************************************************************************/
