#include "argdefgen/action.h"
#include "argdefgen/node.h"
#include "argdefgen/str.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ------------------------------------------------------------------------- */
struct adg_action*
adg_action_create(void)
{
    struct adg_action* action = malloc(sizeof *action);
    if (action == NULL)
        return NULL;

    memset(action, 0, sizeof *action);
    return action;
}

/* ------------------------------------------------------------------------- */
void
adg_action_destroy(struct adg_action* action)
{
    if (action->action_name)  adg_str_free(action->action_name);
    if (action->section_name) adg_str_free(action->section_name);
    if (action->long_option)  adg_str_free(action->long_option);
    if (action->arg_doc)      adg_str_free(action->arg_doc);
    if (action->help)         adg_str_free(action->help);
    if (action->func_name)    adg_str_free(action->func_name);

    if (action->runafter) free(action->runafter);
    if (action->requires) free(action->requires);
    if (action->metadeps) free(action->metadeps);

    free(action);
}

/* ------------------------------------------------------------------------- */
struct adg_action**
adg_action_table_new_empty(void)
{
    return calloc(1, sizeof(struct adg_action*));
}

/* ------------------------------------------------------------------------- */
static int
count_actions_in_tree(union adg_node* node)
{
    int counter = adg_node_is_action(node) ? 1 : 0;

    if (node->base.left)
        counter += count_actions_in_tree(node->base.left);
    if (node->base.right)
        counter += count_actions_in_tree(node->base.right);

    return counter;
}

/* ------------------------------------------------------------------------- */
static char
get_short_option(union adg_node* node)
{
    assert(adg_node_is_action(node));
    switch (node->info.type)
    {
        case ADG_EXPLICIT_ACTION      : return node->explicit_action.shortopt;
        case ADG_EXPLICIT_META_ACTION : return node->explicit_meta_action.shortopt;

        default : return '\0';
    }
}

/* ------------------------------------------------------------------------- */
static const char*
get_long_option(union adg_node* node)
{
    assert(adg_node_is_action(node));
    switch (node->info.type)
    {
        case ADG_EXPLICIT_ACTION      : return node->explicit_action.longopt;
        case ADG_EXPLICIT_META_ACTION : return node->explicit_meta_action.longopt;
        case ADG_IMPLICIT_ACTION      : return node->implicit_action.name;
        case ADG_IMPLICIT_META_ACTION : return node->implicit_meta_action.name;

        default : assert(0); return NULL;
    }
}

/* ------------------------------------------------------------------------- */
static const char*
find_func_name(union adg_node* node)
{
    assert(adg_node_is_action(node));
    union adg_node* attr = (union adg_node*)node->action_base.attrs;
    while (attr)
    {
        assert(attr->info.type == ADG_ACTIONATTRS);
        if (attr->actionattrs.attr->info.type == ADG_FUNC)
            return attr->actionattrs.attr->func.name;

        attr = (union adg_node*)attr->actionattrs.next;
    }

    return NULL;
}

/* ------------------------------------------------------------------------- */
static const char*
find_help(union adg_node* node)
{
    assert(adg_node_is_action(node));
    union adg_node* attr = (union adg_node*)node->action_base.attrs;
    while (attr)
    {
        assert(attr->info.type == ADG_ACTIONATTRS);
        if (attr->actionattrs.attr->info.type == ADG_HELP)
            return attr->actionattrs.attr->help.text;

        attr = (union adg_node*)attr->actionattrs.next;
    }

    return NULL;
}

/* ------------------------------------------------------------------------- */
static union adg_node*
find_args_node(union adg_node* node)
{
    assert(adg_node_is_action(node));
    union adg_node* attr = (union adg_node*)node->action_base.attrs;
    while (attr)
    {
        assert(attr->info.type == ADG_ACTIONATTRS);
        if (attr->actionattrs.attr->info.type == ADG_ARG || attr->actionattrs.attr->info.type == ADG_OPTIONAL_ARG)
            return (union adg_node*)attr->actionattrs.attr;

        attr = (union adg_node*)attr->actionattrs.next;
    }

    return NULL;
}

