#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "simple-trace.h"

#define fatal(ec, fmt, args...) do {                            \
        fprintf(stderr, "[ERROR] "fmt"\n", ## args);            \
        fflush(stderr);                                         \
        exit(ec);                                               \
    } while (0)

#define info(fmt, args...) do {                                 \
        fprintf(stdout, "[INFO] "fmt"\n", ## args);             \
        fflush(stdout);                                         \
    } while (0)





int main(int argc, char *argv[])
{
    int  ctx, i, n;
    
    /* trace flags */
    int DBG_GRAPH, DBG_VAR, DBG_RESOLVE, DBG_ACTION, DBG_VM;

    TRACE_DECLARE_MODULE(test, "test",
        TRACE_FLAG("graph"  , "dependency graph"    , &DBG_GRAPH),
        TRACE_FLAG("var"    , "variable handling"   , &DBG_VAR),
        TRACE_FLAG("resolve", "dependency resolving", &DBG_RESOLVE),
        TRACE_FLAG("action" , "action processing"   , &DBG_ACTION),
        TRACE_FLAG("vm"     , "VM execution"        , &DBG_VM));

    
    trace_init();

    if ((ctx = trace_context_add("test")) < 0)
        fatal(1, "failed to create test trace context");
  
    if (trace_module_add(ctx, &test) != 0)
        fatal(1, "failed to add trace module test (%d: %s)",
              errno, strerror(errno));
            
    info("registered trace module test registered...");
            
    info("got debug flags:");
    info("graph: 0x%x, var: 0x%x, res: 0x%x, act: 0x%x, vm: 0x%x",
         DBG_GRAPH, DBG_VAR, DBG_RESOLVE, DBG_ACTION, DBG_VM);


#define SHOW(n) do {                               \
        printf("%s,%s,%s,%s,%s\n",                 \
               i & 0x01 ? "GRAPH" : "-",           \
               i & 0x02 ? "VAR" : "-",             \
               i & 0x04 ? "RESOLVE" : "-",         \
               i & 0x08 ? "ACTION" : "-",          \
               i & 0x10 ? "VM" : "-");             \
    } while (0)

#define FLIP(id, b)                                                     \
    ((i) & (0x1<<(b)) ? trace_flag_set : trace_flag_clr)(DBG_##id)
    
    for (n = 0; n < 2; n++) {
        if (n & 0x1)
            trace_context_enable(ctx);
        else
            trace_context_disable(ctx);
        
        for (i = 0; i < 32; i++) {
            SHOW(i);
            FLIP(GRAPH, 0);
            FLIP(VAR, 1);
            FLIP(RESOLVE, 2);
            FLIP(ACTION, 3);
            FLIP(VM, 4);
            
            if (n > 0)
                usleep((i & 0x3) * 333333);
            trace_write(DBG_GRAPH  , "DBG_GRAPH");
            trace_write(DBG_VAR    , "DBG_VAR");
            trace_write(DBG_RESOLVE, "DBG_RESOLVE");
            trace_write(DBG_ACTION , "DBG_ACTION");
            trace_write(DBG_VM     , "DBG_VM");
        }
    }

    if (trace_module_del(ctx, "test") != 0)
        fatal(1, "failed to remove trace module %s (%d: %s)", "test",
              errno, strerror(errno));
            
    info("test trace module %s removed...", "test");
    
    return 0;
}




/* 
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim:set expandtab shiftwidth=4:
 */
