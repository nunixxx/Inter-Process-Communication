[cite_start]Aqui está uma proposta de arquivo `README.md` estruturada com base nas especificações do trabalho [cite: 3] e na implementação fornecida no código `ipc.c`.

---

# Mandelbrot Tiles — Comunicação entre Processos (IPC)

[cite_start]Este projeto implementa uma renderização paralela e progressiva do conjunto de Mandelbrot utilizando a linguagem C e a biblioteca gráfica RayLib[cite: 4, 116]. [cite_start]O sistema explora conceitos avançados de Sistemas Operacionais, como criação de processos com `fork()`, comunicação via pipes anônimos e multiplexação de entrada/saída com `select()`[cite: 3, 11].

## 📝 Descrição do Problema

[cite_start]O programa renderiza o fractal de Mandelbrot dividindo o canvas (janela gráfica) em pequenos blocos quadrados chamados **tiles**[cite: 5, 20]. 
* [cite_start]**Processo Pai:** Atua como coordenador, dividindo o trabalho e gerenciando um pool de processos filhos[cite: 5]. [cite_start]Ele desenha os tiles na tela assim que os dados chegam pelos pipes[cite: 6].
* [cite_start]**Processos Filhos:** Cada filho é responsável por calcular o fractal para um tile específico e enviar os resultados (metadados e pixels) de volta ao pai[cite: 5, 50].

## 🚀 Funcionalidades Implementadas

A lógica principal reside no arquivo `ipc.c` e abrange:

* [cite_start]**Gerenciamento de Pool:** Controle dinâmico de um número máximo de processos filhos ativos simultaneamente[cite: 7].
* [cite_start]**Comunicação Assíncrona:** Uso de pipes para o envio de dados do filho para o pai[cite: 9].
* [cite_start]**Renderização Não-Bloqueante:** O pai utiliza `select()` com timeout zero para verificar a disponibilidade de dados nos pipes sem interromper a fluidez da interface gráfica[cite: 11, 80].
* [cite_start]**Coleta de Zumbis:** Uso de `waitpid()` com a flag `WNOHANG` para limpar processos encerrados sem bloquear a execução principal[cite: 10, 88].
* [cite_start]**Robustez de I/O:** Funções `read_arg` e `write_arg` que garantem a transferência completa de bytes, tratando leituras e escritas parciais[cite: 76, 148].

## 🛠️ Tecnologias Utilizadas

* [cite_start]**Linguagem:** C (C99/C11)[cite: 116].
* [cite_start]**Interface Gráfica:** RayLib[cite: 25, 116].
* [cite_start]**SO:** Linux (chamadas de sistema POSIX)[cite: 116].

## 🔧 Compilação e Execução

[cite_start]O projeto inclui um `Makefile` para facilitar o gerenciamento[cite: 28].

1.  **Compilar:**
    ```bash
    make
    ```
2.  **Executar (Padrão 800x600):**
    ```bash
    make run
    ```
3.  **Executar com dimensões customizadas:**
    ```bash
    ./binario/mandelbrot [largura] [altura]
    ```

## 🎮 Controles Interativos

| Tecla | Ação |
| :--- | :--- |
| **WASD** | [cite_start]Mover a câmera[cite: 105]. |
| **R / T** | [cite_start]Diminuir / Aumentar a granularidade do tile[cite: 95]. |
| **F / G** | [cite_start]Ajustar a profundidade de iterações (qualidade)[cite: 97]. |
| **V / B** | [cite_start]Diminuir / Aumentar o limite de processos filhos[cite: 99]. |
| **Espaço** | [cite_start]Ciclar paletas de cores[cite: 103]. |
| **Ctrl+Z** | [cite_start]Desfazer último zoom (histórico de 64 passos)[cite: 101, 107]. |

## ⚠️ Detalhes Técnicos de IPC

Para evitar travamentos (deadlocks) ou processos órfãos:
* [cite_start]O pai fecha imediatamente o descritor de escrita (`fd[1]`) após o `fork()`[cite: 56, 145].
* [cite_start]O filho fecha o descritor de leitura (`fd[0]`) e utiliza apenas o de escrita para enviar os dados[cite: 54, 145].
* [cite_start]O protocolo de envio consiste em 4 inteiros (`ox`, `oy`, `w`, `h`) seguidos pelo buffer de pixels (`w * h` bytes)[cite: 60, 75].

---
[cite_start]**Trabalho Prático de Sistemas Operacionais — 2026/1** [cite: 1]

[cite_start]Seria útil se eu gerasse também o modelo do relatório em Markdown exigido no item 6 dos entregáveis? [cite: 124]