/* ------------------------------------------------------------------------- */
static int
init_action_argdoc_and_range(struct adg_action* action, union adg_node* node)
{
    /* We'll use these as counters for number of required and number of
     * optional args */
    action->arg_doc = adg_str_dup("");
    action->arg_range.l = 0;
    action->arg_range.h = 0;

    union adg_node* arg = find_args_node(node);
    while (arg)
    {
        if (arg->info.type == ADG_ARG)
        {
            int i;
            union adg_node* argname;

            action->arg_doc = adg_str_append(action->arg_doc, "<");
            for (argname = (union adg_node*)arg->arg.argnames, i = 0;
                 argname;
                 argname = (union adg_node*)argname->argname.next, i++)
            {
                if (i != 0)
                    action->arg_doc = adg_str_append(action->arg_doc, "|");
                action->arg_doc = adg_str_append(action->arg_doc, argname->argname.str);
            }
            action->arg_doc = adg_str_append(action->arg_doc, ">");
            action->arg_range.l++;
            action->arg_range.h++;
            arg = (union adg_node*)arg->arg.next;
        }
        else if (arg->info.type == ADG_OPTIONAL_ARG)
        {
            int i;
            union adg_node* argname;

            action->arg_doc = adg_str_append(action->arg_doc, "[");
            for (argname = (union adg_node*)arg->optional_arg.argnames, i = 0;
                 argname;
                 argname = (union adg_node*)argname->argname.next, i++)
            {
                if (i != 0)
                    action->arg_doc = adg_str_append(action->arg_doc, "|");
                action->arg_doc = adg_str_append(action->arg_doc, argname->argname.str);
            }
            if (arg->optional_arg.continued)
            {
                action->arg_doc = adg_str_append(action->arg_doc, "...");
                action->arg_range.h = -1;  /* indicates infinite additional optional args */
            }
            else
                action->arg_range.h++;
            action->arg_doc = adg_str_append(action->arg_doc, "]");
            arg = (union adg_node*)arg->optional_arg.next;
        }
        else
        {
            fprintf(stderr, "Encountered invalid node type `%d' in args linked list\n", node->info.type);
            return -1;
        }

        /* Space between each argument */
        if (arg)
            action->arg_doc = adg_str_append(action->arg_doc, " ");
    }

    /* No arguments is also valid */
    return 0;
}

/* ------------------------------------------------------------------------- */
static int
populate_action_table_from_tree(struct adg_action*** listp, union adg_node* node, const char* section_name)
{
    if (adg_node_is_action(node))
    {
        const char* help = find_help(node);
        const char* long_option = get_long_option(node);
        const char* func_name = find_func_name(node);
        char short_option = get_short_option(node);

        struct adg_action* action = adg_action_create();
        if (action == NULL)
            goto create_action_failed;

        action->section_name = adg_str_dup(section_name);
        if (action->section_name == NULL)
            goto init_action_failed;

        /* The action name is identical to the parsed long option and we use
         * this to refer to the action later when solving dependencies. Must
         * always be set. */
        action->action_name = adg_str_dup(long_option);
        if (action->action_name == NULL)
            goto init_action_failed;

        switch (node->info.type)
        {
            case ADG_EXPLICIT_META_ACTION:
                action->is_meta = 1;
                /* fallthrough */
            case ADG_EXPLICIT_ACTION: {
                if (help == NULL)
                {
                    fprintf(stderr, "Error: Action `%s' has no help attribute. Explicit actions must specify a help string.\n", long_option);
                    goto init_action_failed;
                }
                action->help = adg_str_dup(help);
                if (action->help == NULL)
                    goto init_action_failed;

                if (func_name == NULL)
                {
                    fprintf(stderr, "Error: Action `%s' has no func attribute. Explicit actions must specify a function name.\n ", long_option);
                    goto init_action_failed;
                }
                action->func_name = adg_str_dup(func_name);
                if (action->func_name == NULL)
                    goto init_action_failed;

                action->short_option = short_option;
                action->long_option = adg_str_dup(long_option);
                if (action->long_option == NULL)
                    goto init_action_failed;

                if (init_action_argdoc_and_range(action, node) != 0)
                    goto init_action_failed;
            } break;

            case ADG_IMPLICIT_META_ACTION:
                action->is_meta = 1;
                /* fallthrough */
            case ADG_IMPLICIT_ACTION: {
                if (help)
                    fprintf(stderr, "Warning: Action `%s' has a help attribute, but it will be ignored because the action is implicit.\n", long_option);
                if (find_args_node(node))
                    fprintf(stderr, "Warning: Action `%s' has an args attribute, but it will be ignored because the action is implicit.\n", long_option);

                if (func_name == NULL)
                {
                    fprintf(stderr, "Error: Action `%s' has no func attribute. Implicit actions must specify a function name.\n ", long_option);
                    goto init_action_failed;
                }
                action->func_name = adg_str_dup(func_name);
                if (action->func_name == NULL)
                    goto init_action_failed;

                action->long_option = adg_str_dup("");
                if (action->long_option == NULL)
                    goto init_action_failed;
                action->arg_doc = adg_str_dup("");
                if (action->arg_doc == NULL)
                    goto init_action_failed;
                action->help = adg_str_dup("");
                if (action->help == NULL)
                    goto init_action_failed;

                action->is_implicit = 1;
            } break;

            default: assert(0); break;
        }

        **listp = action;
        (*listp)++;
        goto action_success;

        init_action_failed      : adg_action_destroy(action);
        create_action_failed    : return -1;
    } action_success:

    if (node->info.type == ADG_SECTION)
        section_name = node->section.name;

    if (node->base.left)
        if (populate_action_table_from_tree(listp, node->base.left, section_name) != 0)
            return -1;
    if (node->base.right)
        if (populate_action_table_from_tree(listp, node->base.right, section_name) != 0)
            return -1;

    return 0;
}

