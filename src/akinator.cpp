#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctime>

#include "../include/stack.h"
#include "../include/bin_tree.h"

error_code_e akinator_start     (bin_tree_s* const bin_tree, char** buffer);
error_code_e akinator_find      (bin_tree_s* const bin_tree);
error_code_e akinator_vis_tree  (bin_tree_s* const bin_tree);
error_code_e akinator_save      (bin_tree_s* const bin_tree);
error_code_e akinator_read      (bin_tree_s* const bin_tree, char** buffer);
error_code_e akinator_ask       (bin_tree_s* const bin_tree);
error_code_e akinator_definition(bin_tree_s* const bin_tree);

void clean_buffer   ();
void read_line      (char* buffer, ssize_t size);
void say_smart_words();

const size_t ANSWER_SIZE = 100;

int main() {
    SAFE_CALL( log_start(), )

    char*      buffer   = NULL;
    bin_tree_s bin_tree = {};
    SAFE_CALL( akinator_start(&bin_tree, &buffer), )

    SAFE_CALL( akinator_ask(&bin_tree), )

    say_smart_words();

    free(buffer); // TODO akinator_end
    SAFE_CALL( bin_tree_destroy(&bin_tree), )
    SAFE_CALL( log_end(), )
    return 0;
}


error_code_e akinator_start(bin_tree_s* const bin_tree, char** buffer) {
    assert(bin_tree != NULL);

    SAFE_CALL( bin_tree_assign(bin_tree), )
    SAFE_CALL( akinator_read(bin_tree, buffer), );

    return SUCCESS;
}


error_code_e akinator_find(bin_tree_s* const bin_tree) {
    assert(bin_tree != NULL);

    node_s**      node                    = &bin_tree->root;
    char          answer[ANSWER_SIZE * 2] = "";

    while (node != NULL) {
        snprintf(answer, ANSWER_SIZE * 2, "say \"Это %s?\n\"", (*node)->data);
        printf("Это %s?\n", (*node)->data);
        system(answer);

        read_line(answer, ANSWER_SIZE);

        if (strncmp(answer, "да", 4) == 0) {

            if ((*node)->left == NULL) {
                printf("Это очевидно!!!\n");
                system("say \"Это очевидно\"");
                return SUCCESS;
            }

            node = &(*node)->left;

        } else if (strncmp(answer, "нет", 6) == 0) {

            if ((*node)->left == NULL) {

                char* new_elem   = (char*)calloc(ANSWER_SIZE, sizeof(char));
                char* difference = (char*)calloc(ANSWER_SIZE, sizeof(char));

                printf("а кто это был?\n");
                system("say \"а кто это был?\"");
                read_line(new_elem, ANSWER_SIZE);

                printf("А чем %s отличается от %s?\n", new_elem, (*node)->data);
                snprintf(answer, ANSWER_SIZE * 2, "say \"А чем %s отличается от %s?\n\"", new_elem, (*node)->data);
                system(answer);
                read_line(difference, ANSWER_SIZE);

                SAFE_CALL( bin_tree_insert(bin_tree, &(*node)->left, new_elem), )
                SAFE_CALL( bin_tree_insert(bin_tree, &(*node)->right, (*node)->data), )
                (*node)->data         = difference;
                bin_tree_dmp(bin_tree);

                LOG_OK("akinator_find successfully");
                return SUCCESS;
            }

            node = &(*node)->right;
        } else {
            printf("отвечайте да или нет\n");
            system("say \"отвечайте да или нет\"");
        }
    }

    LOG_ERROR("node is NULL");
    return NULL_PARAM;
}


