#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "util.h"
#include "lexer.h"
#include "parser.h"
#include "symtab.h"
#include "cycle.h"
#include "emit.h"
#include "code.h"
#include "backend.h"


struct label_node {
    struct label_node *next;
    char *label;
    BOOL used;
};


struct label_node *
new_label(struct label_node *top, const char *label)
{
    struct label_node *n = xmalloc(sizeof(struct label_node));
    n->label = xstrdup(label);
    n->next = top;
    n->used = FALSE;
    return n;
}


struct the_node *
create_the_node(void)
{
    struct the_node *n = xmalloc(sizeof(struct the_node));
    n->next = NULL;

    n->labels = NULL;
    n->code = NULL;
    n->jump = NULL;
    n->cond = COND_AL;
    n->attr = 0;

    n->lineno = token.lineno;
    n->filename = strdup(token.filename);

    n->edge[0] = n->edge[1] = NULL;
    n->index = UNDEFINED;
    n->onstack = FALSE;
    n->scc = FALSE;
    return n;
}


static struct the_node *
find_node_with_label(struct the_node *head, const char *label)
{
    /* XXX wow, this is god damn slow */
    while (head) {
        struct label_node *l;
        for (l = head->labels; l; l = l->next) {
            if (!strcmp(l->label, label)) {
                l->used = TRUE;
                return head;
            }
        }
        head = head->next;
    }
    return NULL;
}


static void
link_graph(struct the_node *list)
{
    /* XXX check labels using symtab */
    struct the_node *n;
    for (n = list; n; n = n->next) {
        if (n->attr & ATTRIB_NORETURN) {
            continue;
        }
        n->edge[0] = n->next;
        if (n->cond != COND_AL) {
            n->edge[1] = find_node_with_label(list, n->jump);
        } else if (n->jump) {
            n->edge[0] = find_node_with_label(list, n->jump);
        }
    }
}


void
emit_code(struct the_node *list)
{
    struct the_node *n = reverse_list(list);
    link_graph(n);
    mark_cycles(n);
    while (n) {
        struct label_node *l;
        struct the_node *p = n->next;
        token.lineno = n->lineno;
        free(token.filename);
        token.filename = n->filename;
        for (l = n->labels; l; ) {
            struct label_node *q = l->next;
            emit_label(l->label, l->used);
            free(l->label);
            free(l);
            l = q;
        }
        if (n->cond != COND_AL) {
            n->code = reverse_list(n->code);
            emit_nodes(n->code, NULL, TRUE, n->scc);
            emit_cond(n->jump, n->cond);
            free_nodes(n->code);
            free(n->jump);
        } else if (n->jump) {
            emit_goto(n->jump);
            free(n->jump);
        } else if (n->code) {
            n->code = reverse_list(n->code);
            emit_nodes(n->code, NULL, FALSE, n->scc);
            free_nodes(n->code);
        }
        free(n);
        n = p;
    }
    emit_finalize();
}
