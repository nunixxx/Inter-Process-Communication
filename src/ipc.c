/*
 * ipc.c — Módulo de IPC: fork() + pipes
 *
 * INF01142 — Sistemas Operacionais — 2026/1
 *
 * =========================================================================
 * ESTE É O ÚNICO ARQUIVO QUE DEVE SER MODIFICADO.
 * =========================================================================
 *
 * Implemente todas as funções declaradas em ipc.h.
 * Consulte os comentários em ipc.h para o contrato de cada função.
 *
 * Chamadas de sistema que você precisará:
 *   pipe(2), fork(2), read(2), write(2), close(2),
 *   waitpid(2), select(2) ou poll(2), exit(3), malloc(3), free(3)
 *
 * Referências rápidas:
 *   man 2 pipe      man 2 fork     man 2 waitpid
 *   man 2 select    man 2 read     man 2 write
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
 * Definição interna do Pool
 *
 * Sugestão de estrutura — pode-se alterar caso necessário.
 * ========================================================================= */
typedef struct {
    pid_t pid;      /* PID do filho, ou -1 se a entrada está livre */
    int   read_fd;  /* descritor de leitura do pipe de retorno      */
} PoolEntry;

struct Pool {
    int        max;     /* capacidade máxima do pool  */
    int        active;  /* número de filhos ativos    */
    PoolEntry *entries; /* array de entradas          */
};

/* =========================================================================
 * pool_create
 * ========================================================================= */
Pool *pool_create(int max_children)
{
    Pool *pool = malloc(sizeof(Pool));
    if (!pool) return NULL;

    pool->max     = max_children;
    pool->active  = 0;
    pool->entries = malloc(sizeof(PoolEntry) * max_children);
    if (!pool->entries) { free(pool); return NULL; }

    for (int i = 0; i < max_children; i++) {
        pool->entries[i].pid     = -1;
        pool->entries[i].read_fd = -1;
    }
    return pool;
}

/* =========================================================================
 * pool_destroy
 * ========================================================================= */
void pool_destroy(Pool *pool)
{
    if (!pool) return;
    for (int i = 0; i < pool->max; i++) {
        if (pool->entries[i].read_fd != -1)
            close(pool->entries[i].read_fd);
    }
    free(pool->entries);
    free(pool);
}

/* =========================================================================
 * pool_active
 * ========================================================================= */
int pool_active(const Pool *pool)
{
    return pool->active;
}

/* =========================================================================
 * launch_worker
 *
 * TODO: implemente esta função.
 * ========================================================================= */
void launch_worker(Pool *pool, const RenderParams *params, const Tile *t)
{
    (void)pool;
    (void)params;
    (void)t;

    int fd[2]; 

    if(pool->active >= pool->max) return;
    
    if (pipe(fd) == -1) { perror("pipe"); return; }
    pid_t pid = fork();

    if(pid < 0) { perror("fork"); close(fd[0]); close(fd[1]); return; }

    if(pid == 0) { //FILHO
        close(fd[0]);
            worker_main(params, t, fd[1]);
        }

        close(fd[1]);
        int i = 0;
        while(pool->entries[i].pid != -1 && i < pool->max) i++;

        PoolEntry entry;
        entry.pid = pid;
        entry.read_fd = fd[0];

        pool->entries[i] = entry;
        pool->active++;
        
    /* Dica de estrutura:
     *
     * int fd[2];
     * if (pipe(fd) == -1) { perror("pipe"); return; }
     *
     * pid_t pid = fork();
     * if (pid < 0) { perror("fork"); close(fd[0]); close(fd[1]); return; }
     *
     * if (pid == 0) {
     *     // filho
     *     close(fd[0]); LEITURA
     *     worker_main(params, t, fd[1]);
     *     // worker_main não retorna
     * }
     *
     * // pai
     * close(fd[1]);
     * // registrar pid e fd[0] em uma entrada livre do pool
     * // incrementar pool->active
     */
}

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
 * worker_main
 *
 * TODO: implemente esta função.
 * ========================================================================= */
void worker_main(const RenderParams *params, const Tile *tile, int write_fd)
{
    (void)params;
    (void)tile;
    (void)write_fd;

    int n_pixels = tile->w * tile->h;
    unsigned char *buf = malloc(n_pixels);
    if (!buf) perror("malloc"); exit(1);

    compute_tile(params, tile, buf);

    write_arg(write_fd, &tile->ox, sizeof(int));
    write_arg(write_fd, &tile->oy, sizeof(int));
    write_arg(write_fd, &tile->w, sizeof(int));   
    write_arg(write_fd, &tile->h, sizeof(int));

    write_arg(write_fd, buf, n_pixels);

    close(write_fd);
    free(buf);
    exit(0);

    /* Dica de estrutura:
     *
     * int n_pixels = tile->w * tile->h;
     * unsigned char *buf = malloc(n_pixels);
     * if (!buf) { perror("malloc"); exit(1); }
     *
     * compute_tile(params, tile, buf);
     *
     * // Escrever cabeçalho: ox, oy, w, h
     * // Escrever pixels
     * // Lembre: use um loop para garantir que todos os bytes foram escritos!
     *
     * close(write_fd);
     * free(buf);
     * exit(0);
     */
}