/* ------------------------------------------------------------------------- */
static int
verify_actions_and_sections_are_unique(struct adg_action** action_table)
{
    struct adg_action** a1;
    struct adg_action** a2;
    for (a1 = action_table; *a1; ++a1)
        for (a2 = a1+1; *a2; ++a2)
            if (strcmp((*a1)->action_name, (*a2)->action_name) == 0)
            {
                fprintf(stderr, "Error: Duplicate action name `%s'\n", (*a1)->section_name);
                return -1;
            }

    for (a1 = action_table; *a1; ++a1)
        for (a2 = a1; *a2; ++a2)
            if (strcmp((*a1)->section_name, (*a2)->action_name) == 0)
            {
                fprintf(stderr, "Error: Action `%s' has the same name as a section\n", (*a1)->section_name);
                return -1;
            }

    return 0;
}

/* ------------------------------------------------------------------------- */
static union adg_node*
find_action_node(union adg_node* node, const char* name)
{
    union adg_node* found;
    if (adg_node_is_action(node))
        if (strcmp(get_long_option(node), name) == 0)
            return node;

    if (node->base.left)
        if ((found = find_action_node(node->base.left, name)) != NULL)
            return found;
    if (node->base.right)
        if ((found = find_action_node(node->base.right, name)) != NULL)
            return found;

    return NULL;
}

/* ------------------------------------------------------------------------- */
static union adg_node*
find_node(union adg_node* node, enum adg_node_type type)
{
    union adg_node* found;
    if (node->info.type == type)
        return node;

    if (node->base.left)
        if ((found = find_node(node->base.left, type)) != NULL)
            return found;
    if (node->base.right)
        if ((found = find_node(node->base.right, type)) != NULL)
            return found;

    return NULL;
}

