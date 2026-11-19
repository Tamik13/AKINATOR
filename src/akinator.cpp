#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <ctime>

#include "../include/stack.h"
#include "../include/bin_tree.h"

error_code_e akinator_start     (bin_tree_s* const bin_tree, char** buffer);
error_code_e akinator_guess     (bin_tree_s* const bin_tree);
error_code_e akinator_vis_tree  (bin_tree_s* const bin_tree);
error_code_e akinator_save      (bin_tree_s* const bin_tree);
error_code_e akinator_read      (bin_tree_s* const bin_tree, char** buffer);
error_code_e akinator_ask       (bin_tree_s* const bin_tree);
error_code_e akinator_definition(bin_tree_s* const bin_tree);
error_code_e akinator_compare   (bin_tree_s* const bin_tree);
error_code_e akinator_give_trace(node_s**          node    , stack_s* stack, const char* const buffer);
error_code_e akinator_end       (bin_tree_s* const bin_tree, char* buffer);

void clean_buffer   ();
void read_line      (char* buffer, ssize_t size);
void say_smart_words();
void print_and_say  (const char* const buffer, size_t size ...);

enum command_e {
    NOT_A_COMMAND = 0,
    GUESS         = 1,
    VIS_TREE      = 2,
    DEFINITION    = 3,
    COMPARE       = 4,
    SAVE_AND_QUIT = 5,
    QUIT          = 6
};

const size_t ANSWER_SIZE      = 100;
const size_t MAX_LITERAL_SIZE = 200;

int main() {
    SAFE_CALL( log_start(), )

    char*      buffer   = NULL;
    bin_tree_s bin_tree = {};

    SAFE_CALL( akinator_start(&bin_tree, &buffer), )
    SAFE_CALL( akinator_ask(&bin_tree), )

               say_smart_words();

    SAFE_CALL( akinator_end(&bin_tree, buffer), )
    SAFE_CALL( log_end(), )
    return 0;
}


error_code_e akinator_start(bin_tree_s* const bin_tree, char** buffer) {
    assert(bin_tree != NULL);

    SAFE_CALL( bin_tree_assign(bin_tree), )
    SAFE_CALL( akinator_read(bin_tree, buffer), );

    return SUCCESS;
}


error_code_e akinator_guess(bin_tree_s* const bin_tree) {
    assert(bin_tree != NULL);

    node_s** node                    = &bin_tree->root;
    char     answer[ANSWER_SIZE * 2] = "";

    while (node != NULL) {
        print_and_say("Это %s?", MAX_LITERAL_SIZE, (*node)->data);

        read_line(answer, ANSWER_SIZE);

        if (strcmp(answer, "да") == 0) {

            if ((*node)->left == NULL) {
                print_and_say("Это очевидно!!!", MAX_LITERAL_SIZE);
                return SUCCESS;
            }

            node = &(*node)->left;

        } else if (strcmp(answer, "нет") == 0) {

            if ((*node)->left == NULL) {

                char* new_elem   = (char*)calloc(ANSWER_SIZE, sizeof(char));
                char* difference = (char*)calloc(ANSWER_SIZE, sizeof(char));

                print_and_say("а кто это был?", MAX_LITERAL_SIZE);
                read_line(new_elem, ANSWER_SIZE);

                print_and_say("А чем %s отличается от %s?\n", MAX_LITERAL_SIZE, new_elem, (*node)->data);
                read_line(difference, ANSWER_SIZE);

                SAFE_CALL( bin_tree_insert(bin_tree, &(*node)->left, *node, new_elem), )
                SAFE_CALL( bin_tree_insert(bin_tree, &(*node)->right, *node, (*node)->data), )
                (*node)->data         = difference;
                bin_tree_dmp(bin_tree);

                LOG_OK("akinator_find successfully");
                return SUCCESS;
            }

            node = &(*node)->right;
        } else {
            print_and_say("отвечайте да или нет", MAX_LITERAL_SIZE);
        }
    }

    LOG_ERROR("node is NULL");
    return NULL_PARAM;
}


error_code_e akinator_vis_tree(bin_tree_s* const bin_tree) {
    assert(bin_tree != NULL);

    bin_tree_dmp(bin_tree);

    int status = system("open ./logs/dot/graph.svg");

    if (status == -1) {
        LOG_ERROR("cant system(open ./logs/dot/graph.svg) errno: %s", strerror(errno));
        return SYSTEM_ERROR;
    }

    return SUCCESS;
}


