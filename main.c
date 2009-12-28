/* main.c -- chibi-scheme command-line app              */
/* Copyright (c) 2009 Alex Shinn.  All rights reserved. */
/* BSD-style license: http://synthcode.com/license.txt  */

#include "chibi/eval.h"

#define sexp_argv_symbol "*command-line-arguments*"
#define sexp_argv_proc   "(define (command-line-arguments) "sexp_argv_symbol")"

#define sexp_import_prefix "(import ("
#define sexp_import_suffix "))"

#ifdef PLAN9
#define exit_failure() exits("ERROR")
#else
#define exit_failure() exit(70)
#endif

static void repl (sexp ctx) {
  sexp in, out, err;
  sexp_gc_var4(obj, tmp, res, env);
  sexp_gc_preserve4(ctx, obj, tmp, res, env);
  env = sexp_context_env(ctx);
  sexp_context_tracep(ctx) = 1;
  in = sexp_eval_string(ctx, "(current-input-port)", env);
  out = sexp_eval_string(ctx, "(current-output-port)", env);
  err = sexp_eval_string(ctx, "(current-error-port)", env);
  sexp_port_sourcep(in) = 1;
  while (1) {
    sexp_write_string(ctx, "> ", out);
    sexp_flush(ctx, out);
    obj = sexp_read(ctx, in);
    if (obj == SEXP_EOF)
      break;
    if (sexp_exceptionp(obj)) {
      sexp_print_exception(ctx, obj, err);
    } else {
      tmp = sexp_env_bindings(env);
      sexp_context_top(ctx) = 0;
      res = sexp_eval(ctx, obj, env);
      if (sexp_exceptionp(res)) {
        sexp_print_exception(ctx, res, err);
      } else {
#if SEXP_USE_WARN_UNDEFS
        sexp_warn_undefs(ctx, sexp_env_bindings(env), tmp, err);
#endif
        if (res != SEXP_VOID) {
          sexp_write(ctx, res, out);
          sexp_write_char(ctx, '\n', out);
        }
      }
    }
  }
  sexp_gc_release4(ctx);
}

static sexp check_exception (sexp ctx, sexp res) {
  sexp err;
  if (res && sexp_exceptionp(res)) {
    err = sexp_current_error_port(ctx);
    if (! sexp_oportp(err))
      err = sexp_make_output_port(ctx, stderr, SEXP_FALSE);
    sexp_print_exception(ctx, res, err);
    exit_failure();
  }
  return res;
}

#define init_context() if (! ctx) do {                                  \
      ctx = sexp_make_eval_context(NULL, NULL, NULL, heap_size);        \
      env = sexp_context_env(ctx);                                      \
      sexp_gc_preserve2(ctx, tmp, args);                                \
    } while (0)

#define load_init() if (! init_loaded++) do {                           \
      init_context();                                                   \
      check_exception(ctx, sexp_load_standard_env(ctx, env, SEXP_FIVE)); \
    } while (0)

void run_main (int argc, char **argv) {
  char *arg, *impmod, *p;
  sexp env, out=SEXP_FALSE, res=SEXP_VOID, ctx=NULL;
  sexp_sint_t i, j, len, quit=0, print=0, init_loaded=0;
  sexp_uint_t heap_size=0;
  sexp_gc_var2(tmp, args);
  args = SEXP_NULL;

  /* parse options */
  for (i=1; i < argc && argv[i][0] == '-'; i++) {
    switch (argv[i][1]) {
    case 'e':
    case 'p':
      load_init();
      print = (argv[i][1] == 'p');
      arg = ((argv[i][2] == '\0') ? argv[++i] : argv[i]+2);
      res = check_exception(ctx, sexp_read_from_string(ctx, arg));
      res = check_exception(ctx, sexp_eval(ctx, res, env));
      if (print) {
        if (! sexp_oportp(out))
          out = sexp_eval_string(ctx, "(current-output-port)", env);
        sexp_write(ctx, res, out);
        sexp_write_char(ctx, '\n', out);
      }
      quit = 1;
      i++;
      break;
    case 'l':
      load_init();
      arg = ((argv[i][2] == '\0') ? argv[++i] : argv[i]+2);
      check_exception(ctx, sexp_load_module_file(ctx, argv[++i], env));
      break;
    case 'm':
      load_init();
      arg = ((argv[i][2] == '\0') ? argv[++i] : argv[i]+2);
      len = strlen(arg)+strlen(sexp_import_prefix)+strlen(sexp_import_suffix);
      impmod = (char*) malloc(len+1);
      strcpy(impmod, sexp_import_prefix);
      strcpy(impmod+strlen(sexp_import_prefix), arg);
      strcpy(impmod+len-+strlen(sexp_import_suffix), sexp_import_suffix);
      impmod[len] = '\0';
      for (p=impmod; *p; p++)
        if (*p == '.') *p=' ';
      check_exception(ctx, sexp_eval_string(ctx, impmod, env));
      free(impmod);
      break;
    case 'q':
      init_context();
      if (! init_loaded++) sexp_load_standard_parameters(ctx, env);
      break;
    case 'A':
      init_context();
      arg = ((argv[i][2] == '\0') ? argv[++i] : argv[i]+2);
      sexp_add_module_directory(ctx, tmp=sexp_c_string(ctx,arg,-1), SEXP_TRUE);
      break;
    case 'I':
      init_context();
      arg = ((argv[i][2] == '\0') ? argv[++i] : argv[i]+2);
      sexp_add_module_directory(ctx, tmp=sexp_c_string(ctx,arg,-1), SEXP_FALSE);
      break;
    case '-':
      i++;
      goto done_options;
    case 'h':
      arg = ((argv[i][2] == '\0') ? argv[++i] : argv[i]+2);
      heap_size = atol(arg);
      len = strlen(arg);
      if (heap_size && isalpha(arg[len-1])) {
        switch (tolower(arg[len-1])) {
        case 'k': heap_size *= 1024; break;
        case 'm': heap_size *= (1024*1024); break;
        }
      }
      break;
    case 'V':
      printf("chibi-scheme 0.3\n");
      exit(0);
    default:
      fprintf(stderr, "unknown option: %s\n", argv[i]);
      exit_failure();
    }
  }

 done_options:
  if (! quit) {
    load_init();
    if (i < argc)
      for (j=argc-1; j>i; j--)
        args = sexp_cons(ctx, tmp=sexp_c_string(ctx,argv[j],-1), args);
    else
      args = sexp_cons(ctx, tmp=sexp_c_string(ctx,argv[0],-1), args);
    sexp_env_define(ctx, env, sexp_intern(ctx, sexp_argv_symbol), args);
    sexp_eval_string(ctx, sexp_argv_proc, env);
    if (i < argc) {             /* script usage */
      check_exception(ctx, sexp_load(ctx, tmp=sexp_c_string(ctx, argv[i], -1), env));
      tmp = sexp_intern(ctx, "main");
      tmp = sexp_env_ref(env, tmp, SEXP_FALSE);
      if (sexp_procedurep(tmp)) {
        args = sexp_list1(ctx, args);
        check_exception(ctx, sexp_apply(ctx, tmp, args));
      }
    } else {
      repl(ctx);
    }
  }

  sexp_gc_release2(ctx);
}

int main (int argc, char **argv) {
  sexp_scheme_init();
  run_main(argc, argv);
  return 0;
}