/* ------------------------------------------------------------------------- */
static int
init_action_table_dependencies(struct adg_action** action_table, union adg_node* root)
{
    struct adg_action** ptr;
    for (ptr = action_table; *ptr; ++ptr)
    {
        union adg_node* node;
        struct adg_action* action = *ptr;
        union adg_node* action_node = find_action_node(root, action->action_name);
        if (action_node == NULL)
        {
            fprintf(stderr, "Error: Something horrible happened: Action `%s' in table could not be found in AST, should never happen\n", action->action_name);
            return -1;
        }

        action->runafter = malloc(sizeof(*action->runafter));
        if (action->runafter == NULL)
            return -1;
        action->runafter[0] = -1;

        action->requires = malloc(sizeof(*action->requires));
        if (action->requires == NULL)
            return -1;
        action->requires[0] = -1;

        action->metadeps = malloc(sizeof(*action->metadeps));
        if (action->metadeps == NULL)
            return -1;
        action->metadeps[0] = -1;

        /* Fill in runafter dependency list */
        node = find_node((union adg_node*)action_node->action_base.attrs, ADG_RUNAFTER);
        if (node != NULL)
        {
            union adg_node* runafter;
            int idx = 0;

            for (runafter = node; runafter; runafter = (union adg_node*)runafter->runafter.next)
            {
                struct adg_action** a;
                const char* runafter_str = runafter->runafter.str;
                int found = 0;
                for (a = action_table; *a; ++a)
                    if (strcmp((*a)->section_name, runafter_str) == 0 ||
                        strcmp((*a)->action_name, runafter_str) == 0)
                    {
                        void* newmem = realloc(action->runafter, (idx + 2) * sizeof(*action->runafter));
                        if (newmem == NULL)
                            return -1;
                        action->runafter = newmem;
                        action->runafter[idx++] = a - action_table;
                        action->runafter[idx] = -1;
                        found = 1;
                    }
                if (found == 0)
                {
                    fprintf(stderr, "Error: Undefined action or section `%s' referenced in runafter list in action `%s'\n", runafter_str, action->action_name);
                    return -1;
                }
            }
        }

        /* Fill in requires dependency list */
        node = find_node((union adg_node*)action_node->action_base.attrs, ADG_REQUIRES);
        if (node != NULL)
        {
            union adg_node* requires;
            int idx = 0;

            for (requires = node; requires; requires = (union adg_node*)requires->requires.next)
            {
                struct adg_action** a;
                const char* requires_str = requires->requires.str;
                int found = 0;
                for (a = action_table; *a; ++a)
                    if (strcmp((*a)->section_name, requires_str) == 0 ||
                        strcmp((*a)->action_name, requires_str) == 0)
                    {
                        void* newmem = realloc(action->requires, (idx + 2) * sizeof(*action->requires));
                        if (newmem == NULL)
                            return -1;
                        action->requires = newmem;
                        action->requires[idx++] = a - action_table;
                        action->requires[idx] = -1;
                        found = 1;
                    }
                if (found == 0)
                {
                    fprintf(stderr, "Error: Undefined action or section `%s' referenced in requires list in action `%s'\n", requires_str, action->action_name);
                    return -1;
                }
            }
        }

        /* Fill in metadeps dependency list */
        node = find_node((union adg_node*)action_node->action_base.attrs, ADG_METADEP);
        if (node != NULL)
        {
            union adg_node* metadep;
            int idx = 0;

            for (metadep = node; metadep; metadep = (union adg_node*)metadep->metadep.next)
            {
                struct adg_action** a;
                const char* metadep_str = metadep->metadep.str;
                int found = 0;
                for (a = action_table; *a; ++a)
                    if (strcmp((*a)->section_name, metadep_str) == 0 ||
                        strcmp((*a)->action_name, metadep_str) == 0)
                    {
                        void* newmem = realloc(action->metadeps, (idx + 2) * sizeof(*action->metadeps));
                        if (newmem == NULL)
                            return -1;
                        action->metadeps = newmem;
                        action->metadeps[idx++] = a - action_table;
                        action->metadeps[idx] = -1;
                        found = 1;
                    }
                if (found == 0)
                {
                    fprintf(stderr, "Error: Undefined action or section `%s' referenced in metadeps list in action `%s'\n", metadep_str, action->action_name);
                    return -1;
                }
            }
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
struct adg_action**
adg_action_table_from_nodes(union adg_node* root)
{
    int num_actions;
    struct adg_action** action_table;
    struct adg_action** listp;

    /* Root node must be a section. */
    if (root->info.type != ADG_SECTION)
    {
        fprintf(stderr, "Error: Expected a section\n");
        return NULL;
    }

    /* +1 is a sentinel so we know how long the list is */
    num_actions = count_actions_in_tree(root);
    action_table = malloc((num_actions + 1) * sizeof(*action_table));
    if (action_table == NULL)
        goto alloc_action_table_failed;
    memset(action_table, 0, (num_actions + 1) *  sizeof(*action_table));

    listp = action_table;
    if (populate_action_table_from_tree(&listp, root, root->section.name) != 0)
        goto populate_action_table_failed;
    if (verify_actions_and_sections_are_unique(action_table) != 0)
        goto populate_action_table_failed;
    if (init_action_table_dependencies(action_table, root) != 0)
        goto populate_action_table_failed;

    return action_table;

    populate_action_table_failed : adg_action_table_destroy(action_table);
    alloc_action_table_failed    : return NULL;
}

/* ------------------------------------------------------------------------- */
void
adg_action_table_destroy(struct adg_action** action_table)
{
    struct adg_action** ptr;
    for (ptr = action_table; *ptr; ++ptr)
        adg_action_destroy(*ptr);
    free(action_table);
}