error_code_e akinator_vis_tree(bin_tree_s* const bin_tree) {
    assert(bin_tree != NULL);

    const char* const file_name = "./logs/dot/graph.dot";

    FILE* dot_file = fopen(file_name, "w");
    if (dot_file == NULL) {
        LOG_ERROR("cant open dot file");
        return ERROR_DURING_OPEN;
    }

    if (bin_tree->count_nodes == 0) {
        LOG_OK("void bin_tree cant be convert in img");
        return INCORRECT_SIZE;
    }

    fprintf(dot_file,   "digraph {\n"
                            "\tnodesep=0.6\n"
                            "\trank_sep=1.0\n"
                            );

    ON_DBG(    ssize_t cnt         = 0;)
               node_s**   node     = NULL;
               stack_s    stack    = {};
               STACK_INIT(stack, bin_tree->count_nodes);
    SAFE_CALL( stack_push(&stack, &bin_tree->root), stack_destroy(&stack));

    while (stack.size > 0) {
        ON_DBG(
        if (cnt++ > bin_tree->count_nodes * 3) {
            LOG_ERROR("infinite cycle");
            return INCORRECT_SIZE;
        }
        )

        SAFE_CALL( stack_pop(&stack, &node), stack_destroy(&stack));
        fprintf(dot_file, "\tnode%p [style=filled; fillcolor=\"" AQUA_HX "\" label = \"" BNT_MDF "\"]\n", (void*)*node, (*node)->data);

        if ((*node)->left != NULL && (*node)->right != NULL) {

            fprintf(dot_file, "\tnode%p -> node%p [color=\"" GREEN_HX "\" label=\"да\"]\n", (void*)*node, (void*)(*node)->left);
            fprintf(dot_file, "\tnode%p -> node%p [color=\"" RED_HX   "\" label=\"нет\"]\n", (void*)*node, (void*)(*node)->right);
            SAFE_CALL( stack_push(&stack, &(*node)->left),  stack_destroy(&stack))
            SAFE_CALL( stack_push(&stack, &(*node)->right), stack_destroy(&stack))

        } else if ((*node)->left != NULL) {

            fprintf(dot_file, "\tnode%p -> node%p [color=\"" GREEN_HX "\" label=\"да\"]\n", (void*)*node, (void*)(*node)->left);
            SAFE_CALL( stack_push(&stack, &(*node)->left),  stack_destroy(&stack))

        } else if ((*node)->right != NULL) {

            fprintf(dot_file, "\tnode%p -> node%p [color=\"" RED_HX   "\" label=\"нет\"]\n", (void*)*node, (void*)(*node)->right);
            SAFE_CALL( stack_push(&stack, &(*node)->right), stack_destroy(&stack))

        }
    }

    fprintf(dot_file, "}");

    if (fclose(dot_file) == EOF) {
        LOG_ERROR("cant close dot file");
        return ERROR_DURING_CLOSE;
    }

    SAFE_CALL( stack_destroy(&stack), );

    int status = system("dot -Tsvg ./logs/dot/graph.dot -o ./logs/dot/graph.svg 2>&1"
                        "| grep -v -E 'Warning:.*(non-ASCII|libgvplugin_pango)' >&2");

    if (status == -1) {
        LOG_ERROR("status of system(dot -Tsvg ./logs/dot/graph.dot -o ./logs/dot/graph.svg) = -1");
        return SYSTEM_ERROR;
    }

    status = system("open ./logs/dot/graph.svg");

    if (status == -1) {
        LOG_ERROR("status of system(open ./logs/dot/graph.svg) = -1");
        return SYSTEM_ERROR;
    }

    LOG_OK("graph graphic dmp successfully");
    return SUCCESS;

    return SUCCESS;
}


