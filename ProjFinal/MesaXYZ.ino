/**
 * @file MesaXYZ - Desenho de Quadrados
 * @brief Controlador para mesa CNC com 3 eixos (X, Y, Z)
 * @author Arthur, Gustavo Nascimento, Carol, João Max Germano Lima
 * 
 * Modificações:
 * - Inicializa posição em 0,0,0
 * - Salva última posição em EEPROM após movimentação
 * - Interrupção do eixo Z com fim de curso no pino 13
 */

#include <EEPROM.h>

// ===== CONFIGURAÇÃO DE HARDWARE =====
// Definições dos pinos e parâmetros do motor
int motor = 16;           // Pino de habilitação do motor
#define dt 4              // Tempo de delay entre passos (ms)
#define p1 1              // Padrão de passo 1
#define p2 4              // Padrão de passo 2
#define p3 2              // Padrão de passo 3
#define p4 8              // Padrão de passo 4
#define EM 16             // Máscara para habilitação do motor

// Pinagem dos sensores de limite
const int limitX = 1;     // Pino do sensor de limite do eixo X (A5)
const int limitY = 7;     // Pino do sensor de limite do eixo Y (PD7)
const int limitZ = 13;    // Pino do sensor de limite do eixo Z (PB5)

// ===== VARIÁVEIS GLOBAIS =====
int Mx, My, Mz;          // Estados atuais dos motores X, Y e Z
float stepsPerCm = 1000.0 / 8;  // Conversão cm->passos (1000 passos = 8 cm)

// Posição atual em passos (inicializada em 0,0,0)
long currentPosition[3] = {0, 0, 0}; // X, Y, Z

// Estados do sistema
bool shouldStop = false;  // Flag para parada solicitada
bool penDown = false;     // Estado da caneta (true = abaixada)
bool emergencyActive = false; // Flag para emergência ativa

// Endereços EEPROM para salvar posições
const int EEPROM_X_ADDR = 0;
const int EEPROM_Y_ADDR = 4;
const int EEPROM_Z_ADDR = 8;

// ===== DECLARAÇÕES DE FUNÇÕES =====
void moveAxis(int steps, int limitPin, void (*setPort)(int), int axisIndex, bool checkStop = true);
void moveX(int steps);
void moveY(int steps);
void moveZ(int steps, bool checkStop = true);
void checkForStopCommand();
void emergencyStop();
void drawSquare(float sideLength);
void savePositionToEEPROM();
void loadPositionFromEEPROM();
void homeZ();

// ===== CONFIGURAÇÃO INICIAL =====
void setup() {
  // Configuração dos pinos do eixo X (PORTC)
  pinMode(14, OUTPUT);       // A0 - PC0
  pinMode(15, OUTPUT);       // A1 - PC1
  pinMode(16, OUTPUT);       // A2 - PC2
  pinMode(17, OUTPUT);       // A3 - PC3
  pinMode(18, OUTPUT);       // A4 - PC4
  pinMode(limitX, INPUT_PULLUP); // A5 - PC5 (sensor de limite)

  Mx = EM | p1;
  PORTC = Mx;

  // Configuração dos pinos do eixo Y (PORTD)
  pinMode(2, OUTPUT);        // PD2
  pinMode(3, OUTPUT);        // PD3
  pinMode(4, OUTPUT);        // PD4
  pinMode(5, OUTPUT);        // PD5
  pinMode(6, OUTPUT);        // PD6
  pinMode(limitY, INPUT_PULLUP); // PD7 (sensor de limite)

  My = EM | p1;
  PORTD = My << 2;
  
  // Configuração dos pinos do eixo Z (PORTB)
  pinMode(8, OUTPUT);        // PB0
  pinMode(9, OUTPUT);        // PB1
  pinMode(10, OUTPUT);       // PB2
  pinMode(11, OUTPUT);       // PB3
  pinMode(12, OUTPUT);       // PB4
  pinMode(limitZ, INPUT_PULLUP); // PB5 (sensor de limite)

  Mz = EM | p1;
  PORTB = Mz;
  
  // Inicialização da comunicação serial
  Serial.begin(9600);
  
  // Carrega a última posição da EEPROM
  loadPositionFromEEPROM();
  
  Serial.println("Sistema Mesa XYZ Iniciado");
  Serial.print("Posição inicial: X=");
  Serial.print(currentPosition[0]/stepsPerCm);
  Serial.print("cm, Y=");
  Serial.print(currentPosition[1]/stepsPerCm);
  Serial.print("cm, Z=");
  Serial.print(currentPosition[2]/stepsPerCm);
  Serial.println("cm");
  
  Serial.println("Comandos disponiveis:");
  Serial.println("* Movimento manual: [x/y/z] [distancia_cm]");
  Serial.println("   Ex: x 5.0 (move X +5cm)");
  Serial.println("* Desenhar quadrado: q [lado_cm]");
  Serial.println("   Ex: q 10.0 (desenha quadrado 10x10cm)");
  Serial.println("* Emergencia: p (para tudo imediatamente)");
}

