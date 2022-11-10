#include <sys/syscall.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>

// https://man7.org/linux/man-pages/man2/mmap.2.html
static void* __mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return (void*) syscall(SYS_mmap, addr, length, prot, flags, fd, offset);
}

#define MIN 5
#define LEVELS 8
#define PAGE 4096

// template code stolen from https://people.kth.se/~johanmon/ose/assignments/buddy.pdf

enum flag {
    FREE,
    TAKEN
};

struct head {
    enum flag status;
    short int level;
    struct head *next;
    struct head *prev;
};

struct head *flists[LEVELS] = {NULL};


struct head* new() {
    struct head* new = (struct head *) __mmap(NULL, PAGE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    printf("mmaped addr: %p\n", new);
    if (new == MAP_FAILED) {
        return NULL;
    }
    assert(((long int) new & 0xfff) == 0);
    new->status = FREE;
    new->level = LEVELS - 1;
    flists[new->level] = new;
    return new;
}

struct head *buddy(struct head* block) {
    int index = block->level;
    long int mask = 0x1 << (index + MIN);
    return (struct head*)((long int)block ^ mask);
}

struct head *split(struct head* block) {
    int index = block->level - 1;
    int mask = 0x1 << (index + MIN);
    return (struct head *)((long int) block | mask);
}

struct head *primary(struct head* block) {
    int index = block->level -1;
    int mask = 0xffffffffffffffff << (1 + index + MIN);
    return (struct head *)((long int) block & mask);
}

int level(int req) {
    int total = req + sizeof(struct head);

    int i = 0;
    int size = 1 << MIN;
    while (size < total) {
        i++;
        size = size << 1;
    }
    return i;
}

void append_to_flist(struct head *new_head) {
    if (flists[new_head->level] == NULL) {
        flists[new_head->level] = new_head;
        return;
    }

    struct head *old_head = flists[new_head->level];

    flists[new_head->level] = new_head;
    new_head->next = old_head;
    old_head->prev = new_head;
}

void remove_from_flist(struct head *deletee) {
    printf("deletee: %p, level: %d\n", deletee, deletee->level);
    if (deletee->next != NULL) {
        deletee->next->prev = deletee->prev;
    }
    if (deletee->prev != NULL) {
        deletee->prev->next = deletee->next;
    }
    if (flists[deletee->level] == deletee) {
        flists[deletee->level] = deletee->next;
    }
}

struct head* find(int index) {
    // find the smallest eligable block (upwards)
    struct head *flh;

    for (int i = index; i < LEVELS; i++) {
        flh = flists[i];
        if (flh != NULL) {
            break;
        }
    }
    if (flh == NULL) {
        flh = new();
    }

    remove_from_flist(flh);

    // split chunks until we get to the desired level, 
    // appending new buddies to the flists in the process (downwards)
    while (index != flh->level) {
        struct head* buddy = split(flh);
        flh->level -= 1;
        buddy->level = flh->level;
        append_to_flist(buddy);
    }

    flh->status = TAKEN;

    return flh;
}

void* molloch(size_t n) {
    if (n == 0) {
        return NULL;
    }
    int index = level(n);
    printf("allocating %zu bytes, level %d\n", n, index);
    struct head *taken = find(index);
    return (void*) taken + sizeof(struct head);
}

void sacrifice(void* ptr) {
    struct head *curr = (struct head *) (ptr - sizeof(struct head));

    while (1) {
        
        curr->status = FREE;

        struct head *bud = buddy(curr);
        remove_from_flist(curr);

        if (curr->level == LEVELS - 1) {
            break;
        }

        if (bud->status != FREE) {
            break;
        }

        // morg time

        remove_from_flist(bud);

        curr = primary(curr);
        curr->level = curr->level + 1;

    }
    append_to_flist(curr);

}

void debug() {
    for (int i = LEVELS - 1; i >= 0; i--) {
        printf("level %d, head: %p", i, flists[i]);
        struct head *x = flists[i];
        while (x != NULL && x->next != NULL) {
            printf(" -> %p", x->next);
            x = x->next;
        }

        printf("\n");
    }
}