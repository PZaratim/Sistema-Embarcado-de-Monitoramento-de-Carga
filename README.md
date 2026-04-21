## Sistema Embarcado de Controle de Carga com Máquina de Estados
# Descrição

Este projeto implementa um sistema embarcado em C para ESP32 com foco em controle e monitoramento de carga elétrica. O sistema utiliza uma máquina de estados finitos para gerenciar o comportamento geral da aplicação e controlar dispositivos com base em prioridades.
O sistema avalia continuamente uma variável de carga (load_total, de 0 a 100) e define o estado operacional global (STATE), além de controlar três dispositivos (A, B e C) com diferentes níveis de prioridade.

# Funcionamento

O STATE do sistema é definido com base na carga total:
NORMAL: operação estável com baixa carga
ATTENTION: carga intermediária, exigindo otimização
CRITICAL: alta carga, exigindo redução de consumo

Os dispositivos seguem uma hierarquia de prioridade fixa:

Device A: crítico (maior prioridade, permanece ativo sempre que possível)
Device B: intermediário
Device C: não crítico (primeiro a ser desligado)

# Saídas do sistema
Controle dos dispositivos A, B e C (ligado/desligado)
LEDs de status indicando o estado operacional
Display LCD 16x2 exibindo STATE e nível de carga
Botão físico para ligar e desligar o sistema

# Lógica de controle
Em condição NORMAL, todos os dispositivos podem operar
Em ATTENTION, o sistema reduz consumo desligando dispositivos não críticos
Em CRITICAL, apenas dispositivos de maior prioridade permanecem ativos
O sistema realiza transições graduais para evitar oscilações bruscas

# Objetivo

O projeto tem como objetivo aplicar conceitos de sistemas embarcados, automação e controle digital, incluindo:
- Máquina de estados finitos
- Gerenciamento de prioridade de carga
- Controle de entradas e saídas digitais
- Simulação de comportamento de sistemas industriais

# Tecnologias utilizadas
- Linguagem C
- ESP32
- GPIO (entrada e saída digital)
- LCD 16x2
- Simulação de lógica embarcada em tempo real