// ===== LOOP PRINCIPAL =====
void loop() {
  if (Serial.available() > 0) {
    char command = Serial.read();
    
    // Comando de emergência (prioridade máxima)
    if (command == 'p' || command == 'P') {
      emergencyStop();
      while(Serial.available()) Serial.read(); // Limpa buffer
      return;
    }
    
    // Comando para desenhar quadrado
    if (command == 'q' || command == 'Q') {
      Serial.read(); // Lê o espaço
      float sideLength = Serial.parseFloat();
      while(Serial.available()) Serial.read(); // Limpa buffer
      
      if(sideLength > 0) {
        drawSquare(sideLength);
      } else {
        Serial.println("Erro: Lado de quadrado deve ser positivo!");
      }
    } 
    // Comandos de movimento manual
    else if(command == 'x' || command == 'X' || 
            command == 'y' || command == 'Y' || 
            command == 'z' || command == 'Z') {
      char axis = tolower(command);
      Serial.read(); // Lê espaço
      float distance = Serial.parseFloat();
      while(Serial.available()) Serial.read();
      
      if(distance != 0) {
        int steps = round(distance * stepsPerCm);
        
        Serial.print("Movendo eixo ");
        Serial.print(axis);
        Serial.print(" em ");
        Serial.print(distance);
        Serial.println(" cm");
        
        shouldStop = false;
        
        switch(axis) {
          case 'x': moveX(steps); break;
          case 'y': moveY(steps); break;
          case 'z': moveZ(steps); break;
        }
        
        // Salva posição após movimento
        savePositionToEEPROM();
      }
    }
    else {
      Serial.println("Comando invalido! Digite 'x', 'y', 'z', 'q' ou 'p' de acordo com as instruções");
      while(Serial.available()) Serial.read();
    }
  }
}

// ===== FUNÇÕES DE MOVIMENTO =====

/**
 * Move um eixo por uma quantidade específica de passos
 * @param steps Número de passos (negativo para direção oposta)
 * @param limitPin Pino do sensor de limite
 * @param setPort Função para configurar os pinos do motor
 * @param axisIndex Índice do eixo (0=X, 1=Y, 2=Z)
 * @param checkStop Se verdadeiro, verifica por comandos de parada
 */
void moveAxis(int steps, int limitPin, void (*setPort)(int), int axisIndex, bool checkStop) {
  int limitState = digitalRead(limitPin);
  int direction = steps > 0 ? 1 : -1;
  steps = abs(steps);
  
  // Padrões de acionamento do motor (sequência de meio-passo)
  const int patterns[4] = {17, 20, 18, 24};
  
  for(int i = 0; i < steps && (!checkStop || !shouldStop); i++) {
    if(!emergencyActive) {
      checkForStopCommand();
    }
    
    // // Verificação específica para o eixo Z com fim de curso
    // if(axisIndex == 2 && digitalRead(limitZ) == LOW) {
    //   Serial.println("Limite do eixo Z atingido!");
    //   shouldStop = true;
    //   break;
    // }
    
    // Executa a sequência de passos na direção especificada
    if(direction > 0) {
      for(int j = 0; j < 4; j++) {
        setPort(patterns[j]);
        delay(dt);
      }
    } else {
      for(int j = 3; j >= 0; j--) {
        setPort(patterns[j]);
        delay(dt);
      }
    }
    
    // Atualiza posição atual
    currentPosition[axisIndex] += direction;
  }
}