error_code_e akinator_save(bin_tree_s* const bin_tree) {
    assert(bin_tree != NULL);

    char* buffer = NULL;
    bin_tree_print_preorder(bin_tree, &buffer);

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

    assert(count_read > 0 && (size_t)count_read < size);

    ssize_t  data_ind = 1;
    stack_s  parents = {};
    node_s** node   = &bin_tree->root;
    SAFE_CALL ( bin_tree_insert(bin_tree, node, NULL, NULL), free((*buffer)); close(data_base))
    STACK_INIT( parents, 2);
    SAFE_CALL ( stack_push(&parents, node), free((*buffer)); close(data_base))


    for (ssize_t ind = 1; ind < count_read; ind++) {
        if ((*buffer)[ind] == '(') {
            if ((*node)->data == NULL) {
                (*node)->data  = &(*buffer)[data_ind];
            }
            data_ind       = ind + 1;
            (*buffer)[ind] = '\0';
            SAFE_CALL( stack_push(&parents, node), free((*buffer)); close(data_base))

            if ((*node)->left == NULL) {
                SAFE_CALL( bin_tree_insert(bin_tree, &(*node)->left, *node, NULL), free((*buffer)); close(data_base))
                node = &(*node)->left;

            } else if ((*node)->right == NULL) {
                SAFE_CALL( bin_tree_insert(bin_tree, &(*node)->right, *node, NULL), free((*buffer)); close(data_base))
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

    while (!quit) {
        command_e mode = NOT_A_COMMAND;

        print_and_say("\nЧего желаете, сударь?", MAX_LITERAL_SIZE);

        printf("1)Отгадать\n"
               "2)Показать дерево\n"
               "3)Дать определение\n"
               "4)Сравнить два элемента\n"
               "5)Выйти с сохранением\n"
               "6)Выйти без сохранения\n");

        scanf("%d", &mode);
        clean_buffer();

        switch(mode) {
            case GUESS:
                SAFE_CALL( akinator_guess(bin_tree), )
                break;

            case VIS_TREE:
                SAFE_CALL( akinator_vis_tree(bin_tree), )
                break;

            case DEFINITION:
                SAFE_CALL( akinator_definition(bin_tree), )
                break;

            case COMPARE:
                SAFE_CALL( akinator_compare(bin_tree), );
                break;

            case SAVE_AND_QUIT:
                quit = true;
                SAFE_CALL( akinator_save(bin_tree), )
                break;

            case QUIT:
                quit = true;
                break;

            case NOT_A_COMMAND:
            default:
                print_and_say("Я не понял, повтори\n", MAX_LITERAL_SIZE);
        }
    }

    return SUCCESS;
}


error_code_e akinator_definition(bin_tree_s* const bin_tree) {
    assert(bin_tree != NULL);

    char buffer[ANSWER_SIZE] = "";

    print_and_say("\nВведите элемент которому хотите дать определение: ", MAX_LITERAL_SIZE);
    read_line(buffer, ANSWER_SIZE);

    stack_s stack = {};
    STACK_INIT(stack, 2);

    SAFE_CALL( akinator_give_trace(&bin_tree->root, &stack, buffer), );

    if (stack.size == 0) {
        print_and_say("Такого элемента нет", MAX_LITERAL_SIZE);
        LOG_OK("akinator success");
        return SUCCESS;
    }

    node_s** parent = NULL;
    node_s** node   = NULL;

    while (stack.size > 0) {

        parent = node;
        SAFE_CALL( stack_pop(&stack, &node), )

        if (parent != NULL) {

            if ((*parent)->left == *node) {
                print_and_say("%s", MAX_LITERAL_SIZE, (*parent)->data);

            } else {
                print_and_say("не %s", MAX_LITERAL_SIZE, (*parent)->data);
            }

        }
    }

    return SUCCESS;
}

error_code_e akinator_compare(bin_tree_s* const bin_tree) {
    assert(bin_tree != NULL);

    char first_buffer[ANSWER_SIZE]  = "";
    char second_buffer[ANSWER_SIZE] = "";

    print_and_say("\nВведите первый элемент: ", MAX_LITERAL_SIZE);
    read_line(first_buffer, ANSWER_SIZE);

    stack_s first_stack = {};
    STACK_INIT(first_stack, 2);

    SAFE_CALL( akinator_give_trace(&bin_tree->root, &first_stack, first_buffer), );

    if (first_stack.size == 0) {
        print_and_say("Такого элемента нет", MAX_LITERAL_SIZE);
        LOG_OK("akinator success");
        return SUCCESS;
    }

    print_and_say("\nВведите второй элемент: ", MAX_LITERAL_SIZE);
    read_line(second_buffer, ANSWER_SIZE);

    stack_s second_stack = {};
    STACK_INIT(second_stack, 2);

    SAFE_CALL( akinator_give_trace(&bin_tree->root, &second_stack, second_buffer), );

    if (second_stack.size == 0) {
        print_and_say("Такого элемента нет", MAX_LITERAL_SIZE);
        LOG_OK("akinator success");
        return SUCCESS;
    }

    node_s** first_node    = NULL;
    node_s** second_node   = NULL;
    node_s** first_parent  = NULL;
    node_s** second_parent = NULL;

    print_and_say("Они похожи тем, что", MAX_LITERAL_SIZE);

    while (first_stack.size > 0 && second_stack.size > 0) {

        first_parent  = first_node;
        second_parent = second_node;
        SAFE_CALL( stack_pop(&first_stack,  &first_node) , )
        SAFE_CALL( stack_pop(&second_stack, &second_node), )


        if (first_parent != NULL) {

            if (strncmp((*first_parent)->data, (*second_parent)->data, ANSWER_SIZE)) {
                break;
            }

            if ((*first_parent)->left == *first_node) {
                print_and_say("%s", MAX_LITERAL_SIZE, (*first_parent)->data);

            } else {
                print_and_say("не %s", MAX_LITERAL_SIZE, (*first_parent)->data);
            }

        }

    }

    print_and_say("Но %s в отличие от %s", MAX_LITERAL_SIZE, first_buffer, second_buffer);

    while (first_stack.size > 0) {

        if (first_parent != NULL) {

            if ((*first_parent)->left == *first_node) {
                print_and_say("%s", MAX_LITERAL_SIZE, (*first_parent)->data);

            } else {
                print_and_say("не %s", MAX_LITERAL_SIZE, (*first_parent)->data);
            }

        }

        first_parent = first_node;
        SAFE_CALL( stack_pop(&first_stack,  &first_node),  )

    }

    if (first_parent != NULL) {

        if ((*first_parent)->left == *first_node) {
            print_and_say("%s", MAX_LITERAL_SIZE, (*first_parent)->data);

        } else {
            print_and_say("не %s", MAX_LITERAL_SIZE, (*first_parent)->data);
        }

    }

    while (second_stack.size > 0) {

        if (second_parent != NULL) {

            if ((*second_parent)->left == *second_node) {
                print_and_say("не %s", MAX_LITERAL_SIZE, (*second_parent)->data);

            } else {
                print_and_say("%s",    MAX_LITERAL_SIZE, (*second_parent)->data);
            }

        }

        second_parent = second_node;
        SAFE_CALL( stack_pop(&second_stack, &second_node), )

    }

    if (second_parent != NULL) {

        if ((*second_parent)->left == *second_node) {
            print_and_say("не %s", MAX_LITERAL_SIZE, (*second_parent)->data);

        } else {
            print_and_say("%s",    MAX_LITERAL_SIZE, (*second_parent)->data);
        }

    }

    return SUCCESS;
}


error_code_e akinator_give_trace(node_s** node, stack_s* const stack, const char* const buffer) {
    assert(node != NULL);

    if (strncmp(buffer, (*node)->data, ANSWER_SIZE) == 0) {
        SAFE_CALL( stack_push(stack, node), )
        return SUCCESS;
    }

    if ((*node)->left)                      SAFE_CALL( akinator_give_trace(&(*node)->left,  stack, buffer), )
    if ((*node)->right && stack->size == 0) SAFE_CALL( akinator_give_trace(&(*node)->right, stack, buffer), )

    if (stack->size > 0) {
        SAFE_CALL( stack_push(stack, node), )
    }

    return SUCCESS;
}


error_code_e akinator_end(bin_tree_s* const bin_tree, char* buffer) {
    assert(bin_tree != NULL);
    assert(buffer   != NULL);

    free(buffer);
    buffer = NULL;
    SAFE_CALL( bin_tree_destroy(bin_tree), )

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

    LOG_ERROR("cant read line");
    return;
}


void say_smart_words() {
    srand((unsigned int)time(NULL));
    size_t krichalka = (size_t)rand() % 6;

    switch (krichalka) {
        case 0:
            print_and_say("Слышно нам на всю долгопу как фифты друг друга в попу!", MAX_LITERAL_SIZE);
            return;

        case 1:
            print_and_say("Член эртэшный чмоки-чмоки - у фопфоф устали щеки!", MAX_LITERAL_SIZE);
            return;

        case 2:
            print_and_say("Там, где застрянут эРТэшные танки – смело проходят БэМские тянки!", MAX_LITERAL_SIZE);
            return;

        case 3:
            print_and_say("Что гремит? Что так грохочет? Факи сосать по горло хочет!", MAX_LITERAL_SIZE);
            return;

        case 4:
            print_and_say("От коробки до энка - лучше всех ФэРТэКа", MAX_LITERAL_SIZE);
            return;

        case 5:
            print_and_say("ЭРТЭ ЭРТЭ ЭРТЭ ЭРТЭ ЭРТЭ", MAX_LITERAL_SIZE);
            return;

        default:
            LOG_ERROR("incorrect switch");
            return;
    }

    LOG_ERROR("incorrect switch");
    return;
}


void print_and_say(const char* const buffer, const size_t size ...) {
    assert(buffer != NULL);

    va_list ap = {};
    va_start(ap, size);

    size_t new_buffer_size = size * 10;
    char*  new_buffer      = (char*)calloc(new_buffer_size, sizeof(char));

    if (new_buffer == NULL) {
        LOG_ERROR("cant new_buffer allocate");
        return;
    }

    if (vsnprintf(new_buffer, size, buffer, ap) == -1) {
        LOG_ERROR("error in vsnprintf");
        return;
    }

    printf("%s\n", new_buffer);

    char* cmd = (char*)calloc(new_buffer_size + sizeof("say \"\""), sizeof(char));

    if (cmd == NULL) {
        LOG_ERROR("cnat cmd allocate");
        return;
    }

    if (snprintf(cmd, size, "say \"%s\"", new_buffer) == -1) {
        LOG_ERROR("error in vsnprintf");
        return;
    }

    system(cmd);

    free(new_buffer);
    free(cmd);
    return;
}