error_code_e akinator_save(bin_tree_s* const bin_tree) {
    assert(bin_tree != NULL);

    char* buffer = NULL;
    bin_tree_print_postorder(bin_tree, &buffer);

    int data_file = open("./src/data_base.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);

    if (data_file == -1) {
        LOG_ERROR("cant open data_file");
        return ERROR_DURING_OPEN;
    }

    write(data_file, buffer, strlen(buffer));

    free(buffer);
    buffer = NULL;

    if (close(data_file) == -1) {
        LOG_ERROR("cant close data_file");
        return ERROR_DURING_CLOSE;
    }

    return SUCCESS;
}


error_code_e akinator_read(bin_tree_s* const bin_tree, char** buffer) {
    assert(bin_tree != NULL);

    const char* data_base_name = "./src/data_base.txt";

    size_t  size = 0;
    ssize_t count_read = -1;
    SAFE_CALL( stat_get_size(data_base_name, &size), )

    (*buffer) = (char*)calloc(size, sizeof(char));
    if ((*buffer) == NULL) {
        LOG_ERROR("error during buffer allocation");
        return ALLOCATION_ERROR;
    }

    int data_base = open(data_base_name, O_RDONLY);

    if (data_base == -1) {
        free((*buffer));
        LOG_ERROR("cant open data_base");
        return ERROR_DURING_OPEN;
    }

    if ((count_read = read(data_base, (*buffer), size - 1)) == -1) {
        LOG_ERROR("cant read data_base");
        return ERROR_DURING_READ;
    }

    assert(count_read > 0 && count_read < size);

    ssize_t data_ind = 1;
    stack_s parents = {};
    node_s** node   = &bin_tree->root;
    SAFE_CALL( bin_tree_insert(bin_tree, node, NULL), free((*buffer)); close(data_base))
    STACK_INIT(parents, 2);
    SAFE_CALL( stack_push(&parents, node), free((*buffer)); close(data_base))


    for (ssize_t ind = 1; ind < count_read; ind++) {
        if ((*buffer)[ind] == '(') {
            if ((*node)->data == NULL) {
                (*node)->data  = &(*buffer)[data_ind];
            }
            data_ind       = ind + 1;
            (*buffer)[ind] = '\0';
            SAFE_CALL( stack_push(&parents, node), free((*buffer)); close(data_base))

            if ((*node)->left == NULL) {
                SAFE_CALL( bin_tree_insert(bin_tree, &(*node)->left, NULL), free((*buffer)); close(data_base))
                node = &(*node)->left;

            } else if ((*node)->right == NULL) {
                SAFE_CALL( bin_tree_insert(bin_tree, &(*node)->right, NULL), free((*buffer)); close(data_base))
                node = &(*node)->right;

            } else {
                LOG_ERROR("tread son");
                return INCORRECT_PARAM;
            }

        } else if ((*buffer)[ind] == ')') {
            if ((*node)->data == NULL) {
                (*node)->data  = &(*buffer)[data_ind];
            }
            data_ind       = ind + 1;
            (*buffer)[ind] = '\0';
            SAFE_CALL( stack_pop(&parents, &node), free((*buffer)); close(data_base))

        }
    }

    if (close(data_base) == -1) {
        LOG_ERROR("error during close data_base");
        return ERROR_DURING_CLOSE;
    }

    LOG_OK("akinator_read successfully");
    return SUCCESS;
}


error_code_e akinator_ask(bin_tree_s* const bin_tree) {
    assert(bin_tree != NULL);
    bool quit = false;

    while (!quit) { // TODO new func
        int mode = 0;

        printf("Чего желаете, сударь?\n"
               "1)Отгадать\n"
               "2)Показать дерево\n"
               "3)выйти с сохранением\n"
               "4)выйти без сохранения\n");
        system("say \"Чего желаете, сударь?\"");

        scanf("%d", &mode);
        clean_buffer();

        switch(mode) {
            case 1:
                SAFE_CALL( akinator_find(bin_tree), )
                break;

            case 2:
                SAFE_CALL( akinator_vis_tree(bin_tree), )
                break;

            case 3:
                quit = true;
                SAFE_CALL( akinator_save(bin_tree), )
                break;

            case 4:
                quit = true;
                break;

            default:
                printf("Я не понял, повтори\n");
        }
    }

    return SUCCESS;
}


void clean_buffer() {
    int ch = 0;
    while ((ch = getchar()) != '\n' && ch != EOF) {
        continue;
    }
    return;
}


void read_line(char* buffer, ssize_t size) {
    assert(buffer != NULL);
    assert(size > 0);

    if (fgets(buffer, (int)size, stdin) != NULL) {

        for (ssize_t ind = 0; ind < size; ind++) {
            if (buffer[ind] == '\n') {
                buffer[ind] = '\0';
                return;
            }
        }

        buffer[size - 1] = '\0';
        clean_buffer();
    }

    return;
}


void say_smart_words() {
    srand((unsigned int)time(NULL));
    size_t krichalka = rand() % 6;

    switch (krichalka) {
        case 0:
            system("say \"Слышно нам на всю долгопу как фифты друг друга в попу!\"");
            return;

        case 1:
            system("say \"Член эртэшный чмоки-чмоки - у фопфоф устали щеки!\"");
            return;

        case 2:
            system("say \"Там, где застрянут эРТэшные танки – смело проходят БэМские тянки!\"");
            return;

        case 3:
            system("say \"Что гремит? Что так грохочет? Факи сосать по горло хочет!\"");
            return;

        case 4:
            system("say \"От коробки до энка - лучше всех ФэРТэКа\"");
            return;

        case 5:
            system("say \"ЭРТЭ ЭРТЭ ЭРТЭ ЭРТЭ ЭРТЭ\"");
            return;

        default:
            LOG_ERROR("incorrect switch");
            return;
    }

    LOG_ERROR("incorrect switch");
    return;
}