// Funções específicas por eixo
void moveX(int steps) {
  moveAxis(steps, limitX, [] (int pattern) { PORTC = pattern; }, 0);
}

void moveY(int steps) {
  moveAxis(steps, limitY, [] (int pattern) { PORTD = pattern << 2; }, 1);
}

void moveZ(int steps, bool checkStop) {
  moveAxis(steps, limitZ, [] (int pattern) { PORTB = pattern; }, 2, checkStop);
}

// ===== FUNÇÕES DE EEPROM =====

void savePositionToEEPROM() {
  // Salva as posições atuais na EEPROM
  EEPROM.put(EEPROM_X_ADDR, currentPosition[0]);
  EEPROM.put(EEPROM_Y_ADDR, currentPosition[1]);
  EEPROM.put(EEPROM_Z_ADDR, currentPosition[2]);
  
  Serial.println("Posição salva na EEPROM");
}

void loadPositionFromEEPROM() {
  // Carrega as posições da EEPROM
  EEPROM.get(EEPROM_X_ADDR, currentPosition[0]);
  EEPROM.get(EEPROM_Y_ADDR, currentPosition[1]);
  EEPROM.get(EEPROM_Z_ADDR, currentPosition[2]);
  
  // Verifica se os valores são válidos (primeira execução pode ter lixo)
  if(isnan(currentPosition[0]) || isnan(currentPosition[1]) || isnan(currentPosition[2])) {
    currentPosition[0] = currentPosition[1] = currentPosition[2] = 0;
  }
}

// ===== FUNÇÕES DE CONTROLE =====

/**
 * Verifica se foi recebido comando de parada durante operações longas
 */
void checkForStopCommand() {
  if(Serial.available() > 0) {
    char c = Serial.read();
    if(c == 'p' || c == 'P') {
      emergencyStop();
    }
    while(Serial.available()) Serial.read();
  }
}

/**
 * Executa parada de emergência:
 * 1. Levanta a caneta se estiver abaixada
 * 2. Interrompe todos os movimentos
 * 3. Reseta flags de controle
 */
void emergencyStop() {
  if(emergencyActive) return;
  
  emergencyActive = true;
  shouldStop = true;
  Serial.println("\n--- PARADA DE EMERGENCIA ATIVADA ---");
  
  if(penDown) {
    Serial.println("Retornando caneta para posição segura...");
    moveZ(-round(10 * stepsPerCm), false); // Move sem verificar stop
    penDown = false;
    Serial.println("Caneta segura");
  }
  
  // Salva posição atual
  savePositionToEEPROM();
  
  Serial.println("Sistema pronto para novos comandos");
  emergencyActive = false;
}

// ===== FUNÇÕES DE DESENHO =====

/**
 * Desenha um quadrado com o tamanho especificado
 * @param sideLength Comprimento do lado em centímetros
 */
void drawSquare(float sideLength) {
  int distancePenToBoard = round(10 * stepsPerCm); // Distância para abaixar caneta
  int steps = round(sideLength * stepsPerCm);       // Passos por lado
  
  shouldStop = false;
  emergencyActive = false;
  
  Serial.println("\nIniciando desenho do quadrado...");
  
  // Abaixa caneta
  Serial.println("Posicionando caneta...");
  moveZ(distancePenToBoard);
  penDown = true;
  if(shouldStop) {
    emergencyStop();
    return;
  }

  // Sequência de movimentos para os 4 lados
  struct {
    const char* name;
    void (*move)(int);
    int steps;
  } sides[4] = {
    {"Lado 1: X+", moveX, steps},
    {"Lado 2: Y+", moveY, steps},
    {"Lado 3: X-", moveX, -steps},
    {"Lado 4: Y-", moveY, -steps}
  };

  // Executa cada lado
  for(int i = 0; i < 4 && !shouldStop; i++) {
    Serial.println(sides[i].name);
    sides[i].move(sides[i].steps);
    
    if(shouldStop) {
      emergencyStop();
      return;
    }
  }
  
  // Levanta caneta ao finalizar
  Serial.println("Retornando caneta...");
  moveZ(-distancePenToBoard);
  penDown = false;
  
  // Salva posição final
  savePositionToEEPROM();
  
  Serial.println("Quadrado completo!\n");
}