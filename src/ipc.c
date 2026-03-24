/*
 * ipc.c — Módulo de IPC: fork() + pipes
 * INF01142 — Sistemas Operacionais — 2026/1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <errno.h>

#include "mandelbrot.h"
#include "ipc.h"

/* =========================================================================
 * Definições de Estrutura
 * ========================================================================= */

typedef struct {
    pid_t pid;      /* PID do filho, ou -1 se livre */
    int   read_fd;  /* Descritor de leitura do pipe */
} PoolEntry;

struct Pool {
    int        max;     /* Capacidade máxima */
    int        active;  /* Filhos ativos */
    PoolEntry *entries; /* Array de processos */
};

/* =========================================================================
 * Gerenciamento do Pool
 * ========================================================================= */

Pool *pool_create(int max_children)
{
    Pool *pool = malloc(sizeof(Pool));
    if (!pool) return NULL;

    pool->max    = max_children;
    pool->active  = 0;
    pool->entries = malloc(sizeof(PoolEntry) * max_children);
    
    if (!pool->entries) {
        free(pool);
        return NULL;
    }

    for (int i = 0; i < max_children; i++) {
        pool->entries[i].pid     = -1;
        pool->entries[i].read_fd = -1;
    }

    return pool;
}

void pool_destroy(Pool *pool)
{
    if (!pool) return;

    for (int i = 0; i < pool->max; i++) {
        if (pool->entries[i].read_fd != -1) {
            close(pool->entries[i].read_fd);
        }
    }

    free(pool->entries);
    free(pool);
}

int pool_active(const Pool *pool)
{
    return pool->active;
}

/* =========================================================================
 * Write_arg
 * ========================================================================= */

void write_arg(int fd, const void *buf, size_t count)
{
    size_t total = 0;
    const char *ptr = buf;

    while (total < count) {
        ssize_t written = write(fd, ptr + total, count - total);
        if (written <= 0) {
            perror("write");
            exit(1);
        }
        total += written;
    }
}

/* =========================================================================
 * Read_arg
 * ========================================================================= */

void read_arg(int fd, void *buf, size_t count)
{
    size_t total = 0;
    char *ptr = (char *)buf;

    while (total < count) {
        ssize_t r = read(fd, ptr + total, count - total);
        if (r <= 0) {
            perror("read");
            exit(1);
        }
        total += r;
    }
}

/* =========================================================================
 * worker_main
 * ========================================================================= */

void worker_main(const RenderParams *params, const Tile *tile, int write_fd)
{
    int n_pixels = tile->w * tile->h;
    unsigned char *buf = malloc(n_pixels);
    if (!buf) { 
        perror("malloc"); 
        exit(1); 
    }

    compute_tile(params, tile, buf);

    /* Envia metadados da Tile */
    write_arg(write_fd, &tile->ox, sizeof(int));
    write_arg(write_fd, &tile->oy, sizeof(int));
    write_arg(write_fd, &tile->w,  sizeof(int));
    write_arg(write_fd, &tile->h,  sizeof(int));

    /* Envia os pixels calculados */
    write_arg(write_fd, buf, n_pixels);

    close(write_fd);
    free(buf);
    exit(0);
}

/* =========================================================================
 * launch_worker
 * ========================================================================= */

void launch_worker(Pool *pool, const RenderParams *params, const Tile *t)
{
    int fd[2];

    if (pool->active >= pool->max) return; // Poll Cheia

    if (pipe(fd) == -1) { 
        perror("pipe"); 
        return; 
    }

    pid_t pid = fork();
    if (pid < 0) { 
        perror("fork"); 
        close(fd[0]); 
        close(fd[1]); 
        return; 
    }

    if (pid == 0) {
        /* FILHO */
        close(fd[0]);
        worker_main(params, t, fd[1]);
    }

    /* PAI */
    close(fd[1]);

    int i = 0;
    while (i < pool->max && pool->entries[i].pid != -1) {
        i++;
    }

    if (i == pool->max) {
        close(fd[0]);
        waitpid(pid, NULL, 0);
        return;
    }

    pool->entries[i].pid     = pid;
    pool->entries[i].read_fd = fd[0];
    pool->active++;
}

/* =========================================================================
 * pool_collect_ready
 * ========================================================================= */

int pool_collect_ready(Pool *pool, TileResult *result)
{
    if (pool->active == 0) return 0;

    fd_set rfds;
    FD_ZERO(&rfds);

    int maxfd = -1;
    for (int i = 0; i < pool->max; i++) {
        if (pool->entries[i].pid != -1 && pool->entries[i].read_fd != -1) {
            FD_SET(pool->entries[i].read_fd, &rfds);
            if (pool->entries[i].read_fd > maxfd)
                maxfd = pool->entries[i].read_fd;
        }
    }

    struct timeval tv = {0, 0}; /* Non-blocking */
    int ready = select(maxfd + 1, &rfds, NULL, NULL, &tv);
    if (ready <= 0) return 0;

    for (int i = 0; i < pool->max; i++) {
        if (pool->entries[i].pid == -1 || pool->entries[i].read_fd == -1)
            continue;

        int fd = pool->entries[i].read_fd;
        if (!FD_ISSET(fd, &rfds))
            continue;

        /* Coleta metadados */
        int ox, oy, w, h;
        read_arg(fd, &ox, sizeof(int));
        read_arg(fd, &oy, sizeof(int));
        read_arg(fd, &w,  sizeof(int));
        read_arg(fd, &h,  sizeof(int));

        /* Coleta pixels */
        int n_pixels = w * h;
        unsigned char *pixels = malloc(n_pixels);
        if (!pixels) { 
            perror("malloc"); 
            exit(1); 
        }
        read_arg(fd, pixels, n_pixels);

        /* Preenche o resultado para o chamador */
        result->tile.ox = ox;
        result->tile.oy = oy;
        result->tile.w  = w;
        result->tile.h  = h;
        result->pixels  = pixels;

        /* Cleanup local da entrada */
        close(fd);
        pool->entries[i].read_fd = -1;

        return 1;
    }

    return 0;
}

/* =========================================================================
 * pool_reap
 * ========================================================================= */

void pool_reap(Pool *pool)
{
    int status;
    pid_t pid;

    /* Coleta todos os processos filhos que terminaram (sem bloquear) */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < pool->max; i++) {
            if (pool->entries[i].pid == pid) {
                if (pool->entries[i].read_fd != -1) {
                    close(pool->entries[i].read_fd);
                    pool->entries[i].read_fd = -1;
                }
                pool->entries[i].pid = -1;
                pool->active--;
                break;
            }
        }
    }
}