void rite_arg(int fd, const void *buf, size_t count)
{
    size_t total = 0;
    const char *ptr = buf;

    while (total < count) {
        ssize_t read = read(fd, ptr + total, count - total);
        if (read <= 0) {
            perror("read");
            exit(1);
        }
        total += read;
    }
}

/* =========================================================================
 * pool_collect_ready
 *
 * TODO: implemente esta função.
 * ========================================================================= */
int pool_collect_ready(Pool *pool, TileResult *result)
{

    //checa se a filhos ativos
    if (pool->active == 0) return 0;

     fd_set rfds;
     FD_ZERO(&rfds); //Limpa end de MEM

     int maxfd = -1;

     //Cria conjuntod de FDs para o select
     for (int i = 0; i < pool->max; i++) {
         if (pool->entries[i].pid != -1) {
             FD_SET(pool->entries[i].read_fd,  &rfds);
             if (pool->entries[i].read_fd > maxfd)
                 maxfd = pool->entries[i].read_fd;
         }
     }
    
     //select
     struct timeval tv = {0, 0}; // timeout zero = não bloqueia
     int ready = select(maxfd + 1, &rfds, NULL, NULL, &tv);
     if (ready <= 0) return 0;

     //Procura dados prontos
     for(int i = 0; i < pool->max; i++) 
     {
        if(pool->entries[i].pid == -1) continue; //processo vazio

        int fd = pool->entries[i].read_fd;

        if(!IF_ISSET(fd, &rfds)) continue; //verifica estado do FD

        int r = read(pool->entries[i]->read_fd, &(result->tile.ox), sizeof(int));
        
        if(r <= 0) continue;

        int ox, oy, w, h;

        read_arg(fd, &ox, sizeof(int));
        read_arg(fd, &oy, sizeof(int));
        read_arg(fd, &w, sizeof(int));
        read_arg(fd, &h, sizeof(int));

        int n_pixels = w * h;
        unsigned char *pixels = malloc(n_pixels);
        if (!pixels) perror("malloc"); exit(1);

        read_arg(fd, pixels, n_pixels);

        result->tile.ox = ox;
        result->tile.oy = oy;
        result->tile.w = w;
        result->tile.h = h;
        result->pixels = pixels;

        // Finaliza worker (cleanup)
        close(fd);
        waitpid(pool->entries[i].pid, NULL, 0);

        // Retorna um resultado coletado
        return 1;
     }

    /* Dica de estrutura com select():
     *
     * fd_set rfds;
     * FD_ZERO(&rfds);
     * int maxfd = -1;
     * for (int i = 0; i < pool->max; i++) {
     *     if (pool->entries[i].pid != -1) {
     *         FD_SET(pool->entries[i].read_fd, &rfds);
     *         if (pool->entries[i].read_fd > maxfd)
     *             maxfd = pool->entries[i].read_fd;
     *     }
     * }
     *
     * struct timeval tv = {0, 0}; // timeout zero = não bloqueia
     * int ready = select(maxfd + 1, &rfds, NULL, NULL, &tv);
     * if (ready <= 0) return 0;
     *
     * // Para cada entrada com dados:
     * //   ler cabeçalho (4 ints: ox, oy, w, h)
     * //   alocar result->pixels com malloc(w * h)
     * //   ler w*h bytes (em loop)
     * //   preencher result->tile
     * //   retornar 1
     */

    return 0;
}

/* =========================================================================
 * pool_reap
 *
 * TODO: implemente esta função.
 * ========================================================================= */
void pool_reap(Pool *pool)
{
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) 
    {
        for(int i = 0; i < pool->max; i++)
        {
           if(pool->entries[i].pid == pid)
           {
                if(pool->entries[i].read_fd != -1) 
                     close(pool->entries[i].read_fd);

                // Marca slot como livre
                pool->entries[i].pid = -1;
                pool->active--;
                
                break;
           }
        }
    }
    /* Dica de estrutura:
     *
     * int status;
     * pid_t pid;
     * while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
     *     // encontrar a entrada com esse pid no pool
     *     // fechar o read_fd correspondente
     *     // marcar a entrada como livre (pid = -1, read_fd = -1)
     *     // decrementar pool->active
     * }
     */
}
